import 'dart:async';
import 'dart:convert';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';

class BleController {
  Map<String, dynamic> emocoes = {};
  String status = "Pronto";
  bool scanning = false;
  bool running = false;
  Timer? timer;

  final String serviceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  final String characteristicUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  final String deviceNamePrefix = "DeskBuddy";

  String? nomeSalvo;
  String? senhaSalva;

  Future<void> requestPermissions() async {
    // Android 12+ exige BLUETOOTH_SCAN / CONNECT
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();

    // Alguns aparelhos/Androids ainda exigem location pra scan
    await Permission.locationWhenInUse.request();

    // Não custa pedir também (depende da versão)
    await Permission.bluetooth.request();
    await Permission.bluetoothAdvertise.request();
  }

  Future<void> _carregaCredenciaisSalvas() async {
    final prefs = await SharedPreferences.getInstance();
    nomeSalvo = prefs.getString('deskbuddy_nome');
    senhaSalva = prefs.getString('deskbuddy_senha');
  }

  Future<void> scanAndConnectOnce() async {
    await requestPermissions();

    await _carregaCredenciaisSalvas();

    scanning = true;
    status = "Escaneando...";

    bool found = false;

    try {
      // garante que não tem scan antigo rodando
      await FlutterBluePlus.stopScan();

      // começa scan
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 8));

      // escuta resultados
      late final StreamSubscription<List<ScanResult>> sub;
      sub = FlutterBluePlus.scanResults.listen((results) async {
        if (found) return;

        for (final r in results) {
          final name = r.device.platformName; // <-- no plus é platformName
          if (name.startsWith(deviceNamePrefix)) {
            found = true;
            status = "Dispositivo encontrado! Conectando...";

            // parar scan assim que achar
            await FlutterBluePlus.stopScan();

            final device = r.device;

            try {
              await device.connect(timeout: const Duration(seconds: 10));
            } catch (_) {
              // se já estiver conectado, ignore
            }

            final services = await device.discoverServices();

            for (final service in services) {
              if (service.uuid.toString().toLowerCase() ==
                  serviceUuid.toLowerCase()) {
                for (final c in service.characteristics) {
                  if (c.uuid.toString().toLowerCase() ==
                      characteristicUuid.toLowerCase()) {
                    final value = await c.read();
                    final jsonStr = utf8.decode(value);

                    try {
                      final decoded =
                      jsonDecode(jsonStr) as Map<String, dynamic>;

                      if (nomeSalvo == null || senhaSalva == null) {
                        status = "Nome e senha não definidos.";
                        emocoes = {};
                      } else if (decoded['nome'] == nomeSalvo &&
                          decoded['senha'] == senhaSalva) {
                        emocoes = decoded;
                        status = "Dados recebidos!";
                      } else {
                        emocoes = {};
                        status = "Nome ou senha inválidos para o DeskBuddy!";
                      }
                    } catch (e) {
                      status =
                      "Erro ao decodificar JSON: $e\nValor lido: $jsonStr";
                      emocoes = {};
                    }

                    await device.disconnect();
                    break;
                  }
                }
              }
            }

            await sub.cancel();
            scanning = false;
            return;
          }
        }
      });

      // fallback de tempo total (pra garantir que vai encerrar)
      await Future.delayed(const Duration(seconds: 10));

      if (!found) {
        status = "DeskBuddy não encontrado";
      }

      scanning = false;
      await sub.cancel();
      await FlutterBluePlus.stopScan();
    } catch (e) {
      status = "Erro: $e";
      scanning = false;
      try {
        await FlutterBluePlus.stopScan();
      } catch (_) {}
    }
  }

  void startAutoUpdate(Function onUpdate) {
    running = true;
    status = "Iniciando atualização automática...";

    timer?.cancel();
    timer = Timer.periodic(const Duration(seconds: 5), (_) async {
      await scanAndConnectOnce();
      onUpdate();
    });
  }

  void stopAutoUpdate() {
    timer?.cancel();
    running = false;
    status = "Atualização automática parada.";
  }

  void dispose() {
    timer?.cancel();
  }
}
