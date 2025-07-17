// lib/pages/desk_buddy_home_page.dart
import 'package:deskbuddy/bleController.dart';
import 'package:flutter/material.dart';

class DeskBuddyHomePage extends StatefulWidget {
  @override
  _DeskBuddyHomePageState createState() => _DeskBuddyHomePageState();
}

class _DeskBuddyHomePageState extends State<DeskBuddyHomePage> {
  final BleController bleController = BleController();

  @override
  void initState() {
    super.initState();
    bleController.requestPermissions();
  }

  @override
  void dispose() {
    bleController.dispose();
    super.dispose();
  }

  void updateState() {
    if (mounted) setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("DeskBuddy BLE")),
      body: Padding(
        padding: EdgeInsets.all(20),
        child: Column(
          children: [
            Text("Status: ${bleController.status}", style: TextStyle(fontSize: 18)),
            SizedBox(height: 25),
            if (bleController.emocoes.isEmpty)
              Text("Nenhuma emoção recebida ainda.", style: TextStyle(fontSize: 18)),
            if (bleController.emocoes.isNotEmpty)
              ...[
                if (bleController.emocoes.containsKey('dominante'))
                  Padding(
                    padding: EdgeInsets.only(bottom: 16),
                    child: Row(
                      children: [
                        Text(
                          "Emoção no display: ",
                          style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold, color: Colors.blueAccent),
                        ),
                        Text(
                          capitalize(bleController.emocoes['dominante'].toString()),
                          style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold, color: Colors.deepPurple),
                        ),
                      ],
                    ),
                  ),
                ...bleController.emocoes.entries
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
                  onPressed: bleController.running ? null : () {
                    bleController.startAutoUpdate(updateState);
                    setState(() {});
                  },
                  icon: Icon(Icons.play_arrow),
                  label: Text("Iniciar atualização"),
                  style: ElevatedButton.styleFrom(minimumSize: Size(170, 48)),
                ),
                SizedBox(width: 20),
                ElevatedButton.icon(
                  onPressed: bleController.running ? () {
                    bleController.stopAutoUpdate();
                    setState(() {});
                  } : null,
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
