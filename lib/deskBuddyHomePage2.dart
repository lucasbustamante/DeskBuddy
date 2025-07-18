import 'package:deskbuddy/bleController.dart';
import 'package:flutter/material.dart';
import 'package:flutter_rounded_progress_bar/flutter_rounded_progress_bar.dart';
import 'package:flutter_rounded_progress_bar/rounded_progress_bar_style.dart';
import 'package:flutter_phoenix/flutter_phoenix.dart'; // <-- Importante

import 'buddys_encontrados_page.dart';

class DeskBuddyHomePage2 extends StatefulWidget {
  @override
  _DeskBuddyHomePageState2 createState() => _DeskBuddyHomePageState2();
}

class _DeskBuddyHomePageState2 extends State<DeskBuddyHomePage2> {
  final BleController bleController = BleController();
  bool _showRetry = false;
  int _reloadCount = 0;
  static const Duration timeout = Duration(seconds: 10);

  @override
  void initState() {
    super.initState();
    bleController.requestPermissions();
    _startScan();
  }

  void _startScan() {
    setState(() {
      _showRetry = false;
    });
    bleController.startAutoUpdate(updateState);

    Future.delayed(timeout, () {
      if (mounted && bleController.emocoes.isEmpty) {
        setState(() {
          _showRetry = true;
        });
      }
    });
  }

  void _reload() {
    setState(() {
      _reloadCount++;
    });

    if (_reloadCount >= 3) {
      // Apenas mostra mensagem, não reinicia a busca!
      setState(() {
        _showRetry = true;
      });
    } else {
      bleController.stopAutoUpdate();
      bleController.emocoes.clear();
      _startScan();
    }
  }

  @override
  void dispose() {
    bleController.stopAutoUpdate();
    bleController.dispose();
    super.dispose();
  }

  void updateState() {
    if (mounted) setState(() {});
  }

  double _parsePercent(dynamic value) {
    if (value is int) return value.toDouble();
    if (value is double) return value;
    if (value is String) return double.tryParse(value) ?? 0;
    return 0;
  }

  List<dynamic> getEncontrados() {
    var encontrados = bleController.emocoes['encontrados'];
    if (encontrados is List) return encontrados;
    return [];
  }

  bool get isLoading {
    final status = bleController.status?.toLowerCase() ?? '';
    return status.contains("conect") ||
        status.contains("buscando") ||
        bleController.emocoes.isEmpty;
  }

  @override
  Widget build(BuildContext context) {
    final bgColor = Color(0xFFFFF7ED);

    return Scaffold(
      backgroundColor: bgColor,
      appBar: AppBar(
        elevation: 0,
        title: Row(
          children: [
            Icon(Icons.wb_sunny_rounded, color: Colors.orangeAccent, size: 32),
            SizedBox(width: 10),
            Text("DeskBuddy", style: TextStyle(color: Colors.orange[800], fontWeight: FontWeight.bold)),
          ],
        ),
        backgroundColor: bgColor,
        iconTheme: IconThemeData(color: Colors.orange[800]),
        actions: [
          Padding(
            padding: const EdgeInsets.all(10.0),
            child: CircleAvatar(
              backgroundColor: Colors.orange.shade200,
              child: Icon(Icons.person, color: Colors.orange[900]),
            ),
          ),
        ],
      ),
      drawer: Drawer(
        child: ListView(
          padding: EdgeInsets.zero,
          children: [
            DrawerHeader(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [Colors.orange.shade200, Colors.orange.shade400],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
              ),
              child: Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    CircleAvatar(
                      backgroundColor: Colors.orange[100],
                      radius: 32,
                      child: Icon(Icons.wb_sunny_rounded, color: Colors.orange[800], size: 40),
                    ),
                    SizedBox(height: 12),
                    Text('Meu Buddy', style: TextStyle(color: Colors.white, fontSize: 24)),
                  ],
                ),
              ),
            ),
            ListTile(
              leading: Icon(Icons.home, color: Colors.orange),
              title: Text('Início'),
              onTap: () => Navigator.pop(context),
            ),
            ListTile(
              leading: Icon(Icons.heart_broken, color: Colors.orange),
              title: Text('Relacionamentos'),
              onTap: () => Navigator.pop(context),
            ),
            ListTile(
              leading: Icon(Icons.search, color: Colors.orange),
              title: Text('Buddys encontrados'),
              onTap: () {
                Navigator.pop(context);
                Navigator.push(
                  context,
                  MaterialPageRoute(
                    builder: (context) => BuddysEncontradosPage(
                      encontrados: getEncontrados(),
                    ),
                  ),
                );
              },
            ),
            ListTile(
              leading: Icon(Icons.settings, color: Colors.orange),
              title: Text('Configurações'),
              onTap: () => Navigator.pop(context),
            ),
          ],
        ),
      ),
      body: Padding(
        padding: EdgeInsets.all(20),
        child: (_showRetry && bleController.emocoes.isEmpty)
            ? Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(Icons.search_off, size: 70, color: Colors.orange[300]),
              SizedBox(height: 18),
              Text(
                _reloadCount >= 3
                    ? "Não foi possível conectar ao DeskBuddy.\n\nFeche e reabra o aplicativo para tentar novamente."
                    : "Não foi possível encontrar o DeskBuddy.",
                style: TextStyle(
                  fontSize: 20,
                  color: Colors.orange[900],
                  fontWeight: FontWeight.bold,
                ),
                textAlign: TextAlign.center,
              ),
              SizedBox(height: 16),
              if (_reloadCount < 3)
                ElevatedButton.icon(
                  icon: Icon(Icons.refresh),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.orange,
                    foregroundColor: Colors.white,
                    minimumSize: Size(170, 48),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
                    textStyle: TextStyle(fontWeight: FontWeight.bold),
                  ),
                  onPressed: _reload,
                  label: Text("Tentar novamente"),
                ),
              if (_reloadCount >= 3)
                ElevatedButton.icon(
                  icon: Icon(Icons.restart_alt),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.orange.shade700,
                    foregroundColor: Colors.white,
                    minimumSize: Size(170, 48),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
                    textStyle: TextStyle(fontWeight: FontWeight.bold),
                  ),
                  onPressed: () {
                    Phoenix.rebirth(context); // <-- Reinicia o app!
                  },
                  label: Text("Reiniciar aplicativo"),
                ),
            ],
          ),
        )
            : isLoading
            ? Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              CircularProgressIndicator(
                valueColor: AlwaysStoppedAnimation<Color>(Colors.orange),
              ),
              SizedBox(height: 20),
              Text(
                "Carregando dados do DeskBuddy...",
                style: TextStyle(fontSize: 18, color: Colors.orange[800]),
              ),
            ],
          ),
        )
            : SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Status Card
              Container(
                height: 230,
                width: double.infinity,
                child: Card(
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(28),
                  ),
                  color: Colors.white,
                  elevation: 4,
                  child: Padding(
                    padding: const EdgeInsets.all(20.0),
                    child: Stack(
                      alignment: Alignment.center,
                      children: [
                        // Imagem de fundo
                        Image.asset(
                          'assets/${bleController.emocoes['dominante']}.gif',
                          width: 128,
                          height: 73,
                          fit: BoxFit.cover,
                        ),
                        // Imagem principal do DeskBuddy
                        Image.asset(
                          'assets/images/DeskBuddy.png',
                          width: double.infinity,
                          height: double.infinity,
                          fit: BoxFit.contain,
                        ),
                        // Nome do DeskBuddy CENTRALIZADO NO TOPO
                        Align(
                          alignment: Alignment.topCenter,
                          child: Text(
                            capitalize(bleController.emocoes['nome']?.toString() ?? 'DeskBuddy'),
                            style: TextStyle(
                              color: Colors.orange[800],
                              fontWeight: FontWeight.bold,
                              fontSize: 23,
                              letterSpacing: 1.1,
                            ),
                            overflow: TextOverflow.ellipsis,
                            textAlign: TextAlign.center,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
              SizedBox(height: 22),
              if (bleController.emocoes.isNotEmpty) ...[
                if (bleController.emocoes.containsKey('dominante'))
                  Card(
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
                    color: Colors.orange.shade100,
                    elevation: 2,
                    child: Padding(
                      padding: const EdgeInsets.symmetric(vertical: 18.0, horizontal: 16),
                      child: Row(
                        children: [
                          Icon(Icons.mood, size: 38, color: Colors.orange[700]),
                          SizedBox(width: 12),
                          Text(
                            "Emoção: ",
                            style: TextStyle(fontSize: 21, fontWeight: FontWeight.bold, color: Colors.orange[700]),
                          ),
                          Expanded(
                            child: Text(
                              capitalize(bleController.emocoes['dominante'].toString()),
                              style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold, color: Colors.orange[900]),
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                SizedBox(height: 24),
                // Barras de emoções (exceto encontrados e dominante)
                ...bleController.emocoes.entries
                    .where((e) => e.key != 'dominante' && e.key != 'encontrados' && e.key != 'nome')
                    .map((e) {
                  final percentValue = _parsePercent(e.value);
                  return Padding(
                    padding: const EdgeInsets.symmetric(vertical: 8),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Padding(
                          padding: const EdgeInsets.only(left: 4, bottom: 6),
                          child: Text(
                            capitalize(e.key),
                            style: TextStyle(
                              color: Colors.orange.shade900,
                              fontSize: 17,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ),
                        RoundedProgressBar(
                          key: ValueKey(percentValue),
                          height: 45,
                          style: RoundedProgressBarStyle(
                            colorProgress: (percentValue < 3)
                                ? Colors.transparent
                                : Colors.orange,
                            colorProgressDark: (percentValue < 6)
                                ? Colors.transparent
                                : Colors.deepOrange,
                            colorBorder: Colors.orange.shade200,
                            backgroundProgress: Colors.orange.shade50,
                            borderWidth: 2,
                          ),
                          percent: percentValue,
                          childLeft: Padding(
                            padding: const EdgeInsets.only(left: 10),
                            child: Text(
                              "${percentValue.toInt()}%",
                              style: TextStyle(
                                color: Colors.black,
                                fontSize: 13,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ),
                        ),
                      ],
                    ),
                  );
                }).toList(),
              ],
              SizedBox(height: 10),
            ],
          ),
        ),
      ),
    );
  }

  String capitalize(String s) {
    if (s.isEmpty) return s;
    return s[0].toUpperCase() + s.substring(1);
  }
}
