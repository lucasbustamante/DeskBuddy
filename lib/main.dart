import 'package:deskbuddy/deskBuddyHomePage2.dart';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'login_page.dart';
import 'dart:async';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: const SplashScreen(), // 🔹 Primeiro mostra a Splash
    );
  }
}

/// ✅ SplashScreen personalizada (aparece logo após a nativa)
class SplashScreen extends StatefulWidget {
  const SplashScreen({Key? key}) : super(key: key);

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen> {
  @override
  void initState() {
    super.initState();
    Timer(const Duration(seconds: 2), _checkLogin);
  }

  Future<void> _checkLogin() async {
    final prefs = await SharedPreferences.getInstance();
    final isLoggedIn = prefs.getString('deskbuddy_senha') != null;

    if (!mounted) return;

    Navigator.pushReplacement(
      context,
      MaterialPageRoute(
        builder: (_) =>
        isLoggedIn
            ? DeskBuddyHomePage2()
            : LoginPage(
          onLoginSuccess: (ctx, name, password) {
            Navigator.pushReplacement(
              ctx,
              MaterialPageRoute(builder: (_) => DeskBuddyHomePage2()),
            );
          },
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    var bgColor;
    return Scaffold(
      backgroundColor: Colors.orange.shade50,
      body: Center(
        child: Image.asset(
          'assets/logo.png',
          width: 350,
          height: 350,
        ),
      ),
    );
  }
}
