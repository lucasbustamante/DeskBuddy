import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_blue/flutter_blue.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:permission_handler/permission_handler.dart';

class LoginPage extends StatefulWidget {
  final Function(String, String) onLoginSuccess;

  const LoginPage({Key? key, required this.onLoginSuccess}) : super(key: key);

  @override
  _LoginPageState createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final TextEditingController _nameController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  String? _error;
  bool _scanning = false;
  List<String> _buddyNames = [];
  Map<String, BluetoothDevice> _buddyDevices = {};
  Map<String, String> _buddyIds = {}; // nome -> id BLE (para debug)

  static const String serviceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  static const String characteristicUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

  @override
  void initState() {
    super.initState();
    _pedePermissoesBluetooth();
  }

  Future<void> _pedePermissoesBluetooth() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.locationWhenInUse.request();
    await Permission.bluetooth.request();
    await Permission.bluetoothAdvertise.request();
  }

  Future<void> _buscarBudys() async {
    setState(() {
      _scanning = true;
      _buddyNames.clear();
      _buddyDevices.clear();
      _buddyIds.clear();
    });

    FlutterBlue flutterBlue = FlutterBlue.instance;
    List<BluetoothDevice> foundDevices = [];
    Set<String> nomes = {};
    Map<String, BluetoothDevice> devicesByName = {};
    Map<String, String> idsByName = {};

    var subscription = flutterBlue.scan(timeout: const Duration(seconds: 5)).listen((scanResult) {
      final name = scanResult.device.name;
      print('BLE scan detectado: ${scanResult.device.id} - $name');
      // Garante que device seja único pelo id
      if (name.startsWith("DeskBuddy") && !foundDevices.any((d) => d.id == scanResult.device.id)) {
        foundDevices.add(scanResult.device);
      }
    });

    await Future.delayed(const Duration(seconds: 6));
    await subscription.cancel();

    for (final device in foundDevices) {
      try {
        print('Tentando conectar em ${device.id}');
        await device.connect(timeout: const Duration(seconds: 5));
        final services = await device.discoverServices();
        for (final service in services) {
          if (service.uuid.toString().toLowerCase() == serviceUuid) {
            for (final c in service.characteristics) {
              if (c.uuid.toString().toLowerCase() == characteristicUuid) {
                final value = await c.read();
                final jsonStr = String.fromCharCodes(value);
                print('Characteristic do Buddy lido: $jsonStr');
                try {
                  final json = jsonDecode(jsonStr);
                  if (json['nome'] != null) {
                    final nomeBuddy = json['nome'].toString();
                    nomes.add(nomeBuddy);
                    devicesByName[nomeBuddy] = device;
                    idsByName[nomeBuddy] = device.id.toString();
                    print('Buddy detectado no JSON: $nomeBuddy (${device.id})');
                  }
                } catch (e) {
                  print('Erro ao decodificar JSON: $e');
                }
              }
            }
          }
        }
        await device.disconnect();
      } catch (e) {
        try { await device.disconnect(); } catch (_) {}
        print('Erro ao conectar/lendo characteristic: $e');
      }
    }

    setState(() {
      _scanning = false;
      _buddyNames = nomes.toList();
      _buddyDevices = devicesByName;
      _buddyIds = idsByName;
    });

    if (_buddyNames.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Nenhum DeskBuddy encontrado por perto.'))
      );
    }
  }

  void _selecionarBuddy(String nome) {
    setState(() {
      _nameController.text = nome;
      _buddyNames.clear(); // oculta lista após seleção
      _error = null;
    });
  }

  Future<void> _login() async {
    String name = _nameController.text.trim();
    String password = _passwordController.text.trim();

    if (name.isEmpty || password.isEmpty) {
      setState(() => _error = "Selecione o Buddy e preencha a senha.");
      return;
    }

    final buddyDevice = _buddyDevices[name];
    if (buddyDevice == null) {
      setState(() => _error = "Selecione um Buddy válido.");
      return;
    }

    bool senhaOk = false;
    try {
      await buddyDevice.connect(timeout: const Duration(seconds: 5));
      final services = await buddyDevice.discoverServices();
      for (final service in services) {
        if (service.uuid.toString().toLowerCase() == serviceUuid) {
          for (final c in service.characteristics) {
            if (c.uuid.toString().toLowerCase() == characteristicUuid) {
              final value = await c.read();
              final jsonStr = String.fromCharCodes(value);
              final json = jsonDecode(jsonStr);
              if (json['nome'] == name && json['senha'] == password) {
                senhaOk = true;
              }
            }
          }
        }
      }
      await buddyDevice.disconnect();
    } catch (e) {
      try { await buddyDevice.disconnect(); } catch (_) {}
      setState(() => _error = "Erro ao conectar ao Buddy.");
      return;
    }

    if (senhaOk) {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('deskbuddy_nome', name);
      await prefs.setString('deskbuddy_senha', password);

      widget.onLoginSuccess(name, password);
    } else {
      setState(() {
        _error = "Senha incorreta para este Buddy.";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    bool buddySelecionado = _nameController.text.isNotEmpty;

    return Scaffold(
      body: Padding(
        padding: const EdgeInsets.all(32),
        child: Center(
          child: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(Icons.wb_sunny_rounded, size: 60, color: Colors.orange[800]),
                const SizedBox(height: 18),
                const Text("Conectar DeskBuddy",
                    style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
                const SizedBox(height: 22),

                /// 🔹 BOTÃO FIXO "BUSCAR BUDDYS"
                ElevatedButton.icon(
                  onPressed: _scanning ? null : _buscarBudys,
                  icon: const Icon(Icons.search),
                  label: const Text("Buscar Buddys"),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.orange[700],
                    minimumSize: const Size(180, 45),
                  ),
                ),

                if (_scanning) ...[
                  const SizedBox(height: 12),
                  const CircularProgressIndicator(),
                  const SizedBox(height: 6),
                  const Text('Buscando Buddys próximos...'),
                ],

                /// 🔹 LISTA DE BUDDYS ENCONTRADOS
                if (_buddyNames.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.symmetric(vertical: 16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text("Selecione seu Buddy:",
                            style: TextStyle(fontWeight: FontWeight.bold)),
                        ..._buddyNames.map((nome) => Card(
                          child: ListTile(
                            leading: const Icon(Icons.toys, color: Colors.orange),
                            title: Text(nome),
                            subtitle: Text(_buddyIds[nome] ?? ""), // Mostra o id BLE (debug)
                            trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                            onTap: () => _selecionarBuddy(nome),
                          ),
                        )),
                      ],
                    ),
                  ),

                /// 🔹 MOSTRA APENAS SE UM BUDDY FOI SELECIONADO
                if (buddySelecionado) ...[
                  const SizedBox(height: 20),
                  Text("Buddy selecionado: ${_nameController.text}",
                      style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w500)),
                  const SizedBox(height: 12),
                  TextField(
                    controller: _passwordController,
                    decoration: const InputDecoration(labelText: 'Senha do Buddy'),
                    obscureText: true,
                  ),
                  if (_error != null)
                    Padding(
                      padding: const EdgeInsets.only(top: 10),
                      child: Text(_error!, style: const TextStyle(color: Colors.red)),
                    ),
                  const SizedBox(height: 20),
                  ElevatedButton(
                    onPressed: _login,
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.deepOrange,
                      minimumSize: const Size(160, 50),
                    ),
                    child: const Text("Conectar"),
                  ),
                ]
              ],
            ),
          ),
        ),
      ),
    );
  }
}
