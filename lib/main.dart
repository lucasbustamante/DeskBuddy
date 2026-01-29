import 'dart:async';

import 'package:deskbuddy/app_theme.dart';
import 'package:deskbuddy/deskBuddyHomePage2.dart';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'login_page.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: AppTheme.light(),
      darkTheme: AppTheme.dark(),
      home: const SplashScreen(),
    );
  }
}

/// ✅ SplashScreen (aparece logo após a nativa)
class SplashScreen extends StatefulWidget {
  const SplashScreen({Key? key}) : super(key: key);

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen> {
  @override
  void initState() {
    super.initState();
    Timer(const Duration(milliseconds: 1200), _checkLogin);
  }

  Future<void> _checkLogin() async {
    final prefs = await SharedPreferences.getInstance();
    final isLoggedIn = prefs.getString('deskbuddy_senha') != null;

    if (!mounted) return;

    Navigator.pushReplacement(
      context,
      PageRouteBuilder(
        pageBuilder: (_, __, ___) => isLoggedIn
            ? DeskBuddyHomePage2()
            : LoginPage(
                onLoginSuccess: (ctx, name, password) {
                  Navigator.pushReplacement(
                    ctx,
                    MaterialPageRoute(builder: (_) => DeskBuddyHomePage2()),
                  );
                },
              ),
        transitionsBuilder: (_, animation, __, child) {
          final curved = CurvedAnimation(parent: animation, curve: Curves.easeOutCubic);
          return FadeTransition(
            opacity: curved,
            child: SlideTransition(
              position: Tween<Offset>(begin: const Offset(0, .03), end: Offset.zero).animate(curved),
              child: child,
            ),
          );
        },
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      backgroundColor: AppColors.bg,
      body: Center(
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 26, vertical: 24),
          decoration: BoxDecoration(
            color: Colors.white,
            borderRadius: BorderRadius.circular(28),
            border: Border.all(color: AppColors.line),
            boxShadow: const [
              BoxShadow(
                blurRadius: 30,
                spreadRadius: 0,
                offset: Offset(0, 14),
                color: Color(0x14000000),
              ),
            ],
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Image.asset('assets/logo.png', width: 190, height: 190),
              const SizedBox(height: 14),
              Text(
                "DeskBuddy",
                style: TextStyle(
                  fontSize: 22,
                  fontWeight: FontWeight.w900,
                  color: cs.primary,
                  letterSpacing: .2,
                ),
              ),
              const SizedBox(height: 8),
              const Text(
                "Conectando e sincronizando emoções…",
                style: TextStyle(color: AppColors.muted),
              ),
              const SizedBox(height: 18),
              SizedBox(
                width: 34,
                height: 34,
                child: CircularProgressIndicator(strokeWidth: 3, color: cs.primary),
              )
            ],
          ),
        ),
      ),
    );
  }
}
