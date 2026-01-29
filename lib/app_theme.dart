import 'package:flutter/material.dart';

/// Paleta principal (off-white + laranja) e tema Material 3.
import 'package:flutter/material.dart';

class AppColors {
  // Marca
  static const Color orange = Color(0xFFFF7A02);

  // Base (Light)
  static const Color bg = Color(0xFFFFF7ED);
  static const Color bg2 = Color(0xFFFFFBF5);
  static const Color line = Color(0xFFF1E2D2);

  // Superfícies
  static const Color surface = Color(0xFFFFFFFF); // cards/dialogs
  static const Color surface2 = Color(0xFFFFFBF5); // sections
  static const Color surface3 = Color(0xFFFFF3E3); // highlights suaves

  // Texto (Light)
  static const Color text = Color(0xFF1B1B1B);
  static const Color text2 = Color(0xFF2A2A2A);
  static const Color muted = Color(0xFF6B6B6B);
  static const Color muted2 = Color(0xFF8A8A8A);

  // Estados
  static const Color good = Color(0xFF2E7D32);
  static const Color warn = Color(0xFFB26A00);
  static const Color bad = Color(0xFFC62828);
  static const Color info = Color(0xFF1565C0);

  // Overlays / sombras (use com opacidade)
  static const Color shadow = Color(0xFF000000);
  static const Color scrim = Color(0xFF000000);

  // ===== Dark tokens (opcional, mas recomendado) =====
  static const Color bgDark = Color(0xFF121212);
  static const Color bg2Dark = Color(0xFF171717);
  static const Color lineDark = Color(0xFF2A2A2A);

  static const Color surfaceDark = Color(0xFF1A1A1A);
  static const Color surface2Dark = Color(0xFF202020);
  static const Color surface3Dark = Color(0xFF262626);

  static const Color textDark = Color(0xFFF2F2F2);
  static const Color mutedDark = Color(0xFFB8B8B8);

  static ColorScheme scheme(Brightness brightness) {
    final base = ColorScheme.fromSeed(
      seedColor: orange,
      brightness: brightness,
    );

    final isLight = brightness == Brightness.light;

    // Ajustes pra manter o look off-white / dark clean
    return base.copyWith(
      primary: orange,
      onPrimary: Colors.white,

      // fundo e superfícies
      background: isLight ? bg : bgDark,
      onBackground: isLight ? text : textDark,

      surface: isLight ? surface : surfaceDark,
      onSurface: isLight ? text : textDark,

      surfaceContainerHighest: isLight ? surface2 : surface2Dark,
      surfaceContainer: isLight ? surface3 : surface3Dark,

      outline: isLight ? line : lineDark,
      outlineVariant: isLight ? line.withOpacity(.6) : lineDark.withOpacity(.7),

      // estados (usa os seus fixos)
      error: bad,
      onError: Colors.white,

      secondary: isLight ? orange.withOpacity(.85) : base.secondary,
      onSecondary: Colors.white,
    );
  }

  // Helpers de uso rápido sem repetir opacity no app
  static Color overlay(Color c, double opacity) => c.withOpacity(opacity);

  static Color divider(Brightness b) => b == Brightness.light ? line : lineDark;
  static Color card(Brightness b) => b == Brightness.light ? surface : surfaceDark;
  static Color section(Brightness b) => b == Brightness.light ? surface2 : surface2Dark;
  static Color textPrimary(Brightness b) => b == Brightness.light ? text : textDark;
  static Color textMuted(Brightness b) => b == Brightness.light ? muted : mutedDark;
}

class AppTheme {
  static ThemeData light() {
    final cs = AppColors.scheme(Brightness.light);
    return ThemeData(
      useMaterial3: true,
      colorScheme: cs,
      scaffoldBackgroundColor: AppColors.bg,
      appBarTheme: AppBarTheme(
        backgroundColor: AppColors.bg,
        foregroundColor: AppColors.text,
        elevation: 0,
        centerTitle: false,
        titleTextStyle: const TextStyle(
          fontSize: 20,
          fontWeight: FontWeight.w800,
          color: AppColors.text,
          letterSpacing: 0.2,
        ),
      ),
      cardTheme: const CardThemeData(
        color: Colors.white,
        elevation: 0,
        margin: EdgeInsets.zero,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(18)),
        ),
      ),

      dividerTheme: const DividerThemeData(color: AppColors.line, thickness: 1, space: 1),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: cs.inverseSurface,
        contentTextStyle: TextStyle(color: cs.onInverseSurface),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: Colors.white,
        hintStyle: const TextStyle(color: AppColors.muted),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(18),
          borderSide: BorderSide(color: AppColors.line),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(18),
          borderSide: const BorderSide(color: AppColors.line),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(18),
          borderSide: BorderSide(color: cs.primary, width: 2),
        ),
      ),
    );
  }

  static ThemeData dark() {
    final cs = AppColors.scheme(Brightness.dark);
    return ThemeData(
      useMaterial3: true,
      colorScheme: cs,
      scaffoldBackgroundColor: cs.background,
    );
  }
}
