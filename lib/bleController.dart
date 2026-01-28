import 'dart:async';
import 'dart:convert';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Controla conexão BLE com o DeskBuddy.
/// Objetivo:
/// - Conectar 1x e manter conectado (sem precisar reiniciar o app)
/// - Fazer leitura periódica da characteristic (JSON) sem ficar conectando/desconectando
/// - Auto-reconectar quando cair
class BleController {
  Map<String, dynamic> emocoes = {};
  String status = "Pronto";
  bool scanning = false;
  bool running = false;

  Timer? _pollTimer;
  Timer? _reconnectTimer;

  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _jsonChar;

  final String serviceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  final String characteristicUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  final String deviceNamePrefix = "DeskBuddy";

  String? nomeSalvo;
  String? senhaSalva;

  Future<void> requestPermissions() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.locationWhenInUse.request();
    await Permission.bluetooth.request();
    await Permission.bluetoothAdvertise.request();
  }

  Future<void> _carregaCredenciaisSalvas() async {
    final prefs = await SharedPreferences.getInstance();
    nomeSalvo = prefs.getString('deskbuddy_nome');
    senhaSalva = prefs.getString('deskbuddy_senha');
  }

  /// Inicia (ou garante) conexão e começa polling.
  Future<void> start(Function onUpdate) async {
    running = true;
    await requestPermissions();
    await _carregaCredenciaisSalvas();

    // já tenta conectar imediatamente
    await _ensureConnected(onUpdate);

    // polling do JSON
    _pollTimer?.cancel();
    _pollTimer = Timer.periodic(const Duration(seconds: 2), (_) async {
      if (!running) return;
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

  // ---------------------------------------------------------------------------
  // Internals
  // ---------------------------------------------------------------------------

  Future<void> _ensureConnected(Function onUpdate) async {
    if (!running) return;

    // Se já tem device e está conectado, ok.
    if (_device != null) {
      final state = await _device!.connectionState.first;
      if (state == BluetoothConnectionState.connected && _jsonChar != null) {
        status = "Conectado";
        onUpdate();
        return;
      }
    }

    await _disconnect();

    status = "Buscando DeskBuddy...";
    scanning = true;
    onUpdate();

    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {}

    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) async {
      if (!running || _device != null) return;

      for (final r in results) {
        final advName = r.device.platformName;

        if (!advName.startsWith(deviceNamePrefix)) continue;

        // Se já tem nome salvo, tenta bater pelo nome no advertising:
        // "DeskBuddy: Kizmo" -> contém "Kizmo"
        if (nomeSalvo != null && nomeSalvo!.isNotEmpty) {
          if (!advName.toLowerCase().contains(nomeSalvo!.toLowerCase())) {
            continue;
          }
        }

        _device = r.device;
        scanning = false;
        status = "Conectando...";
        onUpdate();

        try {
          await FlutterBluePlus.stopScan();
        } catch (_) {}

        await _connectAndDiscover(onUpdate);
        break;
      }
    });

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 8));

    // fallback: se não achou, encerra scan
    await Future.delayed(const Duration(seconds: 9));
    if (_device == null) {
      scanning = false;
      status = "DeskBuddy não encontrado";
      onUpdate();
      try {
        await FlutterBluePlus.stopScan();
      } catch (_) {}
    }
  }

  Future<void> _connectAndDiscover(Function onUpdate) async {
    if (_device == null) return;

    // limpa estado de char antigo
    _jsonChar = null;

    try {
      // connect
      try {
        await _device!.connect(timeout: const Duration(seconds: 15));
      } catch (_) {
        // se já estiver conectando/conectado, ignore
      }

      // observa quedas
      await _connSub?.cancel();
      _connSub = _device!.connectionState.listen((state) async {
        if (!running) return;
        if (state == BluetoothConnectionState.disconnected) {
          status = "Conexão caiu. Reconectando...";
          emocoes = {};
          _jsonChar = null;
          onUpdate();
          _scheduleReconnect(onUpdate);
        }
      });

      // Descobre services e acha a characteristic do JSON
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
        status = "Characteristic do DeskBuddy não encontrada.";
        onUpdate();
        await _disconnect();
        _scheduleReconnect(onUpdate);
        return;
      }

      status = "Conectado";
      onUpdate();

      // primeira leitura logo ao conectar
      await _readOnce(onUpdate);
    } catch (e) {
      status = "Erro ao conectar: $e";
      emocoes = {};
      onUpdate();
      await _disconnect();
      _scheduleReconnect(onUpdate);
    }
  }

  Future<void> _readOnce(Function onUpdate) async {
    if (!running) return;

    // se perdeu conexão, tenta recuperar
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
      final value = await _jsonChar!.read();
      final jsonStr = utf8.decode(value);

      final decoded = jsonDecode(jsonStr);
      if (decoded is! Map) {
        status = "JSON inválido retornado pelo DeskBuddy.";
        emocoes = {};
        onUpdate();
        return;
      }

      // valida credenciais salvas
      if (nomeSalvo == null || senhaSalva == null) {
        status = "Nome e senha não definidos.";
        emocoes = {};
        onUpdate();
        return;
      }

      if (decoded['nome'] == nomeSalvo && decoded['senha'] == senhaSalva) {
        emocoes = Map<String, dynamic>.from(decoded as Map);
        status = "Dados atualizados";
      } else {
        emocoes = {};
        status = "Nome ou senha inválidos para este DeskBuddy!";
      }

      onUpdate();
    } catch (e) {
      status = "Erro ao ler JSON: $e";
      emocoes = {};
      onUpdate();
      // tenta recuperar sem travar o app
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
    try {
      await _connSub?.cancel();
      _connSub = null;
    } catch (_) {}

    if (_device != null) {
      try {
        await _device!.disconnect();
      } catch (_) {}
    }
    _device = null;
    _jsonChar = null;

    try {
      await _scanSub?.cancel();
      _scanSub = null;
    } catch (_) {}

    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {}
  }

  // ---------------------------------------------------------------------------
  // API compat com o código atual da Home
  // ---------------------------------------------------------------------------

  void startAutoUpdate(Function onUpdate) {
    // antes: conectava/desconectava a cada 5s.
    // agora: mantém conectado e só lê periodicamente.
    start(onUpdate);
  }

  void stopAutoUpdate() {
    stop();
  }
}
