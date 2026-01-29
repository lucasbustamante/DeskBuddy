import 'package:deskbuddy/app_theme.dart';
import 'package:flutter/material.dart';

class BuddysEncontradosPage extends StatefulWidget {
  final List<dynamic> encontrados;

  const BuddysEncontradosPage({Key? key, required this.encontrados}) : super(key: key);

  @override
  _BuddysEncontradosPageState createState() => _BuddysEncontradosPageState();
}

class _BuddysEncontradosPageState extends State<BuddysEncontradosPage> {
  final _searchCtrl = TextEditingController();

  bool _ordemAlfabetica = true;
  bool _ordemGosta = false;

  @override
  void dispose() {
    _searchCtrl.dispose();
    super.dispose();
  }

  List<dynamic> get _filteredList {
    List<dynamic> lista = List<dynamic>.from(widget.encontrados);

    final q = _searchCtrl.text.trim().toLowerCase();
    if (q.isNotEmpty) {
      lista = lista.where((e) => (e['nome'] ?? "").toString().toLowerCase().contains(q)).toList();
    }

    if (_ordemGosta) {
      lista.sort((a, b) {
        int rank(dynamic e) {
          if (e['gosta'] == true) return 0;
          if (e['gosta'] == false) return 1;
          return 2;
        }

        final cmp = rank(a).compareTo(rank(b));
        if (cmp != 0) return cmp;
        return (a['nome'] ?? "").toString().toLowerCase().compareTo((b['nome'] ?? "").toString().toLowerCase());
      });
    } else if (_ordemAlfabetica) {
      lista.sort((a, b) =>
          (a['nome'] ?? "").toString().toLowerCase().compareTo((b['nome'] ?? "").toString().toLowerCase()));
    }

    return lista;
  }

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final list = _filteredList;

    return Scaffold(
      backgroundColor: AppColors.bg,
      appBar: AppBar(
        title: const Text("Buddys encontrados"),
      ),
      body: Padding(
        padding: const EdgeInsets.fromLTRB(16, 10, 16, 16),
        child: Column(
          children: [
            // Search
            TextField(
              controller: _searchCtrl,
              onChanged: (_) => setState(() {}),
              textInputAction: TextInputAction.search,
              decoration: InputDecoration(
                hintText: "Buscar pelo nome…",
                prefixIcon: const Icon(Icons.search_rounded),
                suffixIcon: _searchCtrl.text.isEmpty
                    ? null
                    : IconButton(
                        icon: const Icon(Icons.close_rounded),
                        onPressed: () {
                          _searchCtrl.clear();
                          setState(() {});
                        },
                      ),
              ),
            ),
            const SizedBox(height: 10),

            // Sorting chips
            Row(
              children: [
                Expanded(
                  child: _FilterChip(
                    selected: _ordemAlfabetica && !_ordemGosta,
                    label: "A–Z",
                    icon: Icons.sort_by_alpha_rounded,
                    onTap: () => setState(() {
                      _ordemAlfabetica = true;
                      _ordemGosta = false;
                    }),
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: _FilterChip(
                    selected: _ordemGosta,
                    label: "Gosta 1º",
                    icon: Icons.favorite_rounded,
                    onTap: () => setState(() {
                      _ordemGosta = true;
                      _ordemAlfabetica = false;
                    }),
                  ),
                ),
              ],
            ),

            const SizedBox(height: 12),
            Expanded(
              child: list.isEmpty
                  ? Center(
                      child: Container(
                        padding: const EdgeInsets.all(18),
                        decoration: BoxDecoration(
                          color: Colors.white,
                          borderRadius: BorderRadius.circular(22),
                          border: Border.all(color: AppColors.line),
                        ),
                        child: const Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(Icons.search_off_rounded, size: 48, color: AppColors.muted),
                            SizedBox(height: 10),
                            Text("Nenhum buddy encontrado", style: TextStyle(fontWeight: FontWeight.w900)),
                            SizedBox(height: 6),
                            Text("Tente ajustar a busca ou volte mais tarde.",
                                style: TextStyle(color: AppColors.muted), textAlign: TextAlign.center),
                          ],
                        ),
                      ),
                    )
                  : ListView.separated(
                      itemCount: list.length,
                      separatorBuilder: (_, __) => const SizedBox(height: 10),
                      itemBuilder: (context, index) {
                        final b = list[index] as Map;
                        final nome = (b['nome'] ?? "—").toString();
                        final gosta = b['gosta'];
                        final afinidade = b['afinidade'];
                        final estado = (b['estado'] ?? b['emotion'] ?? "").toString();

                        Color badgeBg = AppColors.bg2;
                        Color badgeFg = AppColors.muted;
                        IconData badgeIcon = Icons.help_outline_rounded;

                        if (gosta == true) {
                          badgeBg = cs.primary.withOpacity(.12);
                          badgeFg = cs.primary;
                          badgeIcon = Icons.favorite_rounded;
                        } else if (gosta == false) {
                          badgeBg = AppColors.bad.withOpacity(.12);
                          badgeFg = AppColors.bad;
                          badgeIcon = Icons.heart_broken_rounded;
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
                                width: 44,
                                height: 44,
                                decoration: BoxDecoration(
                                  color: cs.primary.withOpacity(.10),
                                  borderRadius: BorderRadius.circular(16),
                                ),
                                child: Icon(Icons.person_rounded, color: cs.primary),
                              ),
                              const SizedBox(width: 12),
                              Expanded(
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    Text(nome, style: const TextStyle(fontWeight: FontWeight.w900, fontSize: 16)),
                                    const SizedBox(height: 4),
                                    Wrap(
                                      spacing: 8,
                                      runSpacing: 8,
                                      children: [
                                        if (estado.trim().isNotEmpty) _SmallChip(label: estado, icon: Icons.mood_rounded),
                                        if (afinidade != null) _SmallChip(label: "Afinidade: $afinidade", icon: Icons.trending_up_rounded),
                                      ],
                                    ),
                                  ],
                                ),
                              ),
                              const SizedBox(width: 10),
                              Container(
                                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                                decoration: BoxDecoration(
                                  color: badgeBg,
                                  borderRadius: BorderRadius.circular(999),
                                  border: Border.all(color: badgeFg.withOpacity(.22)),
                                ),
                                child: Row(
                                  mainAxisSize: MainAxisSize.min,
                                  children: [
                                    Icon(badgeIcon, size: 16, color: badgeFg),
                                    const SizedBox(width: 6),
                                    Text(
                                      gosta == true
                                          ? "Gosta"
                                          : gosta == false
                                              ? "Não gosta"
                                              : "Indef.",
                                      style: TextStyle(color: badgeFg, fontWeight: FontWeight.w900, fontSize: 12),
                                    ),
                                  ],
                                ),
                              ),
                            ],
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

class _FilterChip extends StatelessWidget {
  final bool selected;
  final String label;
  final IconData icon;
  final VoidCallback onTap;

  const _FilterChip({
    required this.selected,
    required this.label,
    required this.icon,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    final bg = selected ? cs.primary.withOpacity(.12) : Colors.white;
    final fg = selected ? cs.primary : AppColors.text;

    return InkWell(
      borderRadius: BorderRadius.circular(18),
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
        decoration: BoxDecoration(
          color: bg,
          borderRadius: BorderRadius.circular(18),
          border: Border.all(color: selected ? cs.primary.withOpacity(.30) : AppColors.line),
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(icon, size: 18, color: fg),
            const SizedBox(width: 8),
            Text(label, style: TextStyle(color: fg, fontWeight: FontWeight.w900)),
          ],
        ),
      ),
    );
  }
}

class _SmallChip extends StatelessWidget {
  final String label;
  final IconData icon;
  const _SmallChip({required this.label, required this.icon});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
      decoration: BoxDecoration(
        color: cs.primary.withOpacity(.10),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: cs.primary.withOpacity(.20)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 14, color: cs.primary),
          const SizedBox(width: 6),
          Text(label, style: TextStyle(color: cs.primary, fontWeight: FontWeight.w900, fontSize: 12)),
        ],
      ),
    );
  }
}
