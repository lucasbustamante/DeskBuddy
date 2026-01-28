import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:permission_handler/permission_handler.dart';

class LoginPage extends StatefulWidget {
  final void Function(BuildContext, String, String) onLoginSuccess;

  const LoginPage({Key? key, required this.onLoginSuccess}) : super(key: key);

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final TextEditingController _nameController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();

  String? _error;
  bool _scanning = false;
  bool _connecting = false;

  List<String> _buddyNames = [];
  Map<String, BluetoothDevice> _buddyDevices = {};
  Map<String, String> _buddyIds = {}; // nome -> remoteId (debug)

  static const String serviceUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  static const String characteristicUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

  StreamSubscription<List<ScanResult>>? _scanSub;

  @override
  void initState() {
    super.initState();
    _pedePermissoesBluetooth();
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _nameController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  Future<void> _pedePermissoesBluetooth() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();

    // Alguns aparelhos/Androids ainda exigem location para scan
    await Permission.locationWhenInUse.request();

    // Dependendo do Android/ROM pode ajudar
    await Permission.bluetooth.request();
    await Permission.bluetoothAdvertise.request();
  }

  Future<void> _buscarBudys() async {
    setState(() {
      _scanning = true;
      _error = null;
      _buddyNames.clear();
      _buddyDevices.clear();
      _buddyIds.clear();
    });

    final List<BluetoothDevice> foundDevices = [];

    // limpa scan anterior, se houver
    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {}

    await _scanSub?.cancel();

    try {
      // Inicia scan (vai parar sozinho no timeout)
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));

      _scanSub = FlutterBluePlus.scanResults.listen((results) {
        for (final r in results) {
          final device = r.device;
          final name = device.platformName;

          // Debug
          // ignore: avoid_print
          print('BLE scan detectado: ${device.remoteId} - $name');

          if (!name.startsWith("DeskBuddy")) continue;

          final already =
          foundDevices.any((d) => d.remoteId == device.remoteId);
          if (!already) {
            foundDevices.add(device);
          }
        }
      });

      // Espera um pouco mais do que o scan pra garantir que pegou resultados
      await Future.delayed(const Duration(seconds: 6));
    } catch (e) {
      setState(() {
        _error = "Erro ao escanear: $e";
      });
    } finally {
      try {
        await FlutterBluePlus.stopScan();
      } catch (_) {}
      await _scanSub?.cancel();
      _scanSub = null;
    }

    // Agora: conecta em cada device encontrado e lê o JSON
    final Set<String> nomes = {};
    final Map<String, BluetoothDevice> devicesByName = {};
    final Map<String, String> idsByName = {};

    for (final device in foundDevices) {
      try {
        // ignore: avoid_print
        print('Tentando conectar em ${device.remoteId}');

        try {
          await device.connect(timeout: const Duration(seconds: 5));
        } catch (_) {
          // se já estiver conectado, ignore
        }

        final services = await device.discoverServices();

        for (final service in services) {
          if (service.uuid.toString().toLowerCase() !=
              serviceUuid.toLowerCase()) {
            continue;
          }

          for (final c in service.characteristics) {
            if (c.uuid.toString().toLowerCase() !=
                characteristicUuid.toLowerCase()) {
              continue;
            }

            final List<int> value = await c.read();
            final String jsonStr = utf8.decode(value);

            // ignore: avoid_print
            print('Characteristic do Buddy lido: $jsonStr');

            try {
              final decoded = jsonDecode(jsonStr);
              if (decoded is Map && decoded['nome'] != null) {
                final nomeBuddy = decoded['nome'].toString();
                nomes.add(nomeBuddy);
                devicesByName[nomeBuddy] = device;
                idsByName[nomeBuddy] = device.remoteId.toString();

                // ignore: avoid_print
                print('Buddy detectado no JSON: $nomeBuddy (${device.remoteId})');
              }
            } catch (e) {
              // ignore: avoid_print
              print('Erro ao decodificar JSON: $e');
            }
          }
        }

        await device.disconnect();
      } catch (e) {
        try {
          await device.disconnect();
        } catch (_) {}

        // ignore: avoid_print
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
        const SnackBar(content: Text('Nenhum DeskBuddy encontrado por perto.')),
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
    final String name = _nameController.text.trim();
    final String password = _passwordController.text.trim();

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

    setState(() { _connecting = true; _error = null; });

    try {
      try {
        await buddyDevice.connect(timeout: const Duration(seconds: 5));
      } catch (_) {}

      final services = await buddyDevice.discoverServices();

      for (final service in services) {
        if (service.uuid.toString().toLowerCase() !=
            serviceUuid.toLowerCase()) {
          continue;
        }

        for (final c in service.characteristics) {
          if (c.uuid.toString().toLowerCase() !=
              characteristicUuid.toLowerCase()) {
            continue;
          }

          final value = await c.read();
          final jsonStr = utf8.decode(value);
          final decoded = jsonDecode(jsonStr);

          if (decoded is Map &&
              decoded['nome'] == name &&
              decoded['senha'] == password) {
            senhaOk = true;
          }
        }
      }

      await buddyDevice.disconnect();
      if (mounted) setState(() { _connecting = false; });
    } catch (e) {
      try {
        await buddyDevice.disconnect();
      } catch (_) {}

      if (mounted) setState(() { _connecting = false; _error = "Erro ao conectar ao Buddy: $e"; });
      return;
    }

    if (senhaOk) {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('deskbuddy_nome', name);
      await prefs.setString('deskbuddy_senha', password);

      if (mounted) setState(() { _connecting = false; });
      widget.onLoginSuccess(context, name, password);
    } else {
      if (mounted) setState(() { _connecting = false; _error = "Senha incorreta para este Buddy."; });
    }
  }

  @override
  Widget build(BuildContext context) {
    final bool buddySelecionado = _nameController.text.isNotEmpty;

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
                const Text(
                  "Conectar DeskBuddy",
                  style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold),
                ),
                const SizedBox(height: 22),

                ElevatedButton.icon(
                  onPressed: _scanning ? null : _buscarBudys,
                  icon: const Icon(Icons.search),
                  label: const Text("Buscar Buddys"),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.orange,
                    minimumSize: const Size(180, 45),
                  ),
                ),

                if (_scanning) ...[
                  const SizedBox(height: 12),
                  const CircularProgressIndicator(),
                  const SizedBox(height: 6),
                  const Text('Buscando Buddys próximos...'),
                ],

                if (_buddyNames.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.symmetric(vertical: 16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(
                          "Selecione seu Buddy:",
                          style: TextStyle(fontWeight: FontWeight.bold),
                        ),
                        ..._buddyNames.map(
                              (nome) => Card(
                            child: ListTile(
                              leading: const Icon(Icons.toys, color: Colors.orange),
                              title: Text(nome),
                              subtitle: Text(_buddyIds[nome] ?? ""),
                              trailing: const Icon(Icons.arrow_forward_ios, size: 16),
                              onTap: () => _selecionarBuddy(nome),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),

                if (buddySelecionado) ...[
                  const SizedBox(height: 20),
                  Text(
                    "Buddy selecionado: ${_nameController.text}",
                    style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w500),
                  ),
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
                    onPressed: (_connecting) ? null : _login,
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.deepOrange,
                      minimumSize: const Size(160, 50),
                    ),
                    child: _connecting
                        ? const SizedBox(
                            height: 22,
                            width: 22,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Text("Conectar"),
                  ),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }
}
