import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:deskbuddy/app_theme.dart';
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
    final cs = Theme.of(context).colorScheme;
    final bool buddySelecionado = _nameController.text.isNotEmpty;

    return Scaffold(
      backgroundColor: AppColors.bg,
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 520),
              child: SingleChildScrollView(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Container(
                      padding: const EdgeInsets.all(18),
                      decoration: BoxDecoration(
                        color: Colors.white,
                        borderRadius: BorderRadius.circular(26),
                        border: Border.all(color: AppColors.line),
                        boxShadow: const [
                          BoxShadow(
                            blurRadius: 30,
                            offset: Offset(0, 14),
                            color: Color(0x10000000),
                          )
                        ],
                      ),
                      child: Column(
                        children: [
                          Row(
                            children: [
                              Container(
                                width: 44,
                                height: 44,
                                decoration: BoxDecoration(
                                  color: cs.primary.withOpacity(.12),
                                  borderRadius: BorderRadius.circular(16),
                                ),
                                child: Icon(Icons.wb_sunny_rounded, color: cs.primary),
                              ),
                              const SizedBox(width: 12),
                              const Expanded(
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    Text("Conectar DeskBuddy", style: TextStyle(fontSize: 18, fontWeight: FontWeight.w900)),
                                    SizedBox(height: 2),
                                    Text("Ache seu buddy e entre com a senha", style: TextStyle(color: AppColors.muted)),
                                  ],
                                ),
                              ),
                            ],
                          ),
                          const SizedBox(height: 14),

                          // Buscar Buddys
                          SizedBox(
                            width: double.infinity,
                            child: ElevatedButton.icon(
                              onPressed: _scanning ? null : _buscarBudys,
                              icon: _scanning
                                  ? const SizedBox(
                                      width: 18,
                                      height: 18,
                                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                                    )
                                  : const Icon(Icons.search_rounded),
                              label: Text(_scanning ? "Buscando…" : "Buscar Buddys"),
                              style: ElevatedButton.styleFrom(
                                backgroundColor: cs.primary,
                                foregroundColor: Colors.white,
                                padding: const EdgeInsets.symmetric(vertical: 14),
                                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
                              ),
                            ),
                          ),
                          const SizedBox(height: 12),

                          if (_buddyNames.isNotEmpty)
                            Align(
                              alignment: Alignment.centerLeft,
                              child: Text(
                                "Selecione seu buddy",
                                style: TextStyle(color: AppColors.muted, fontWeight: FontWeight.w800),
                              ),
                            ),
                          if (_buddyNames.isNotEmpty) const SizedBox(height: 8),

                          if (_buddyNames.isNotEmpty)
                            Container(
                              decoration: BoxDecoration(
                                color: AppColors.bg2,
                                borderRadius: BorderRadius.circular(18),
                                border: Border.all(color: AppColors.line),
                              ),
                              child: Column(
                                children: _buddyNames.map((name) {
                                  final selected = _nameController.text == name;
                                  return InkWell(
                                    borderRadius: BorderRadius.circular(18),
                                    onTap: () => setState(() {
                                      _nameController.text = name;
                                      _error = null;
                                    }),
                                    child: Container(
                                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
                                      decoration: BoxDecoration(
                                        border: Border(
                                          bottom: BorderSide(
                                            color: name == _buddyNames.last ? Colors.transparent : AppColors.line,
                                          ),
                                        ),
                                      ),
                                      child: Row(
                                        children: [
                                          Icon(
                                            selected ? Icons.radio_button_checked_rounded : Icons.radio_button_off_rounded,
                                            color: selected ? cs.primary : AppColors.muted,
                                          ),
                                          const SizedBox(width: 10),
                                          Expanded(
                                            child: Text(
                                              name,
                                              style: TextStyle(
                                                fontWeight: FontWeight.w900,
                                                color: selected ? AppColors.text : AppColors.text,
                                              ),
                                            ),
                                          ),
                                          if (_buddyIds.containsKey(name))
                                            Text(
                                              _buddyIds[name]!.split(':').last,
                                              style: const TextStyle(color: AppColors.muted, fontSize: 12),
                                            ),
                                        ],
                                      ),
                                    ),
                                  );
                                }).toList(),
                              ),
                            ),

                          const SizedBox(height: 14),

                          TextField(
                            controller: _passwordController,
                            obscureText: true,
                            enabled: buddySelecionado,
                            decoration: InputDecoration(
                              hintText: "Senha do buddy",
                              prefixIcon: const Icon(Icons.lock_rounded),
                            ),
                          ),

                          if (_error != null) ...[
                            const SizedBox(height: 10),
                            Container(
                              width: double.infinity,
                              padding: const EdgeInsets.all(12),
                              decoration: BoxDecoration(
                                color: AppColors.bad.withOpacity(.10),
                                borderRadius: BorderRadius.circular(18),
                                border: Border.all(color: AppColors.bad.withOpacity(.25)),
                              ),
                              child: Row(
                                children: [
                                  const Icon(Icons.error_outline_rounded, color: AppColors.bad),
                                  const SizedBox(width: 10),
                                  Expanded(
                                    child: Text(
                                      _error!,
                                      style: const TextStyle(color: AppColors.bad, fontWeight: FontWeight.w800),
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          ],

                          const SizedBox(height: 14),

                          SizedBox(
                            width: double.infinity,
                            child: ElevatedButton(
                              onPressed: (buddySelecionado && !_connecting) ? _login : null,
                              style: ElevatedButton.styleFrom(
                                backgroundColor: cs.primary,
                                foregroundColor: Colors.white,
                                padding: const EdgeInsets.symmetric(vertical: 14),
                                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
                              ),
                              child: _connecting
                                  ? const SizedBox(
                                      height: 22,
                                      width: 22,
                                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                                    )
                                  : const Text("Conectar", style: TextStyle(fontWeight: FontWeight.w900)),
                            ),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 14),
                    const Text(
                      "Se não aparecer, aproxime o DeskBuddy e verifique o Bluetooth.",
                      style: TextStyle(color: AppColors.muted),
                      textAlign: TextAlign.center,
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }

}
