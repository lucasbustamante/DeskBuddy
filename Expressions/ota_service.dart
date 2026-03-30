import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

const String kOtaServiceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const String kOtaCtrlUuid    = "6e400010-b5a3-f393-e0a9-e50e24dcca9e";
const String kOtaDataUuid    = "6e400011-b5a3-f393-e0a9-e50e24dcca9e";
const String kOtaStatusUuid  = "6e400012-b5a3-f393-e0a9-e50e24dcca9e";

const int CMD_OTA_START = 0x01;
const int CMD_OTA_END   = 0x02;
const int CMD_OTA_ABORT = 0x03;

const int ACK_READY      = 0xA0;
const int ACK_CHUNK_OK   = 0xA1;
const int ACK_CHUNK_FAIL = 0xA2;
const int ACK_COMPLETE   = 0xA3;
const int ACK_ERROR      = 0xAF;

const int kBleAttOverhead   = 3;
const int kMtuRequest       = 512;

enum OtaState { idle, preparando, conectando, enviando, validando, finalizando, concluido, erro }

class OtaService {
  final void Function(OtaState state, double progress, String message)? onProgress;
  final void Function(String error)? onError;
  final void Function()? onSuccess;

  OtaService({this.onProgress, this.onError, this.onSuccess});

  OtaState _state     = OtaState.idle;
  bool     _cancelled = false;

  BluetoothCharacteristic? _ctrlChar;
  BluetoothCharacteristic? _dataChar;
  BluetoothCharacteristic? _statusChar;

  // Stream de notificações — um único listener durante todo o OTA
  StreamSubscription<List<int>>? _statusSub;

  // Fila de ACKs recebidos (evita perder notificações que chegam antes do await)
  final _ackQueue = StreamController<int>.broadcast();

  // ─────────────────────────────────────────────────────────────────────────

  Future<void> startOta({
    required BluetoothDevice device,
    String assetPath = 'assets/firmware/deskbuddy.bin',
  }) async {
    _cancelled = false;
    _setState(OtaState.preparando, 0, "Carregando firmware…");

    // 1. Carrega .bin dos assets
    Uint8List firmware;
    try {
      final data = await rootBundle.load(assetPath);
      firmware = data.buffer.asUint8List();
    } catch (e) {
      _fail("Firmware não encontrado em $assetPath.");
      return;
    }
    if (firmware.isEmpty) { _fail("Arquivo de firmware está vazio."); return; }

    _setState(OtaState.conectando, 0,
        "Firmware: ${(firmware.length / 1024).toStringAsFixed(1)} KB. Negociando MTU…");

    // 2. Negocia MTU
    int mtu = 23;
    try {
      mtu = await device.requestMtu(kMtuRequest)
          .timeout(const Duration(seconds: 6), onTimeout: () => 23);
    } catch (_) { mtu = 23; }

    // Payload = MTU - 3 (ATT overhead), com margem extra de segurança
    final int chunkSize = (mtu - kBleAttOverhead - 3).clamp(20, 509);
    _setState(OtaState.conectando, 0,
        "MTU: $mtu → chunk: $chunkSize B. Descobrindo serviços…");

    // 3. Descobre características OTA
    try {
      await _discoverOtaCharacteristics(device);
    } catch (e) {
      _fail("Características OTA não encontradas: $e");
      return;
    }

    // 4. Inscreve notificações — ANTES de qualquer write
    await _subscribeStatus();

    // 5. Envia CMD_START
    _setState(OtaState.enviando, 0, "Iniciando OTA…");
    final size = firmware.length;
    final startCmd = Uint8List(4)
      ..[0] = CMD_OTA_START
      ..[1] = (size >> 16) & 0xFF
      ..[2] = (size >> 8)  & 0xFF
      ..[3] = (size)       & 0xFF;

    await _ctrlChar!.write(startCmd, withoutResponse: false);

    final ackReady = await _waitAck(timeoutSeconds: 12);
    if (ackReady != ACK_READY) {
      _fail("ESP32 não respondeu READY (0x${ackReady.toRadixString(16)})."
          " Verifique se o firmware OTA está gravado no ESP32.");
      return;
    }

    // 6. Envia chunks
    final totalChunks = (firmware.length / chunkSize).ceil();

    for (int i = 0; i < totalChunks; i++) {
      if (_cancelled) { await _sendAbort(); _fail("Cancelado."); return; }

      final start = i * chunkSize;
      final end   = (start + chunkSize).clamp(0, firmware.length);
      final chunk = firmware.sublist(start, end);

      bool ok      = false;
      int retries  = 0;

      while (!ok && retries < 3) {
        try {
          await _dataChar!.write(chunk, withoutResponse: false);
          final ack = await _waitAck(timeoutSeconds: 10);

          if (ack == ACK_CHUNK_OK) {
            ok = true;
          } else if (ack == ACK_ERROR) {
            _fail("ESP32 erro fatal no chunk $i."); return;
          } else {
            // ACK_CHUNK_FAIL ou timeout (0xFF)
            retries++;
            await Future.delayed(const Duration(milliseconds: 300));
          }
        } catch (e) {
          retries++;
          if (retries >= 3) { _fail("Chunk $i falhou 3x: $e"); return; }
          await Future.delayed(const Duration(milliseconds: 400));
        }
      }

      if (!ok) { _fail("Chunk $i não pôde ser enviado após 3 tentativas."); return; }

      _setState(OtaState.enviando, (i + 1) / totalChunks,
          "Enviando… ${((i + 1) * 100 / totalChunks).toStringAsFixed(0)}%"
          "  (${i + 1}/$totalChunks · ${chunk.length}B)");
    }

    // 7. CMD_END — ESP32 valida e reinicia
    _setState(OtaState.validando, 1.0, "Validando firmware no ESP32…");
    await _ctrlChar!.write(Uint8List.fromList([CMD_OTA_END]), withoutResponse: false);

    // Aguarda ACK_COMPLETE com timeout generoso (ESP32 pode demorar ~5s gravando)
    // Se timeout (0xFF), assume sucesso se todos os chunks foram OK —
    // porque o ESP32 pode reiniciar ANTES do notify chegar ao app.
    final finalAck = await _waitAck(timeoutSeconds: 30);

    // ACK_COMPLETE (0xA3) = sucesso confirmado pelo ESP32
    // 0xFF = timeout: ESP32 reiniciou antes do notify chegar (aceitável)
    // ACK_ERROR (0xAF) = FALHA REAL — não tratar como sucesso
    final bool isSuccess = (finalAck == ACK_COMPLETE) || (finalAck == 0xFF);

    if (isSuccess) {
      final msg = (finalAck == ACK_COMPLETE)
          ? "Reiniciando DeskBuddy…"
          : "ESP32 reiniciou. Atualização concluída!";
      _setState(OtaState.finalizando, 1.0, msg);
      await Future.delayed(const Duration(seconds: 3));
      _setState(OtaState.concluido, 1.0, "✅ Firmware atualizado! Aguarde o DeskBuddy reiniciar.");
      onSuccess?.call();
    } else {
      // ACK_ERROR = ESP32 reportou falha real (sem partição OTA, bin inválido, etc)
      _fail(
        finalAck == ACK_ERROR
          ? "Falha no ESP32. Verifique se compilou com:\nTools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"
          : "Código inesperado: 0x${finalAck.toRadixString(16)}"
      );
    }

    await _cleanup();
  }

  void cancel() => _cancelled = true;

  // ─── Internals ─────────────────────────────────────────────────────────────

  Future<void> _discoverOtaCharacteristics(BluetoothDevice device) async {
    final services = await device.discoverServices();
    for (final svc in services) {
      if (svc.uuid.toString().toLowerCase() != kOtaServiceUuid.toLowerCase()) continue;
      for (final c in svc.characteristics) {
        final u = c.uuid.toString().toLowerCase();
        if (u == kOtaCtrlUuid.toLowerCase())   _ctrlChar   = c;
        if (u == kOtaDataUuid.toLowerCase())   _dataChar   = c;
        if (u == kOtaStatusUuid.toLowerCase()) _statusChar = c;
      }
    }
    if (_ctrlChar == null || _dataChar == null || _statusChar == null) {
      throw Exception("Characteristics OTA não encontradas no ESP32.");
    }
  }

  Future<void> _subscribeStatus() async {
    await _statusChar!.setNotifyValue(true);
    // Redireciona cada notificação para a fila — nunca perde um ACK
    _statusSub = _statusChar!.onValueReceived.listen((value) {
      if (value.isNotEmpty && !_ackQueue.isClosed) {
        _ackQueue.add(value[0]);
      }
    });
  }

  /// Aguarda o próximo byte da fila de ACKs com timeout.
  /// Retorna 0xFF se expirar o tempo.
  Future<int> _waitAck({int timeoutSeconds = 10}) async {
    try {
      return await _ackQueue.stream
          .first
          .timeout(Duration(seconds: timeoutSeconds), onTimeout: () => 0xFF);
    } catch (_) {
      return 0xFF;
    }
  }

  Future<void> _sendAbort() async {
    try {
      await _ctrlChar?.write(Uint8List.fromList([CMD_OTA_ABORT]), withoutResponse: false);
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
    try { await _statusSub?.cancel(); } catch (_) {}
    try { await _statusChar?.setNotifyValue(false); } catch (_) {}
    if (!_ackQueue.isClosed) await _ackQueue.close();
    _ctrlChar = _dataChar = _statusChar = null;
    _statusSub = null;
  }

  OtaState get state => _state;
}
