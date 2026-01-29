import 'package:deskbuddy/deskBuddyHomePage2.dart';
import 'package:deskbuddy/login_page.dart';
import 'package:flutter/material.dart';
import 'package:deskbuddy/app_theme.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// 🔹 WIDGET que permite reiniciar o app do zero
class RestartWidget extends StatefulWidget {
  final Widget Function() builder;
  const RestartWidget({Key? key, required this.builder}) : super(key: key);

  static void restartApp(BuildContext context) {
    context.findAncestorStateOfType<_RestartWidgetState>()?.restartApp();
  }

  @override
  _RestartWidgetState createState() => _RestartWidgetState();
}

class _RestartWidgetState extends State<RestartWidget> {
  Key key = UniqueKey();

  void restartApp() {
    setState(() {
      key = UniqueKey(); // 🔥 força reconstrução total do app
    });
  }

  
  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      backgroundColor: AppColors.bg,
      appBar: AppBar(
        title: const Text('Configurações'),
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 10, 16, 16),
        children: [
          Container(
            padding: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: Colors.white,
              borderRadius: BorderRadius.circular(22),
              border: Border.all(color: AppColors.line),
            ),
            child: Row(
              children: [
                Container(
                  width: 44,
                  height: 44,
                  decoration: BoxDecoration(
                    color: cs.primary.withOpacity(.12),
                    borderRadius: BorderRadius.circular(16),
                  ),
                  child: Icon(Icons.settings_rounded, color: cs.primary),
                ),
                const SizedBox(width: 12),
                const Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text("Sessão", style: TextStyle(fontWeight: FontWeight.w900, fontSize: 16)),
                      SizedBox(height: 2),
                      Text("Gerencie login e conexão", style: TextStyle(color: AppColors.muted)),
                    ],
                  ),
                )
              ],
            ),
          ),
          const SizedBox(height: 12),
          Container(
            padding: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: Colors.white,
              borderRadius: BorderRadius.circular(22),
              border: Border.all(color: AppColors.line),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  "DeskBuddy",
                  style: TextStyle(fontWeight: FontWeight.w900, fontSize: 16),
                ),
                const SizedBox(height: 6),
                const Text(
                  "Você pode desconectar deste buddy para trocar de dispositivo ou redefinir os dados de conexão.",
                  style: TextStyle(color: AppColors.muted),
                ),
                const SizedBox(height: 14),
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton.icon(
                    icon: const Icon(Icons.logout_rounded, color: Colors.white),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: cs.primary,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
                    ),
                    onPressed: () async {
                      final confirm = await showDialog<bool>(
                        context: context,
                        builder: (context) => AlertDialog(
                          title: const Text("Desconectar do Buddy"),
                          content: const Text(
                            "Tem certeza que deseja sair deste DeskBuddy?"
                            "Você precisará informar a senha novamente na próxima vez.",
                          ),
                          actions: [
                            TextButton(
                              child: const Text("Cancelar"),
                              onPressed: () => Navigator.pop(context, false),
                            ),
                            TextButton(
                              child: const Text("Sair"),
                              onPressed: () => Navigator.pop(context, true),
                            ),
                          ],
                        ),
                      );

                      if (confirm == true) {
                        await Settings()._restartApp(context);
                      }
                    },
                    label: const Text("Desconectar", style: TextStyle(fontWeight: FontWeight.w900)),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

}

/// 🔹 APP PRINCIPAL
void main() {
  runApp(RestartWidget(builder: () => const MyApp()));
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: LoginPage(
        // ✅ sempre usa o contexto atual do MaterialApp reconstruído
        onLoginSuccess: (ctx, name, password) {
          Navigator.of(ctx).pushReplacement(
            MaterialPageRoute(builder: (_) =>  DeskBuddyHomePage2()),
          );
        },
      ),
    );
  }
}

/// 🔹 SETTINGS COM REINÍCIO COMPLETO NO "SAIR"
class Settings extends StatelessWidget {
  const Settings({Key? key}) : super(key: key);

  /// ✅ Função que limpa os dados e reinicia o app carregando o Login
  Future<void> _restartApp(BuildContext context) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('deskbuddy_senha');

    // ✅ Garante que a LoginPage seja carregada como nova raiz
    Navigator.of(context).pushAndRemoveUntil(
      MaterialPageRoute(
        builder: (context) => LoginPage(
          onLoginSuccess: (ctx, name, password) {
            Navigator.of(ctx).pushReplacement(
              MaterialPageRoute(
                builder: (context) => DeskBuddyHomePage2(),
              ),
            );
          },
        ),
      ),
          (route) => false,
    );


    // ✅ Após navegar, força reconstrução da árvore do app
    Future.delayed(const Duration(milliseconds: 100), () {
      RestartWidget.restartApp(context);
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text(
          'Configurações',
          style: TextStyle(color: Colors.orange),
        ),
        backgroundColor: Colors.orange[50],
        iconTheme: const IconThemeData(color: Colors.orange),
        elevation: 1,
      ),
      body: ListView(
        padding: const EdgeInsets.all(24),
        children: [
          Card(
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(18),
            ),
            color: Colors.orange[50],
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    "DeskBuddy",
                    style: TextStyle(
                        fontSize: 21,
                        fontWeight: FontWeight.bold,
                        color: Colors.orange),
                  ),
                  const SizedBox(height: 12),
                  const Text(
                    "Gerencie as configurações do seu DeskBuddy.\n"
                        "Você pode desconectar deste Buddy para trocar de dispositivo ou redefinir os dados de conexão.",
                    style: TextStyle(fontSize: 16),
                  ),
                  const SizedBox(height: 28),
                  ElevatedButton.icon(
                    icon: const Icon(Icons.logout, color: Colors.white),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.deepOrange,
                      minimumSize: const Size(180, 50),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(14),
                      ),
                    ),
                    onPressed: () async {
                      final confirm = await showDialog(
                        context: context,
                        builder: (context) => AlertDialog(
                          title: const Text("Desconectar do Buddy"),
                          content: const Text(
                              "Tem certeza que deseja sair deste DeskBuddy? "
                                  "Será necessário informar a senha na próxima vez."),
                          actions: [
                            TextButton(
                              child: const Text("Cancelar"),
                              onPressed: () => Navigator.pop(context, false),
                            ),
                            TextButton(
                              child: const Text("Sair",
                                  style: TextStyle(color: Colors.red)),
                              onPressed: () => Navigator.pop(context, true),
                            ),
                          ],
                        ),
                      );
                      if (confirm == true) {
                        await _restartApp(context); // ✅ desconecta e reinicia
                      }
                    },
                    label: const Text(
                      "Desconectar do Buddy",
                      style: TextStyle(
                          fontWeight: FontWeight.bold, color: Colors.white),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
