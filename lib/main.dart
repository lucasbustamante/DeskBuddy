import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_blue/flutter_blue.dart';
import 'package:permission_handler/permission_handler.dart';
import 'dart:convert';

void main() => runApp(DeskBuddyApp());

class DeskBuddyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'DeskBuddy BLE',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(primarySwatch: Colors.blue),
      home: DeskBuddyHomePage(),
    );
  }
}

class DeskBuddyHomePage extends StatefulWidget {
  @override
  State<DeskBuddyHomePage> createState() => _DeskBuddyHomePageState();
}

class _DeskBuddyHomePageState extends State<DeskBuddyHomePage> {
  Map<String, dynamic> emocoes = {};
  String status = "Pronto";
  bool scanning = false;
  bool running = false;
  Timer? timer;

  // UUIDs do ESP32 (copie exatamente igual do código ESP32)
  final String serviceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  final String characteristicUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  final String deviceName = "DeskBuddy";

  @override
  void initState() {
    super.initState();
    requestPermissions();
  }

  @override
  void dispose() {
    timer?.cancel();
    super.dispose();
  }

  Future<void> requestPermissions() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.locationWhenInUse.request();
    await Permission.bluetooth.request();
    await Permission.bluetoothAdvertise.request();
  }

  Future<void> scanAndConnectOnce() async {
    setState(() {
      scanning = true;
      status = "Escaneando...";
    });

    FlutterBlue flutterBlue = FlutterBlue.instance;
    bool found = false;

    try {
      var subscription = flutterBlue.scan(timeout: Duration(seconds: 8)).listen((scanResult) async {
        if (scanResult.device.name == deviceName && !found) {
          found = true;
          setState(() {
            status = "Dispositivo encontrado! Conectando...";
          });

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
                    setState(() {
                      emocoes = decoded;
                      status = "Dados recebidos!";
                    });
                  } catch (e) {
                    setState(() {
                      status = "Erro ao decodificar JSON: $e\nValor lido: $jsonStr";
                    });
                  }
                  await scanResult.device.disconnect();
                  break;
                }
              }
            }
          }
        }
      }, onDone: () {
        if (!found) {
          setState(() {
            status = "DeskBuddy não encontrado";
          });
        }
        setState(() {
          scanning = false;
        });
      });

      await Future.delayed(Duration(seconds: 10));
      await subscription.cancel();
    } catch (e) {
      setState(() {
        status = "Erro: $e";
        scanning = false;
      });
    }
  }

  void startAutoUpdate() {
    setState(() {
      running = true;
      status = "Iniciando atualização automática...";
    });

    timer?.cancel(); // Cancela qualquer timer anterior
    timer = Timer.periodic(Duration(seconds: 5), (timer) async {
      if (!mounted) return;
      await scanAndConnectOnce();
    });
  }

  void stopAutoUpdate() {
    timer?.cancel();
    setState(() {
      running = false;
      status = "Atualização automática parada.";
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("DeskBuddy BLE")),
      body: Padding(
        padding: EdgeInsets.all(20),
        child: Column(
          children: [
            Text("Status: $status", style: TextStyle(fontSize: 18)),
            SizedBox(height: 25),
            if (emocoes.isEmpty)
              Text("Nenhuma emoção recebida ainda.", style: TextStyle(fontSize: 18)),
            if (emocoes.isNotEmpty)
              ...[
                if (emocoes.containsKey('dominante'))
                  Padding(
                    padding: EdgeInsets.only(bottom: 16),
                    child: Row(
                      children: [
                        Text(
                          "Emoção no display: ",
                          style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold, color: Colors.blueAccent),
                        ),
                        Text(
                          capitalize(emocoes['dominante'].toString()),
                          style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold, color: Colors.deepPurple),
                        ),
                      ],
                    ),
                  ),
                ...emocoes.entries
                    .where((e) => e.key != 'dominante')
                    .map((e) => Padding(
                  padding: EdgeInsets.symmetric(vertical: 4),
                  child: Row(
                    children: [
                      Text("${capitalize(e.key)}:", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                      SizedBox(width: 10),
                      Text("${e.value}%", style: TextStyle(fontSize: 18)),
                    ],
                  ),
                )),
              ],
            Spacer(),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                ElevatedButton.icon(
                  onPressed: running ? null : startAutoUpdate,
                  icon: Icon(Icons.play_arrow),
                  label: Text("Iniciar atualização"),
                  style: ElevatedButton.styleFrom(minimumSize: Size(170, 48)),
                ),
                SizedBox(width: 20),
                ElevatedButton.icon(
                  onPressed: running ? stopAutoUpdate : null,
                  icon: Icon(Icons.stop),
                  label: Text("Parar"),
                  style: ElevatedButton.styleFrom(minimumSize: Size(110, 48)),
                ),
              ],
            ),
            SizedBox(height: 20),
          ],
        ),
      ),
    );
  }

  String capitalize(String s) {
    if (s.isEmpty) return s;
    return s[0].toUpperCase() + s.substring(1);
  }
}
