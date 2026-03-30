// ─────────────────────────────────────────────────────────────────────────────
// bleController.dart — Controlador BLE do DeskBuddy
//
// CORREÇÃO OTA [FIX-UI-1]:
// O polling periódico de JSON conflitava com a transmissão OTA pois ambos
// tentavam usar a mesma conexão BLE ao mesmo tempo.
// Adicionados métodos pausePolling() / resumePolling() para que a
// FirmwareUpdatePage possa suspender o polling durante o OTA.
// ─────────────────────────────────────────────────────────────────────────────

import 'dart:async';
import 'dart:convert';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';

class BleController {
  Map<String, dynamic> emocoes = {};
  String status = 'Pronto';
  bool scanning = false;
  bool running = false;

  // [FIX-UI-1] Flag de pausa do polling (durante OTA)
  bool _pollPaused = false;

  Timer? _pollTimer;
  Timer? _reconnectTimer;

  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _jsonChar;

  final String serviceUuid        = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
  final String characteristicUuid = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
  final String deviceNamePrefix   = 'DeskBuddy';

  String? nomeSalvo;
  String? senhaSalva;

  // ── OTA: expõe o device conectado e versão de firmware ─────────────────────
  BluetoothDevice? get connectedDevice => _device;

  String? get firmwareVersion {
    final v = emocoes['fw_version'];
    if (v == null) return null;
    return v.toString();
  }

  // ── [FIX-UI-1] Pause/Resume polling para OTA ─────────────────────────────
  /// Pausa o polling periódico do JSON.
  /// Chame antes de iniciar o OTA para evitar conflito na conexão BLE.
  void pausePolling() {
    _pollPaused = true;
  }

  /// Retoma o polling periódico do JSON.
  /// Chame após o OTA terminar (sucesso ou erro).
  void resumePolling() {
    _pollPaused = false;
  }

  Future<void> requestPermissions() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.locationWhenInUse.request();
    await Permission.bluetooth.request();
    await Permission.bluetoothAdvertise.request();
  }

  Future<void> _carregaCredenciaisSalvas() async {
    final prefs = await SharedPreferences.getInstance();
    nomeSalvo  = prefs.getString('deskbuddy_nome');
    senhaSalva = prefs.getString('deskbuddy_senha');
  }

  Future<void> start(Function onUpdate) async {
    running = true;
    await requestPermissions();
    await _carregaCredenciaisSalvas();

    await _ensureConnected(onUpdate);

    _pollTimer?.cancel();
    _pollTimer = Timer.periodic(const Duration(seconds: 2), (_) async {
      if (!running) return;
      // [FIX-UI-1] Pula a leitura se o polling estiver pausado (durante OTA)
      if (_pollPaused) return;
      await _readOnce(onUpdate);
    });
  }

  void stop() {
    running = false;
    _pollTimer?.cancel();
    _reconnectTimer?.cancel();
  }

  Future<void> dispose() async {
    stop();
    await _scanSub?.cancel();
    await _connSub?.cancel();
    await _disconnect();
  }

  // ─── Internals ─────────────────────────────────────────────────────────────

  Future<void> _ensureConnected(Function onUpdate) async {
    if (!running) return;

    if (_device != null) {
      final state = await _device!.connectionState.first;
      if (state == BluetoothConnectionState.connected && _jsonChar != null) {
        status = 'Conectado';
        onUpdate();
        return;
      }
    }

    await _disconnect();

    status = 'Buscando DeskBuddy...';
    scanning = true;
    onUpdate();

    try { await FlutterBluePlus.stopScan(); } catch (_) {}

    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) async {
      if (!running || _device != null) return;

      for (final r in results) {
        final advName = r.device.platformName;
        if (!advName.startsWith(deviceNamePrefix)) continue;

        if (nomeSalvo != null && nomeSalvo!.isNotEmpty) {
          if (!advName.toLowerCase().contains(nomeSalvo!.toLowerCase())) continue;
        }

        _device = r.device;
        scanning = false;
        status = 'Conectando...';
        onUpdate();

        try { await FlutterBluePlus.stopScan(); } catch (_) {}
        await _connectAndDiscover(onUpdate);
        break;
      }
    });

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 8));

    await Future.delayed(const Duration(seconds: 9));
    if (_device == null) {
      scanning = false;
      status = 'DeskBuddy não encontrado';
      onUpdate();
      try { await FlutterBluePlus.stopScan(); } catch (_) {}
    }
  }

  Future<void> _connectAndDiscover(Function onUpdate) async {
    if (_device == null) return;

    _jsonChar = null;

    try {
      try {
        await _device!.connect(timeout: const Duration(seconds: 15));
      } catch (_) {}

      await _connSub?.cancel();
      _connSub = _device!.connectionState.listen((state) async {
        if (!running) return;
        if (state == BluetoothConnectionState.disconnected) {
          status = 'Conexão caiu. Reconectando...';
          emocoes = {};
          _jsonChar = null;
          onUpdate();
          _scheduleReconnect(onUpdate);
        }
      });

      final services = await _device!.discoverServices();

      for (final s in services) {
        if (s.uuid.toString().toLowerCase() != serviceUuid.toLowerCase()) continue;
        for (final c in s.characteristics) {
          if (c.uuid.toString().toLowerCase() == characteristicUuid.toLowerCase()) {
            _jsonChar = c;
            break;
          }
        }
      }

      if (_jsonChar == null) {
        status = 'Characteristic do DeskBuddy não encontrada.';
        onUpdate();
        await _disconnect();
        _scheduleReconnect(onUpdate);
        return;
      }

      status = 'Conectado';
      onUpdate();
      await _readOnce(onUpdate);
    } catch (e) {
      status = 'Erro ao conectar: $e';
      emocoes = {};
      onUpdate();
      await _disconnect();
      _scheduleReconnect(onUpdate);
    }
  }

  Future<void> _readOnce(Function onUpdate) async {
    if (!running) return;
    // [FIX-UI-1] Não lê durante OTA
    if (_pollPaused) return;

    if (_device == null || _jsonChar == null) {
      await _ensureConnected(onUpdate);
      return;
    }

    final state = await _device!.connectionState.first;
    if (state != BluetoothConnectionState.connected) {
      await _ensureConnected(onUpdate);
      return;
    }

    try {
      final value   = await _jsonChar!.read();
      final jsonStr = utf8.decode(value);
      final decoded = jsonDecode(jsonStr);

      if (decoded is! Map) {
        status = 'JSON inválido retornado pelo DeskBuddy.';
        emocoes = {};
        onUpdate();
        return;
      }

      if (nomeSalvo == null || senhaSalva == null) {
        status = 'Nome e senha não definidos.';
        emocoes = {};
        onUpdate();
        return;
      }

      if (decoded['nome'] == nomeSalvo && decoded['senha'] == senhaSalva) {
        emocoes = Map<String, dynamic>.from(decoded as Map);
        status = 'Dados atualizados';
      } else {
        emocoes = {};
        status = 'Nome ou senha inválidos para este DeskBuddy!';
      }

      onUpdate();
    } catch (e) {
      status = 'Erro ao ler JSON: $e';
      emocoes = {};
      onUpdate();
      _scheduleReconnect(onUpdate);
    }
  }

  void _scheduleReconnect(Function onUpdate) {
    if (!running) return;
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(const Duration(seconds: 2), () async {
      if (!running) return;
      await _ensureConnected(onUpdate);
    });
  }

  Future<void> _disconnect() async {
    try { await _connSub?.cancel(); _connSub = null; } catch (_) {}
    if (_device != null) {
      try { await _device!.disconnect(); } catch (_) {}
    }
    _device   = null;
    _jsonChar = null;
    try { await _scanSub?.cancel(); _scanSub = null; } catch (_) {}
    try { await FlutterBluePlus.stopScan(); } catch (_) {}
  }

  void startAutoUpdate(Function onUpdate) => start(onUpdate);
  void stopAutoUpdate() => stop();
}
