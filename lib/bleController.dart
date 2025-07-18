//ble_controller.dart
import 'dart:async';
import 'dart:convert';
import 'package:flutter_blue/flutter_blue.dart';
import 'package:permission_handler/permission_handler.dart';

class BleController {
  Map<String, dynamic> emocoes = {};
  String status = "Pronto";
  bool scanning = false;
  bool running = false;
  Timer? timer;

  final String serviceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  final String characteristicUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  final String deviceName = "DeskBuddy";

  Future<void> requestPermissions() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.locationWhenInUse.request();
    await Permission.bluetooth.request();
    await Permission.bluetoothAdvertise.request();
  }

  Future<void> scanAndConnectOnce() async {
    scanning = true;
    status = "Escaneando...";

    FlutterBlue flutterBlue = FlutterBlue.instance;
    bool found = false;

    try {
      var subscription = flutterBlue.scan(timeout: Duration(seconds: 8)).listen((scanResult) async {
        if (scanResult.device.name == deviceName && !found) {
          found = true;
          status = "Dispositivo encontrado! Conectando...";

          await scanResult.device.connect(timeout: Duration(seconds: 10));
          List<BluetoothService> services = await scanResult.device.discoverServices();

          for (BluetoothService service in services) {
            if (service.uuid.toString().toLowerCase() == serviceUuid.toLowerCase()) {
              for (BluetoothCharacteristic c in service.characteristics) {
                if (c.uuid.toString().toLowerCase() == characteristicUuid.toLowerCase()) {
                  var value = await c.read();
                  String jsonStr = String.fromCharCodes(value);

                  try {
                    var decoded = jsonDecode(jsonStr) as Map<String, dynamic>;
                    emocoes = decoded;
                    status = "Dados recebidos!";
                  } catch (e) {
                    status = "Erro ao decodificar JSON: $e\nValor lido: $jsonStr";
                  }
                  await scanResult.device.disconnect();
                  break;
                }
              }
            }
          }
        }
      }, onDone: () {
        if (!found) status = "DeskBuddy não encontrado";
        scanning = false;
      });

      await Future.delayed(Duration(seconds: 10));
      await subscription.cancel();
    } catch (e) {
      status = "Erro: $e";
      scanning = false;
    }
  }

  void startAutoUpdate(Function onUpdate) {
    running = true;
    status = "Iniciando atualização automática...";

    timer?.cancel();
    timer = Timer.periodic(Duration(seconds: 5), (_) async {
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
