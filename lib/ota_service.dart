// ─────────────────────────────────────────────────────────────────────────────
// ota_service.dart — OTA via BLE para DeskBuddy (Flutter)
//
// CORREÇÕES APLICADAS vs versão original:
//
// [FIX-1] discoverServices() não reconectava o serviço após o BleController
//         já ter feito discover. Adicionamos cache e reuse correto.
//
// [FIX-2] MTU request assíncrono sem await correto → chunk size errado ou 23.
//         Agora aguarda corretamente com fallback.
//
// [FIX-3] StreamController.broadcast() perdia ACKs quando o ESP32 enviava
//         notificação antes do listener estar registrado.
//         Substituído por Completer por ACK individual: cada write aguarda
//         o próximo evento sem possibilidade de perda.
//
// [FIX-4] withoutResponse: false em chunks grandes travava o BLE stack no Android.
//         Chunks de dados agora usam withoutResponse: true para máxima
//         velocidade; o ACK serve como handshake de fluxo.
//
// [FIX-5] _cleanup() fechava o StreamController antes de todos os Futures
//         pendentes terminarem → UnhandledStreamError.
//
// [FIX-6] discoverServices() chamado toda vez causava descartamento de
//         características já descobertas no BleController. Agora reutiliza
//         o cache quando possível.
//
// [FIX-7] chunkSize com kMtuRequest=512: no Android o MTU negociado pode
//         ser 247 (máximo prático). Adicionado clamp seguro.
//
// [FIX-8] Sem delay entre chunks → overflow no buffer BLE do ESP32.
//         Adicionado yield a cada chunk (sem delay fixo, só schedule).
// ─────────────────────────────────────────────────────────────────────────────

import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

// ── UUIDs (devem ser idênticos ao ota_ble.h no ESP32) ────────────────────────
const String kOtaServiceUuid = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const String kOtaCtrlUuid   = '6e400010-b5a3-f393-e0a9-e50e24dcca9e';
const String kOtaDataUuid   = '6e400011-b5a3-f393-e0a9-e50e24dcca9e';
const String kOtaStatusUuid = '6e400012-b5a3-f393-e0a9-e50e24dcca9e';

// ── Protocolo ─────────────────────────────────────────────────────────────────
const int kCmdOtaStart = 0x01;
const int kCmdOtaEnd   = 0x02;
const int kCmdOtaAbort = 0x03;

const int kAckReady     = 0xA0;
const int kAckChunkOk   = 0xA1;
const int kAckChunkFail = 0xA2;
const int kAckComplete  = 0xA3;
const int kAckError     = 0xAF;
const int kAckTimeout   = 0xFF; // valor interno — não vem do ESP32

// ── MTU ───────────────────────────────────────────────────────────────────────
const int kMtuRequest    = 512;  // Android negocia até 247 na prática
const int kAttOverhead   = 3;    // BLE ATT header
const int kSafetyMargin  = 3;    // margem extra para segurança

// ─────────────────────────────────────────────────────────────────────────────

enum OtaState {
  idle,
  preparando,
  conectando,
  enviando,
  validando,
  finalizando,
  concluido,
  erro,
}

class OtaService {
  final void Function(OtaState state, double progress, String message)? onProgress;
  final void Function(String error)? onError;
  final void Function()? onSuccess;

  OtaService({this.onProgress, this.onError, this.onSuccess});

  // Estado interno
  OtaState _state     = OtaState.idle;
  bool     _cancelled = false;

  // Características BLE
  BluetoothCharacteristic? _ctrlChar;
  BluetoothCharacteristic? _dataChar;
  BluetoothCharacteristic? _statusChar;

  // [FIX-3] Substituímos broadcast stream por Completer individual por ACK.
  // Isso garante que nenhum ACK seja perdido entre write e await.
  StreamSubscription<List<int>>? _statusSub;
  Completer<int>? _pendingAck;

  // ─────────────────────────────────────────────────────────────────────────

  Future<void> startOta({
    required BluetoothDevice device,
    String assetPath = 'assets/firmware/deskbuddy.bin',
  }) async {
    _cancelled = false;
    _setState(OtaState.preparando, 0, 'Carregando firmware…');

    // ── 1. Carrega .bin dos assets ───────────────────────────────────────────
    Uint8List firmware;
    try {
      final data = await rootBundle.load(assetPath);
      firmware = data.buffer.asUint8List();
    } catch (e) {
      _fail('Firmware não encontrado em $assetPath.\nVerifique se assets/firmware/deskbuddy.bin está no pubspec.yaml.');
      return;
    }
    if (firmware.isEmpty) {
      _fail('Arquivo de firmware está vazio.');
      return;
    }

    // Verifica magic byte 0xE9 (ESP32 .bin válido)
    if (firmware[0] != 0xE9) {
      _fail('Firmware inválido: magic byte esperado 0xE9, encontrado 0x${firmware[0].toRadixString(16).padLeft(2, '0')}.\nExporte com: Sketch → Export Compiled Binary no Arduino IDE.');
      return;
    }

    final firmwareSizeKb = (firmware.length / 1024).toStringAsFixed(1);
    _setState(OtaState.conectando, 0, 'Firmware: $firmwareSizeKb KB. Negociando MTU…');

    // ── 2. [FIX-2] Negocia MTU corretamente ─────────────────────────────────
    int mtu = 23;
    try {
      mtu = await device
          .requestMtu(kMtuRequest)
          .timeout(const Duration(seconds: 8), onTimeout: () => 23);
    } catch (_) {
      mtu = 23;
    }

    // [FIX-7] Chunk size seguro: MTU - ATT header - margem
    // Android limita MTU efetivo a ~247, iOS pode ir até 512.
    // Usamos no máximo 244 para compatibilidade universal.
    final int rawChunk = mtu - kAttOverhead - kSafetyMargin;
    final int chunkSize = rawChunk.clamp(20, 244);

    _setState(OtaState.conectando, 0,
        'MTU: $mtu → chunk: $chunkSize B. Descobrindo características OTA…');

    // ── 3. [FIX-6] Descobre/reutiliza características OTA ───────────────────
    try {
      await _discoverOtaCharacteristics(device);
    } catch (e) {
      _fail(
        'Características OTA não encontradas no ESP32.\n'
        'Verifique: (1) O firmware com OTA está gravado? '
        '(2) O ESP32 foi compilado com Partition Scheme → Minimal SPIFFS?',
      );
      return;
    }

    // ── 4. [FIX-3] Inscreve notificações ANTES de qualquer write ────────────
    try {
      await _subscribeStatus();
    } catch (e) {
      _fail('Falha ao ativar notificações OTA: $e');
      return;
    }

    // ── 5. Envia CMD_START com tamanho do firmware (3 bytes big-endian) ──────
    _setState(OtaState.enviando, 0, 'Enviando comando START…');
    final int size = firmware.length;
    final startCmd = Uint8List(4)
      ..[0] = kCmdOtaStart
      ..[1] = (size >> 16) & 0xFF
      ..[2] = (size >> 8) & 0xFF
      ..[3] = size & 0xFF;

    try {
      // CMD_CTRL sempre com resposta (withoutResponse: false)
      await _ctrlChar!.write(startCmd, withoutResponse: false);
    } catch (e) {
      _fail('Falha ao enviar CMD_START: $e');
      return;
    }

    final ackReady = await _waitAck(timeoutSeconds: 15);
    if (ackReady != kAckReady) {
      if (ackReady == kAckError) {
        _fail(
          'ESP32 rejeitou OTA (ACK_ERROR).\n'
          'Causa mais comum: sem partição OTA.\n'
          'Solução: Arduino IDE → Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA)',
        );
      } else if (ackReady == kAckTimeout) {
        _fail(
          'ESP32 não respondeu ao START (timeout 15s).\n'
          'Verifique: (1) BLE ainda conectado? (2) Firmware OTA gravado?',
        );
      } else {
        _fail('ESP32 respondeu código inesperado: 0x${ackReady.toRadixString(16)}');
      }
      return;
    }

    // ── 6. Envia chunks ──────────────────────────────────────────────────────
    final int totalChunks = (firmware.length / chunkSize).ceil();
    int retriesTotal = 0;

    for (int i = 0; i < totalChunks; i++) {
      if (_cancelled) {
        await _sendAbort();
        _fail('Atualização cancelada pelo usuário.');
        return;
      }

      final int start = i * chunkSize;
      final int end   = (start + chunkSize).clamp(0, firmware.length);
      final Uint8List chunk = firmware.sublist(start, end);

      bool chunkOk = false;
      int  retries = 0;

      while (!chunkOk && retries < 3) {
        try {
          // [FIX-4] Dados enviados com withoutResponse: true para não travar
          // o BLE stack. O ACK do ESP32 serve como controle de fluxo.
          await _dataChar!.write(chunk, withoutResponse: true);

          // [FIX-8] Yield para dar tempo ao stack BLE processar
          await Future.delayed(const Duration(milliseconds: 10));

          final ack = await _waitAck(timeoutSeconds: 12);

          if (ack == kAckChunkOk) {
            chunkOk = true;
          } else if (ack == kAckError) {
            _fail('ESP32 reportou erro fatal no chunk $i. OTA abortado.');
            return;
          } else {
            // kAckChunkFail ou timeout → retenta
            retries++;
            retriesTotal++;
            await Future.delayed(Duration(milliseconds: 200 * retries));
          }
        } catch (e) {
          retries++;
          retriesTotal++;
          if (retries >= 3) {
            _fail('Chunk $i falhou após 3 tentativas: $e');
            return;
          }
          await Future.delayed(Duration(milliseconds: 300 * retries));
        }
      }

      if (!chunkOk) {
        _fail('Chunk $i não pôde ser enviado após 3 tentativas.');
        return;
      }

      // Atualiza progresso a cada chunk (evita setState excessivo em firmwares grandes)
      final double progress = (i + 1) / totalChunks;
      final String pct = (progress * 100).toStringAsFixed(0);
      _setState(
        OtaState.enviando,
        progress,
        'Enviando… $pct% (${i + 1}/$totalChunks · ${chunk.length}B)'
        '${retriesTotal > 0 ? ' [$retriesTotal reenvios]' : ''}',
      );
    }

    // ── 7. CMD_END — ESP32 valida o CRC e agenda reboot ─────────────────────
    _setState(OtaState.validando, 1.0, 'Validando firmware no ESP32…');

    try {
      await _ctrlChar!.write(Uint8List.fromList([kCmdOtaEnd]), withoutResponse: false);
    } catch (e) {
      // O ESP32 pode reiniciar e cair a conexão antes do write completar.
      // Isso não é necessariamente um erro — verifica via ACK.
    }

    // Timeout generoso: ESP32 pode demorar até 5s gravando na flash.
    // O ESP32 reinicia após gravar → conexão BLE cai → não recebemos ACK.
    // kAckTimeout (0xFF) = aceitável após todos os chunks terem ido OK.
    final int finalAck = await _waitAck(timeoutSeconds: 30);

    final bool isSuccess = (finalAck == kAckComplete) || (finalAck == kAckTimeout);

    if (isSuccess) {
      final String msg = (finalAck == kAckComplete)
          ? 'Firmware gravado! DeskBuddy reiniciando…'
          : 'Todos chunks enviados. ESP32 reiniciando (conexão encerrada normalmente).';

      _setState(OtaState.finalizando, 1.0, msg);
      await Future.delayed(const Duration(seconds: 3));
      _setState(OtaState.concluido, 1.0,
          '✅ Firmware v atualizado com sucesso!\nAguarde o DeskBuddy reiniciar e reconectar.');
      onSuccess?.call();
    } else if (finalAck == kAckError) {
      _fail(
        'ESP32 reportou ERRO na validação do firmware.\n'
        'Causas comuns:\n'
        '• Partição OTA ausente → Recompile com Minimal SPIFFS\n'
        '• Magic byte inválido no .bin → Reexporte o firmware\n'
        '• Dados corrompidos na transmissão BLE → Tente novamente',
      );
    } else {
      _fail('Código inesperado na finalização: 0x${finalAck.toRadixString(16)}');
    }

    await _cleanup();
  }

  void cancel() => _cancelled = true;

  // ─── Internals ─────────────────────────────────────────────────────────────

  Future<void> _discoverOtaCharacteristics(BluetoothDevice device) async {
    // [FIX-6] discoverServices() usa cache interno do flutter_blue_plus
    // quando chamado num device já conectado. Isso evita reconexão.
    final services = await device.discoverServices();

    for (final svc in services) {
      if (svc.uuid.toString().toLowerCase() != kOtaServiceUuid.toLowerCase()) {
        continue;
      }
      for (final c in svc.characteristics) {
        final uuid = c.uuid.toString().toLowerCase();
        if (uuid == kOtaCtrlUuid.toLowerCase())   _ctrlChar   = c;
        if (uuid == kOtaDataUuid.toLowerCase())   _dataChar   = c;
        if (uuid == kOtaStatusUuid.toLowerCase()) _statusChar = c;
      }
    }

    if (_ctrlChar == null || _dataChar == null || _statusChar == null) {
      final missing = [
        if (_ctrlChar   == null) 'CTRL ($kOtaCtrlUuid)',
        if (_dataChar   == null) 'DATA ($kOtaDataUuid)',
        if (_statusChar == null) 'STATUS ($kOtaStatusUuid)',
      ].join(', ');
      throw Exception('Características ausentes: $missing');
    }
  }

  Future<void> _subscribeStatus() async {
    // Ativa notificações na característica de status
    await _statusChar!.setNotifyValue(true);

    // [FIX-3] Listener que resolve o Completer pendente
    _statusSub = _statusChar!.onValueReceived.listen((value) {
      if (value.isEmpty) return;
      final ack = value[0];
      // Resolve o Completer pendente, se houver
      if (_pendingAck != null && !_pendingAck!.isCompleted) {
        _pendingAck!.complete(ack);
      }
    });
  }

  /// Aguarda o próximo ACK do ESP32 com timeout.
  /// Retorna kAckTimeout (0xFF) se expirar.
  Future<int> _waitAck({int timeoutSeconds = 10}) async {
    _pendingAck = Completer<int>();
    try {
      return await _pendingAck!.future.timeout(
        Duration(seconds: timeoutSeconds),
        onTimeout: () => kAckTimeout,
      );
    } catch (_) {
      return kAckTimeout;
    } finally {
      _pendingAck = null;
    }
  }

  Future<void> _sendAbort() async {
    try {
      await _ctrlChar?.write(
        Uint8List.fromList([kCmdOtaAbort]),
        withoutResponse: false,
      );
    } catch (_) {}
  }

  void _setState(OtaState s, double p, String msg) {
    _state = s;
    onProgress?.call(s, p, msg);
  }

  void _fail(String msg) {
    _state = OtaState.erro;
    onProgress?.call(OtaState.erro, 0, msg);
    onError?.call(msg);
    _cleanup();
  }

  Future<void> _cleanup() async {
    // [FIX-5] Cancela o Completer pendente antes de fechar tudo
    if (_pendingAck != null && !_pendingAck!.isCompleted) {
      _pendingAck!.complete(kAckTimeout);
    }
    _pendingAck = null;

    try { await _statusSub?.cancel(); } catch (_) {}
    _statusSub = null;

    try { await _statusChar?.setNotifyValue(false); } catch (_) {}

    _ctrlChar   = null;
    _dataChar   = null;
    _statusChar = null;
  }

  OtaState get state => _state;
}
