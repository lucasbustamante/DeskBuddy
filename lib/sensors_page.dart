import 'dart:convert';

import 'package:deskbuddy/app_theme.dart';
import 'package:flutter/material.dart';

class SensorsPage extends StatelessWidget {
  final Map<String, dynamic> emocoes;
  final String bleStatusText;

  const SensorsPage({
    super.key,
    required this.emocoes,
    required this.bleStatusText,
  });

  bool _asBool(dynamic v) {
    if (v == null) return false;
    if (v is bool) return v;
    final s = v.toString().trim().toLowerCase();
    return s == 'true' || s == '1' || s == 'on' || s == 'ligado' || s == 'yes';
  }

  String _v(dynamic v, {String dash = "—"}) {
    if (v == null) return dash;
    final s = v.toString().trim();
    return s.isEmpty ? dash : s;
  }

  dynamic _pick(List<String> keys) {
    for (final k in keys) {
      if (emocoes.containsKey(k)) return emocoes[k];
    }
    return null;
  }

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    // ---- Bateria
    final batteryPct = _pick(const ['bateria_pct', 'battery_pct', 'bat_pct']);
    final batteryV = _pick(const ['bateria_v', 'battery_v', 'bat_v', 'bateria_volt', 'battery_v']);
    final batteryLow = _asBool(_pick(const ['bateria_low', 'battery_low', 'bat_low']));

    // ---- LDR
    final ldrMv = _pick(const ['ldr_mv', 'ldr', 'ldr_mV', 'ldr_mv_value']);

    // ---- Bluetooth (status do rádio / funcionalidade)
    final btOn = _pick(const ['bluetooth_on', 'bt_on', 'bluetooth_ligado', 'bt_ligado']);
    final btOk = _pick(const ['bluetooth_ok', 'bt_ok', 'ble_ok', 'bluetooth_funcional', 'bt_funcional']);
    final bleApp = _pick(const ['ble_app_conectado', 'ble_app', 'app_ble']);

    // ---- MPU/IMU
    final mpuOn = _pick(const ['mpu_on', 'mpu_ligado', 'imu_on', 'imu_ligado']);
    final mpuOk = _pick(const ['mpu_ok', 'imu_ok', 'mpu_funcional', 'imu_funcional', 'mpu_connected', 'imu_connected']);

    return Scaffold(
      appBar: AppBar(
        title: Row(
          children: [
            Container(
              width: 36,
              height: 36,
              decoration: BoxDecoration(
                color: cs.primary.withOpacity(.12),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: cs.primary.withOpacity(.25)),
              ),
              child: Icon(Icons.memory_rounded, color: cs.primary),
            ),
            const SizedBox(width: 10),
            const Text("Sensores"),
            const Spacer(),
            _StatusPill(text: bleStatusText),
          ],
        ),
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 14, 16, 24),
          children: [
            _SectionHeader(
              icon: Icons.developer_board_rounded,
              title: "Área de desenvolvimento",
              subtitle: "Diagnóstico rápido + debug",
            ),
            const SizedBox(height: 10),
            _SensorGrid(
              children: [
                _SensorCard(
                  icon: Icons.battery_full_rounded,
                  title: "Bateria",
                  status: batteryLow ? _SensorStatus.warn : _SensorStatus.ok,
                  lines: [
                    "Nível: ${batteryPct != null ? "${_v(batteryPct)}%" : "—"}",
                    "Tensão: ${batteryV != null ? "${_v(batteryV)} V" : "—"}",
                  ],
                ),
                _SensorCard(
                  icon: Icons.light_mode_rounded,
                  title: "LDR",
                  status: ldrMv != null ? _SensorStatus.ok : _SensorStatus.unknown,
                  lines: [
                    "Leitura: ${ldrMv != null ? "${_v(ldrMv)} mV" : "—"}",
                    "Luminosidade ambiente",
                  ],
                ),
                _SensorCard(
                  icon: Icons.bluetooth_rounded,
                  title: "Bluetooth",
                  status: _asBool(btOk) ? _SensorStatus.ok : (btOk == null ? _SensorStatus.unknown : _SensorStatus.bad),
                  lines: [
                    "Rádio: ${btOn == null ? "—" : (_asBool(btOn) ? "Ligado" : "Desligado")}",
                    "Funcional: ${btOk == null ? "—" : (_asBool(btOk) ? "Sim" : "Não")}",
                    "App conectado: ${bleApp == null ? "—" : (_asBool(bleApp) ? "Sim" : "Não")}",
                  ],
                ),
                _SensorCard(
                  icon: Icons.sensors_rounded,
                  title: "MPU / IMU",
                  status: _asBool(mpuOk) ? _SensorStatus.ok : (mpuOk == null ? _SensorStatus.unknown : _SensorStatus.bad),
                  lines: [
                    "Conectado: ${mpuOn == null ? "—" : (_asBool(mpuOn) ? "Sim" : "Não")}",
                    "Funcional: ${mpuOk == null ? "—" : (_asBool(mpuOk) ? "Sim" : "Não")}",
                  ],
                ),
              ],
            ),
            const SizedBox(height: 14),
            _JsonDebugCard(emocoes: emocoes),
          ],
        ),
      ),
    );
  }
}

// -----------------------------------------------------------------------------
// UI helpers
// -----------------------------------------------------------------------------

class _SectionHeader extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;

  const _SectionHeader({
    required this.icon,
    required this.title,
    required this.subtitle,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AppColors.bg2,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: AppColors.line),
      ),
      child: Row(
        children: [
          Container(
            width: 42,
            height: 42,
            decoration: BoxDecoration(
              color: cs.primary.withOpacity(.14),
              borderRadius: BorderRadius.circular(14),
            ),
            child: Icon(icon, color: cs.primary),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title, style: const TextStyle(fontWeight: FontWeight.w900, fontSize: 16)),
                const SizedBox(height: 2),
                Text(subtitle, style: const TextStyle(color: AppColors.muted)),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

enum _SensorStatus { ok, warn, bad, unknown }

class _SensorGrid extends StatelessWidget {
  final List<Widget> children;
  const _SensorGrid({required this.children});

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, c) {
        final w = c.maxWidth;

        // 1 coluna no celular / 2 colunas em telas maiores
        final isWide = w >= 680;
        final cardWidth = isWide ? (w - 10) / 2 : w;

        return Wrap(
          spacing: 10,
          runSpacing: 10,
          children: children
              .map((child) => SizedBox(width: cardWidth, child: child))
              .toList(),
        );
      },
    );
  }
}

class _SensorCard extends StatelessWidget {
  final IconData icon;
  final String title;
  final _SensorStatus status;
  final List<String> lines;

  const _SensorCard({
    required this.icon,
    required this.title,
    required this.status,
    required this.lines,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    Color dot = cs.primary;
    String label = "OK";
    if (status == _SensorStatus.warn) {
      dot = AppColors.warn;
      label = "Atenção";
    } else if (status == _SensorStatus.bad) {
      dot = AppColors.bad;
      label = "Erro";
    } else if (status == _SensorStatus.unknown) {
      dot = AppColors.muted2;
      label = "—";
    }

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: AppColors.line),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                width: 36,
                height: 36,
                decoration: BoxDecoration(
                  color: cs.primary.withOpacity(.12),
                  borderRadius: BorderRadius.circular(14),
                ),
                child: Icon(icon, color: cs.primary, size: 20),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: Text(title, style: const TextStyle(fontWeight: FontWeight.w900)),
              ),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(999),
                  color: dot.withOpacity(.12),
                  border: Border.all(color: dot.withOpacity(.35)),
                ),
                child: Row(
                  children: [
                    Container(width: 8, height: 8, decoration: BoxDecoration(color: dot, shape: BoxShape.circle)),
                    const SizedBox(width: 8),
                    Text(label, style: TextStyle(fontWeight: FontWeight.w900, color: dot)),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          ...lines.map((t) => Padding(
                padding: const EdgeInsets.only(bottom: 6),
                child: Text(t, style: const TextStyle(color: AppColors.text2, fontWeight: FontWeight.w600)),
              )),
        ],
      ),
    );
  }
}

class _JsonDebugCard extends StatelessWidget {
  final Map<String, dynamic> emocoes;
  const _JsonDebugCard({required this.emocoes});

  @override
  Widget build(BuildContext context) {
    final sanitized = Map<String, dynamic>.from(emocoes);
    if (sanitized.containsKey('senha')) sanitized['senha'] = '••••';

    final cs = Theme.of(context).colorScheme;
    return ExpansionTile(
      tilePadding: const EdgeInsets.symmetric(horizontal: 14),
      collapsedBackgroundColor: Colors.white,
      backgroundColor: Colors.white,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22), side: const BorderSide(color: AppColors.line)),
      collapsedShape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22), side: const BorderSide(color: AppColors.line)),
      title: const Text("Avançado", style: TextStyle(fontWeight: FontWeight.w900)),
      subtitle: const Text("JSON completo do DeskBuddy", style: TextStyle(color: AppColors.muted)),
      leading: Container(
        width: 36,
        height: 36,
        decoration: BoxDecoration(
          color: cs.primary.withOpacity(.12),
          borderRadius: BorderRadius.circular(14),
        ),
        child: Icon(Icons.data_object_rounded, color: cs.primary, size: 20),
      ),
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(14, 0, 14, 14),
          child: Container(
            width: double.infinity,
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: AppColors.bg2,
              borderRadius: BorderRadius.circular(18),
              border: Border.all(color: AppColors.line),
            ),
            child: Text(
              const JsonEncoder.withIndent("  ").convert(sanitized),
              style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
            ),
          ),
        ),
      ],
    );
  }
}

// Reaproveitando o pill visual da home
class _StatusPill extends StatelessWidget {
  final String text;
  const _StatusPill({required this.text});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final t = (text).trim().isEmpty ? "—" : text;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: cs.primary.withOpacity(.10),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: cs.primary.withOpacity(.25)),
      ),
      child: Text(
        t,
        style: TextStyle(fontWeight: FontWeight.w900, color: cs.primary),
        overflow: TextOverflow.ellipsis,
      ),
    );
  }
}
