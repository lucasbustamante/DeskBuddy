import 'dart:convert';

import 'package:deskbuddy/app_theme.dart';
import 'package:deskbuddy/bleController.dart';
import 'package:deskbuddy/settings.dart';
import 'package:flutter/material.dart';
import 'package:flutter_phoenix/flutter_phoenix.dart';

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
    setState(() => _showRetry = false);

    bleController.startAutoUpdate(updateState);

    Future.delayed(timeout, () {
      if (mounted && bleController.emocoes.isEmpty) {
        setState(() => _showRetry = true);
      }
    });
  }

  Future<void> _reload() async {
    setState(() => _reloadCount++);

    if (_reloadCount >= 3) {
      setState(() => _showRetry = true);
      return;
    }

    bleController.stopAutoUpdate();
    bleController.emocoes.clear();
    _startScan();
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
    final encontrados = bleController.emocoes['encontrados'];
    if (encontrados is List) return encontrados;
    return [];
  }

  bool get isLoading {
    final status = (bleController.status).toLowerCase();
    return status.contains("conect") ||
        status.contains("buscando") ||
        bleController.emocoes.isEmpty;
  }

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      backgroundColor: AppColors.bg,
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
              child: Icon(Icons.wb_sunny_rounded, color: cs.primary),
            ),
            const SizedBox(width: 10),
            const Text("DeskBuddy"),
            const Spacer(),
            _StatusPill(text: bleController.status),
          ],
        ),
      ),
      drawer: _AppDrawer(
        onOpenFound: () {
          Navigator.pop(context);
          Navigator.push(
            context,
            MaterialPageRoute(
              builder: (context) => BuddysEncontradosPage(encontrados: getEncontrados()),
            ),
          );
        },
        onOpenSettings: () {
          Navigator.pop(context);
          Navigator.push(context, MaterialPageRoute(builder: (_) => const Settings()));
        },
      ),
      body: SafeArea(
        child: (_showRetry && bleController.emocoes.isEmpty)
            ? _RetryState(
                reloadCount: _reloadCount,
                onRetry: _reloadCount < 3 ? _reload : null,
                onRestart: () => Phoenix.rebirth(context),
              )
            : isLoading
                ? _LoadingState(status: bleController.status)
                : RefreshIndicator(
                    color: cs.primary,
                    onRefresh: _reload,
                    child: CustomScrollView(
                      physics: const AlwaysScrollableScrollPhysics(),
                      slivers: [
                        SliverPadding(
                          padding: const EdgeInsets.fromLTRB(16, 10, 16, 16),
                          sliver: SliverList(
                            delegate: SliverChildListDelegate([
                              _BuddyHeroCard(emocoes: bleController.emocoes),
                              const SizedBox(height: 14),
                              _QuickStatsRow(emocoes: bleController.emocoes),
                              const SizedBox(height: 14),
                              _DominantEmotionCard(emocoes: bleController.emocoes),
                              const SizedBox(height: 14),
                              _EmotionDistribution(
                                emocoes: bleController.emocoes,
                                parsePercent: _parsePercent,
                              ),
                              const SizedBox(height: 14),
                              _ToolsCard(
                                onOpenFound: () {
                                  Navigator.push(
                                    context,
                                    MaterialPageRoute(
                                      builder: (_) => BuddysEncontradosPage(encontrados: getEncontrados()),
                                    ),
                                  );
                                },
                                onOpenSettings: () {
                                  Navigator.push(context, MaterialPageRoute(builder: (_) => const Settings()));
                                },
                              ),
                              const SizedBox(height: 14),
                              _AdvancedCard(emocoes: bleController.emocoes),
                              const SizedBox(height: 90),
                            ]),
                          ),
                        ),
                      ],
                    ),
                  ),
      ),
    );
  }
}

// -----------------------------------------------------------------------------
// UI Widgets
// -----------------------------------------------------------------------------

class _AppDrawer extends StatelessWidget {
  final VoidCallback onOpenFound;
  final VoidCallback onOpenSettings;

  const _AppDrawer({
    required this.onOpenFound,
    required this.onOpenSettings,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return Drawer(
      child: SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(14, 10, 14, 12),
          child: Column(
            children: [
              Container(
                width: double.infinity,
                padding: const EdgeInsets.all(14),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(22),
                  border: Border.all(color: AppColors.line),
                  color: AppColors.bg2,
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
                      child: Icon(Icons.wb_sunny_rounded, color: cs.primary),
                    ),
                    const SizedBox(width: 12),
                    const Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text("Menu", style: TextStyle(fontWeight: FontWeight.w900, fontSize: 16)),
                          SizedBox(height: 2),
                          Text("Ações e configurações", style: TextStyle(color: AppColors.muted)),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 10),
              _DrawerTile(
                icon: Icons.search_rounded,
                label: "Buddys encontrados",
                subtitle: "Veja quem está por perto",
                onTap: onOpenFound,
              ),
              _DrawerTile(
                icon: Icons.settings_rounded,
                label: "Configurações",
                subtitle: "Conta, sessão e preferências",
                onTap: onOpenSettings,
              ),
              const Spacer(),
              const Text(
                "Dica: puxe pra baixo para atualizar as emoções.",
                style: TextStyle(color: AppColors.muted),
                textAlign: TextAlign.center,
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _DrawerTile extends StatelessWidget {
  final IconData icon;
  final String label;
  final String subtitle;
  final VoidCallback onTap;

  const _DrawerTile({
    required this.icon,
    required this.label,
    required this.subtitle,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return InkWell(
      borderRadius: BorderRadius.circular(18),
      onTap: onTap,
      child: Container(
        margin: const EdgeInsets.symmetric(vertical: 6),
        padding: const EdgeInsets.all(14),
        decoration: BoxDecoration(
          color: Colors.white,
          borderRadius: BorderRadius.circular(18),
          border: Border.all(color: AppColors.line),
        ),
        child: Row(
          children: [
            Container(
              width: 40,
              height: 40,
              decoration: BoxDecoration(
                color: cs.primary.withOpacity(.12),
                borderRadius: BorderRadius.circular(14),
              ),
              child: Icon(icon, color: cs.primary),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(label, style: const TextStyle(fontWeight: FontWeight.w900)),
                  const SizedBox(height: 2),
                  Text(subtitle, style: const TextStyle(color: AppColors.muted)),
                ],
              ),
            ),
            const Icon(Icons.chevron_right_rounded, color: AppColors.muted),
          ],
        ),
      ),
    );
  }
}

class _StatusPill extends StatelessWidget {
  final String text;
  const _StatusPill({required this.text});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final t = text.trim().isEmpty ? "Pronto" : text;

    Color bg = cs.primary.withOpacity(.10);
    Color fg = cs.primary;

    final lower = t.toLowerCase();
    if (lower.contains("erro") || lower.contains("falh")) {
      bg = AppColors.bad.withOpacity(.12);
      fg = AppColors.bad;
    } else if (lower.contains("busc") || lower.contains("conect")) {
      bg = AppColors.warn.withOpacity(.14);
      fg = AppColors.warn;
    }

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
      decoration: BoxDecoration(
        color: bg,
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: fg.withOpacity(.22)),
      ),
      child: Text(
        t,
        style: TextStyle(color: fg, fontWeight: FontWeight.w800, fontSize: 12),
        overflow: TextOverflow.ellipsis,
      ),
    );
  }
}

class _BuddyHeroCard extends StatelessWidget {
  final Map<String, dynamic> emocoes;
  const _BuddyHeroCard({required this.emocoes});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    final nome = capitalize(emocoes['nome']?.toString() ?? 'DeskBuddy');
    final dominante = (emocoes['dominante']?.toString().trim().isEmpty ?? true)
        ? "neutro"
        : emocoes['dominante'].toString();

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(26),
        border: Border.all(color: AppColors.line),
        boxShadow: const [
          BoxShadow(
            blurRadius: 26,
            offset: Offset(0, 12),
            color: Color(0x0F000000),
          )
        ],
      ),
      child: Column(
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  nome,
                  style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w900),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
              const SizedBox(width: 10),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
                decoration: BoxDecoration(
                  color: cs.primary.withOpacity(.10),
                  borderRadius: BorderRadius.circular(999),
                  border: Border.all(color: cs.primary.withOpacity(.22)),
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(_emotionIcon(dominante), size: 16, color: cs.primary),
                    const SizedBox(width: 6),
                    Text(
                      capitalize(dominante),
                      style: TextStyle(color: cs.primary, fontWeight: FontWeight.w900, fontSize: 12),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),

          // 🔒 NÃO mexer no asset/stack (mantido como está; só reposicionamos/estilizamos o card)
          AspectRatio(
            aspectRatio: 16 / 9,
            child: ClipRRect(
              borderRadius: BorderRadius.circular(22),
              child: Container(
                color: AppColors.bg2,
                child: Stack(
                  alignment: Alignment.center,
                  children: [
                    // Imagem de fundo (emoção)
                    Opacity(
                      opacity: .95,
                      child: Image.asset(
                        'assets/$dominante.gif',
                        width: 100,
                        height: 53,
                        fit: BoxFit.cover,
                      ),
                    ),
                    // Imagem principal do DeskBuddy (stack original)
                    Image.asset(
                      'assets/images/DeskBuddyBody.png',
                      width: double.infinity,
                      height: double.infinity,
                      fit: BoxFit.contain,
                    ),
                  ],
                ),
              ),
            ),
          ),

          const SizedBox(height: 12),
          Row(
            children: [
              _MiniInfo(label: "Parceiro", value: emocoes['parceiro']),
              const SizedBox(width: 10),
              _MiniInfo(label: "BLE App", value: emocoes['ble_app_conectado']),
              const SizedBox(width: 10),
              _MiniInfo(label: "Sleeping", value: emocoes['sleeping']),
            ],
          ),
        ],
      ),
    );
  }
}

class _MiniInfo extends StatelessWidget {
  final String label;
  final dynamic value;
  const _MiniInfo({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    final v = (value == null || value.toString().trim().isEmpty) ? "—" : value.toString();
    return Expanded(
      child: Container(
        padding: const EdgeInsets.all(10),
        decoration: BoxDecoration(
          color: AppColors.bg2,
          borderRadius: BorderRadius.circular(18),
          border: Border.all(color: AppColors.line),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(label, style: const TextStyle(color: AppColors.muted, fontWeight: FontWeight.w700, fontSize: 12)),
            const SizedBox(height: 2),
            Text(v, style: const TextStyle(fontWeight: FontWeight.w900)),
          ],
        ),
      ),
    );
  }
}

class _QuickStatsRow extends StatelessWidget {
  final Map<String, dynamic> emocoes;
  const _QuickStatsRow({required this.emocoes});

  @override
  Widget build(BuildContext context) {
    final batteryPct = emocoes['bateria_pct'];
    final batteryV = emocoes['bateria_v'];
    final low = emocoes['bateria_low']?.toString().toLowerCase() == 'true';

    return Row(
      children: [
        Expanded(
          child: _StatCard(
            icon: Icons.battery_full_rounded,
            label: "Bateria",
            value: batteryPct != null ? "${batteryPct}%" : "—",
            sub: batteryV != null ? "${batteryV} V" : null,
            tone: low ? _Tone.bad : _Tone.neutral,
          ),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: _StatCard(
            icon: Icons.light_mode_rounded,
            label: "LDR",
            value: emocoes['ldr_mv'] != null ? "${emocoes['ldr_mv']} mV" : "—",
            sub: "luminosidade",
            tone: _Tone.neutral,
          ),
        ),
      ],
    );
  }
}

enum _Tone { neutral, warn, bad }

class _StatCard extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final String? sub;
  final _Tone tone;

  const _StatCard({
    required this.icon,
    required this.label,
    required this.value,
    this.sub,
    required this.tone,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    Color c = cs.primary;
    Color bg = cs.primary.withOpacity(.10);
    if (tone == _Tone.warn) {
      c = AppColors.warn;
      bg = AppColors.warn.withOpacity(.12);
    } else if (tone == _Tone.bad) {
      c = AppColors.bad;
      bg = AppColors.bad.withOpacity(.12);
    }

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: AppColors.line),
      ),
      child: Row(
        children: [
          Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(color: bg, borderRadius: BorderRadius.circular(14)),
            child: Icon(icon, color: c),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(label, style: const TextStyle(color: AppColors.muted, fontWeight: FontWeight.w700, fontSize: 12)),
                const SizedBox(height: 2),
                Text(value, style: const TextStyle(fontWeight: FontWeight.w900, fontSize: 16)),
                if (sub != null) ...[
                  const SizedBox(height: 2),
                  Text(sub!, style: const TextStyle(color: AppColors.muted, fontSize: 12)),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _DominantEmotionCard extends StatelessWidget {
  final Map<String, dynamic> emocoes;
  const _DominantEmotionCard({required this.emocoes});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final dominante = emocoes['dominante']?.toString();

    return Container(
      padding: const EdgeInsets.all(14),
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
            child: Icon(Icons.mood_rounded, color: cs.primary),
          ),
          const SizedBox(width: 12),
          const Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                //Text("Agora", style: TextStyle(color: AppColors.muted, fontWeight: FontWeight.w700, fontSize: 12)),
                SizedBox(height: 2),
                Text("Status", style: TextStyle(fontWeight: FontWeight.w900, fontSize: 16)),
              ],
            ),
          ),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
            decoration: BoxDecoration(
              color: cs.primary.withOpacity(.10),
              borderRadius: BorderRadius.circular(18),
              border: Border.all(color: cs.primary.withOpacity(.22)),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(_emotionIcon(dominante), size: 18, color: cs.primary),
                const SizedBox(width: 8),
                Text(
                  capitalize(dominante ?? "—"),
                  style: TextStyle(color: cs.primary, fontWeight: FontWeight.w900),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _EmotionDistribution extends StatelessWidget {
  final Map<String, dynamic> emocoes;
  final double Function(dynamic) parsePercent;

  const _EmotionDistribution({
    required this.emocoes,
    required this.parsePercent,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    const excluded = {
      'dominante',
      'encontrados',
      'nome',
      'parceiro',
      'bateria_pct',
      'bateria_v',
      'bateria_low',
      'ble_app_conectado',
      'sleeping',
      'ldr_mv',
    };

    final items = emocoes.entries
        .where((e) => !excluded.contains(e.key))
        .map((e) => MapEntry(e.key, parsePercent(e.value)))
        .toList();

    items.sort((a, b) => b.value.compareTo(a.value));
    final top = items.take(8).toList();

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
                child: Icon(Icons.bar_chart_rounded, color: cs.primary, size: 20),
              ),
              const SizedBox(width: 10),
              const Expanded(
                child: Text(
                  "Distribuição de emoções",
                  style: TextStyle(fontWeight: FontWeight.w900, fontSize: 16),
                ),
              ),
              Text(
                "top ${top.length}",
                style: const TextStyle(color: AppColors.muted, fontWeight: FontWeight.w800),
              ),
            ],
          ),
          const SizedBox(height: 12),

          if (top.isEmpty)
            const Text("Sem dados de emoções ainda.", style: TextStyle(color: AppColors.muted))
          else
            ...top.map((e) => _EmotionBar(
                  label: e.key,
              percent: (e.value.clamp(0, 100) as num).toDouble(),
            )),
        ],
      ),
    );
  }
}

class _EmotionBar extends StatelessWidget {
  final String label;
  final double percent;

  const _EmotionBar({required this.label, required this.percent});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final p = percent.isNaN ? 0.0 : percent;

    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(_emotionIcon(label), size: 16, color: cs.primary),
              const SizedBox(width: 6),
              Expanded(
                child: Text(
                  capitalize(label),
                  style: const TextStyle(fontWeight: FontWeight.w900),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
              Text("${p.toStringAsFixed(0)}%", style: const TextStyle(color: AppColors.muted, fontWeight: FontWeight.w900)),
            ],
          ),
          const SizedBox(height: 6),
          ClipRRect(
            borderRadius: BorderRadius.circular(999),
            child: LayoutBuilder(
              builder: (context, c) {
                final w = c.maxWidth;
                final fill = (w * (p / 100)).clamp(0.0, w).toDouble();
                return Stack(
                  children: [
                    Container(height: 12, color: AppColors.bg2),
                    AnimatedContainer(
                      duration: const Duration(milliseconds: 350),
                      curve: Curves.easeOutCubic,
                      width: fill,
                      height: 12,
                      color: cs.primary,
                    ),
                  ],
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

class _ToolsCard extends StatelessWidget {
  final VoidCallback onOpenFound;
  final VoidCallback onOpenSettings;

  const _ToolsCard({
    required this.onOpenFound,
    required this.onOpenSettings,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

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
                child: Icon(Icons.auto_awesome_rounded, color: cs.primary, size: 20),
              ),
              const SizedBox(width: 10),
              const Expanded(
                child: Text("Ações rápidas", style: TextStyle(fontWeight: FontWeight.w900, fontSize: 16)),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: onOpenFound,
                  icon: const Icon(Icons.search_rounded),
                  label: const Text("Encontrados"),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: cs.primary,
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(vertical: 14),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
                  ),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: onOpenSettings,
                  icon: const Icon(Icons.settings_rounded),
                  label: const Text("Config"),
                  style: OutlinedButton.styleFrom(
                    foregroundColor: cs.primary,
                    padding: const EdgeInsets.symmetric(vertical: 14),
                    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
                    side: BorderSide(color: cs.primary.withOpacity(.35)),
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _AdvancedCard extends StatelessWidget {
  final Map<String, dynamic> emocoes;
  const _AdvancedCard({required this.emocoes});

  @override
  Widget build(BuildContext context) {
    return ExpansionTile(
      tilePadding: const EdgeInsets.symmetric(horizontal: 14),
      collapsedBackgroundColor: Colors.white,
      backgroundColor: Colors.white,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22), side: const BorderSide(color: AppColors.line)),
      collapsedShape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22), side: const BorderSide(color: AppColors.line)),
      title: const Text("Avançado", style: TextStyle(fontWeight: FontWeight.w900)),
      subtitle: const Text("Debug e JSON completo", style: TextStyle(color: AppColors.muted)),
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
              const JsonEncoder.withIndent("  ").convert(emocoes),
              style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
            ),
          ),
        ),
      ],
    );
  }
}

class _LoadingState extends StatelessWidget {
  final String status;
  const _LoadingState({required this.status});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(22),
        child: Container(
          padding: const EdgeInsets.all(18),
          decoration: BoxDecoration(
            color: Colors.white,
            borderRadius: BorderRadius.circular(22),
            border: Border.all(color: AppColors.line),
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              SizedBox(
                width: 34,
                height: 34,
                child: CircularProgressIndicator(strokeWidth: 3, color: cs.primary),
              ),
              const SizedBox(height: 14),
              const Text("Procurando seu DeskBuddy…", style: TextStyle(fontWeight: FontWeight.w900)),
              const SizedBox(height: 6),
              Text(status, style: const TextStyle(color: AppColors.muted), textAlign: TextAlign.center),
            ],
          ),
        ),
      ),
    );
  }
}

class _RetryState extends StatelessWidget {
  final int reloadCount;
  final Future<void> Function()? onRetry;
  final VoidCallback onRestart;

  const _RetryState({
    required this.reloadCount,
    required this.onRetry,
    required this.onRestart,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    final canRetry = onRetry != null;
    final title = reloadCount >= 3
        ? "Não foi possível conectar ao DeskBuddy."
        : "Não foi possível encontrar seu DeskBuddy.";
    final subtitle = reloadCount >= 3
        ? "Feche e reabra o aplicativo para tentar novamente, ou reinicie por aqui."
        : "Verifique se ele está ligado e perto do celular.";

    return Center(
      child: Padding(
        padding: const EdgeInsets.all(22),
        child: Container(
          padding: const EdgeInsets.all(18),
          decoration: BoxDecoration(
            color: Colors.white,
            borderRadius: BorderRadius.circular(22),
            border: Border.all(color: AppColors.line),
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.search_off_rounded, size: 54, color: cs.primary),
              const SizedBox(height: 14),
              Text(title, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w900), textAlign: TextAlign.center),
              const SizedBox(height: 8),
              Text(subtitle, style: const TextStyle(color: AppColors.muted), textAlign: TextAlign.center),
              const SizedBox(height: 16),
              if (canRetry)
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton.icon(
                    onPressed: () => onRetry!(),
                    icon: const Icon(Icons.refresh_rounded),
                    label: const Text("Tentar novamente"),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: cs.primary,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
                    ),
                  ),
                ),
              if (!canRetry)
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton.icon(
                    onPressed: onRestart,
                    icon: const Icon(Icons.restart_alt_rounded),
                    label: const Text("Reiniciar aplicativo"),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: cs.primary,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
                    ),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}

// Helpers
String capitalize(String s) {
  if (s.trim().isEmpty) return s;
  return s[0].toUpperCase() + s.substring(1);
}

IconData _emotionIcon(String? emotion) {
  final e = (emotion ?? '').toLowerCase();
  if (e.contains("feliz") || e.contains("happy")) return Icons.sentiment_very_satisfied_rounded;
  if (e.contains("triste") || e.contains("sad")) return Icons.sentiment_dissatisfied_rounded;
  if (e.contains("bravo") || e.contains("angry")) return Icons.sentiment_very_dissatisfied_rounded;
  if (e.contains("apaixonado") || e.contains("love")) return Icons.favorite_rounded;
  if (e.contains("sono") || e.contains("sleep")) return Icons.bedtime_rounded;
  if (e.contains("entediado") || e.contains("sleep")) return Icons.sentiment_neutral_rounded;
  if (e.contains("fome") || e.contains("hunger")) return Icons.restaurant_rounded;
  return Icons.sentiment_satisfied_rounded;
}
