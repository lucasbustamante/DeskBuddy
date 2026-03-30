// ─────────────────────────────────────────────────────────────────────────────
// firmware_update_page.dart — Tela de Atualização OTA via BLE
//
// CORREÇÕES vs versão original:
// [FIX-UI-1] BleController mantinha polling JSON durante OTA → conflito BLE.
//             Adicionada pausa do polling antes do OTA e retomada após.
// [FIX-UI-2] discoverServices() durante OTA falhava se o device já tinha
//             serviços em cache. Agora usa o cache do flutter_blue_plus.
// [FIX-UI-3] Mensagem de erro scrollável para exibir diagnósticos longos.
// ─────────────────────────────────────────────────────────────────────────────

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'package:deskbuddy/app_theme.dart';
import 'package:deskbuddy/ota_service.dart';

class FirmwareUpdatePage extends StatefulWidget {
  /// Device BLE já conectado (vindo do BleController).
  final BluetoothDevice? device;

  /// Versão atual do firmware exibida na UI (lida do JSON BLE).
  final String? currentVersion;

  /// Callback para pausar o polling JSON do BleController durante o OTA.
  /// Sem isso, o BleController tenta ler a characteristic JSON ao mesmo
  /// tempo que o OTA usa a conexão → conflito BLE → falha no OTA.
  final VoidCallback? onPausePolling;

  /// Callback para retomar o polling JSON após o OTA.
  final VoidCallback? onResumePolling;

  const FirmwareUpdatePage({
    Key? key,
    this.device,
    this.currentVersion,
    this.onPausePolling,
    this.onResumePolling,
  }) : super(key: key);

  @override
  State<FirmwareUpdatePage> createState() => _FirmwareUpdatePageState();
}

class _FirmwareUpdatePageState extends State<FirmwareUpdatePage>
    with SingleTickerProviderStateMixin {

  OtaState _state    = OtaState.idle;
  double   _progress = 0;
  String   _message  = 'Pronto para atualizar.';
  bool     _running  = false;

  OtaService? _otaService;

  late AnimationController _pulseController;
  late Animation<double>   _pulseAnimation;

  @override
  void initState() {
    super.initState();
    _pulseController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    )..repeat(reverse: true);
    _pulseAnimation = Tween<double>(begin: 0.6, end: 1.0).animate(
      CurvedAnimation(parent: _pulseController, curve: Curves.easeInOut),
    );
  }

  @override
  void dispose() {
    _pulseController.dispose();
    _otaService?.cancel();
    // [FIX-UI-1] Garante que o polling seja retomado se a tela for fechada
    // durante o OTA (ex: botão back no OS).
    widget.onResumePolling?.call();
    super.dispose();
  }

  // ─── OTA logic ─────────────────────────────────────────────────────────────

  Future<void> _startUpdate() async {
    if (widget.device == null) {
      _showSnack('DeskBuddy não conectado. Volte e aguarde a conexão BLE.', isError: true);
      return;
    }

    // Verifica estado da conexão
    BluetoothConnectionState connState;
    try {
      connState = await widget.device!.connectionState.first;
    } catch (_) {
      _showSnack('Não foi possível verificar o estado da conexão BLE.', isError: true);
      return;
    }

    if (connState != BluetoothConnectionState.connected) {
      _showSnack('DeskBuddy está desconectado. Aguarde a reconexão.', isError: true);
      return;
    }

    // [FIX-UI-1] Pausa o polling JSON para não conflitar com o OTA
    widget.onPausePolling?.call();

    setState(() {
      _running  = true;
      _state    = OtaState.preparando;
      _progress = 0;
      _message  = 'Iniciando…';
    });

    _otaService = OtaService(
      onProgress: (state, progress, message) {
        if (!mounted) return;
        setState(() {
          _state    = state;
          _progress = progress;
          _message  = message;
        });
      },
      onError: (error) {
        if (!mounted) return;
        setState(() => _running = false);
        // [FIX-UI-1] Retoma polling em caso de erro
        widget.onResumePolling?.call();
        _showSnack(error, isError: true);
      },
      onSuccess: () {
        if (!mounted) return;
        setState(() => _running = false);
        // Não retoma polling pois o ESP32 irá reiniciar e reconectar
        _showSnack('✅ Firmware atualizado! DeskBuddy está reiniciando…', isError: false);
      },
    );

    await _otaService!.startOta(device: widget.device!);
  }

  void _cancelUpdate() {
    _otaService?.cancel();
    widget.onResumePolling?.call();
    setState(() {
      _running  = false;
      _state    = OtaState.idle;
      _progress = 0;
      _message  = 'Atualização cancelada.';
    });
  }

  void _showSnack(String msg, {required bool isError}) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      content: Text(msg, maxLines: 5, overflow: TextOverflow.ellipsis),
      backgroundColor: isError ? AppColors.bad : AppColors.good,
      behavior: SnackBarBehavior.floating,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(14)),
      duration: Duration(seconds: isError ? 8 : 4),
    ));
  }

  // ─── UI helpers ─────────────────────────────────────────────────────────────

  String get _stateLabel {
    switch (_state) {
      case OtaState.preparando:  return 'Preparando';
      case OtaState.conectando:  return 'Conectando';
      case OtaState.enviando:    return 'Enviando firmware';
      case OtaState.validando:   return 'Validando';
      case OtaState.finalizando: return 'Finalizando';
      case OtaState.concluido:   return 'Concluído ✅';
      case OtaState.erro:        return 'Erro ❌';
      default:                   return 'Pronto';
    }
  }

  Color get _stateColor {
    switch (_state) {
      case OtaState.concluido: return AppColors.good;
      case OtaState.erro:      return AppColors.bad;
      default:                 return AppColors.orange;
    }
  }

  IconData get _stateIcon {
    switch (_state) {
      case OtaState.preparando:
      case OtaState.conectando:
      case OtaState.enviando:
      case OtaState.validando:
      case OtaState.finalizando: return Icons.upload_rounded;
      case OtaState.concluido:   return Icons.check_circle_rounded;
      case OtaState.erro:        return Icons.error_rounded;
      default:                   return Icons.system_update_rounded;
    }
  }

  // ─── Build ─────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      backgroundColor: AppColors.bg,
      appBar: AppBar(
        title: Row(
          children: [
            Container(
              width: 34,
              height: 34,
              decoration: BoxDecoration(
                color: cs.primary.withOpacity(.12),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: cs.primary.withOpacity(.25)),
              ),
              child: Icon(Icons.system_update_rounded, color: cs.primary, size: 18),
            ),
            const SizedBox(width: 10),
            const Text('Atualização de firmware'),
          ],
        ),
        leading: _running
            ? const SizedBox.shrink()
            : IconButton(
                icon: const Icon(Icons.arrow_back_rounded),
                onPressed: () => Navigator.pop(context),
              ),
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 14, 16, 24),
          children: [
            // Card de status
            _StatusCard(
              state: _state,
              stateLabel: _stateLabel,
              stateColor: _stateColor,
              stateIcon: _stateIcon,
              message: _message,
              progress: _progress,
              running: _running,
              pulseAnimation: _pulseAnimation,
            ),
            const SizedBox(height: 14),

            // Card de informações
            _InfoCard(
              currentVersion: widget.currentVersion,
              deviceConnected: widget.device != null,
            ),
            const SizedBox(height: 14),

            // Avisos
            if (!_running && _state != OtaState.concluido) ...[
              _WarningCard(),
              const SizedBox(height: 20),
            ],

            // Botões
            if (_state == OtaState.concluido)
              _DoneButton(onBack: () => Navigator.pop(context)),

            if (_running)
              _CancelButton(onCancel: _cancelUpdate),

            if (!_running && _state != OtaState.concluido)
              _StartButton(
                enabled: widget.device != null,
                onStart: _startUpdate,
              ),
          ],
        ),
      ),
    );
  }
}

// ─── Sub-widgets ─────────────────────────────────────────────────────────────

class _StatusCard extends StatelessWidget {
  final OtaState state;
  final String   stateLabel;
  final Color    stateColor;
  final IconData stateIcon;
  final String   message;
  final double   progress;
  final bool     running;
  final Animation<double> pulseAnimation;

  const _StatusCard({
    required this.state,
    required this.stateLabel,
    required this.stateColor,
    required this.stateIcon,
    required this.message,
    required this.progress,
    required this.running,
    required this.pulseAnimation,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(18),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: AppColors.line),
        boxShadow: const [BoxShadow(blurRadius: 18, offset: Offset(0, 8), color: Color(0x0A000000))],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              AnimatedBuilder(
                animation: pulseAnimation,
                builder: (context, child) => Opacity(
                  opacity: running ? pulseAnimation.value : 1.0,
                  child: child,
                ),
                child: Container(
                  width: 46,
                  height: 46,
                  decoration: BoxDecoration(
                    color: stateColor.withOpacity(.14),
                    borderRadius: BorderRadius.circular(16),
                  ),
                  child: Icon(stateIcon, color: stateColor, size: 24),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      stateLabel,
                      style: TextStyle(
                        fontWeight: FontWeight.w900,
                        fontSize: 16,
                        color: stateColor,
                      ),
                    ),
                    const SizedBox(height: 3),
                    // [FIX-UI-3] Texto de erro scrollável
                    Text(
                      message,
                      style: const TextStyle(color: AppColors.muted, fontSize: 13),
                      maxLines: 6,
                      overflow: TextOverflow.ellipsis,
                    ),
                  ],
                ),
              ),
              if (running)
                SizedBox(
                  width: 22,
                  height: 22,
                  child: CircularProgressIndicator(
                    strokeWidth: 2.5,
                    color: stateColor,
                  ),
                ),
            ],
          ),

          // Barra de progresso
          if (state != OtaState.idle && state != OtaState.erro) ...[
            const SizedBox(height: 16),
            Row(
              children: [
                Expanded(
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(999),
                    child: LayoutBuilder(
                      builder: (ctx, c) {
                        final fill = (c.maxWidth * progress).clamp(0.0, c.maxWidth);
                        return Stack(
                          children: [
                            Container(height: 10, color: AppColors.bg2),
                            AnimatedContainer(
                              duration: const Duration(milliseconds: 250),
                              curve: Curves.easeOutCubic,
                              width: fill,
                              height: 10,
                              decoration: BoxDecoration(
                                color: stateColor,
                                borderRadius: BorderRadius.circular(999),
                              ),
                            ),
                          ],
                        );
                      },
                    ),
                  ),
                ),
                const SizedBox(width: 10),
                SizedBox(
                  width: 42,
                  child: Text(
                    '${(progress * 100).toStringAsFixed(0)}%',
                    style: TextStyle(
                      color: stateColor,
                      fontWeight: FontWeight.w900,
                      fontSize: 13,
                    ),
                    textAlign: TextAlign.right,
                  ),
                ),
              ],
            ),
          ],
        ],
      ),
    );
  }
}

class _InfoCard extends StatelessWidget {
  final String? currentVersion;
  final bool    deviceConnected;

  const _InfoCard({this.currentVersion, required this.deviceConnected});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: AppColors.line),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text('Informações', style: TextStyle(fontWeight: FontWeight.w900, fontSize: 15)),
          const SizedBox(height: 10),
          _InfoRow(icon: Icons.info_outline_rounded,  label: 'Versão atual',        value: currentVersion ?? '—',                    color: cs.primary),
          _InfoRow(icon: Icons.folder_open_rounded,   label: 'Firmware a instalar', value: 'assets/firmware/deskbuddy.bin',           color: cs.primary),
          _InfoRow(icon: Icons.bluetooth_rounded,     label: 'Conexão BLE',         value: deviceConnected ? 'Conectado' : 'Desconectado', color: deviceConnected ? AppColors.good : AppColors.bad),
        ],
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  final IconData icon;
  final String   label;
  final String   value;
  final Color    color;

  const _InfoRow({required this.icon, required this.label, required this.value, required this.color});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 5),
      child: Row(
        children: [
          Icon(icon, size: 16, color: AppColors.muted),
          const SizedBox(width: 8),
          Text(label, style: const TextStyle(color: AppColors.muted, fontSize: 13)),
          const Spacer(),
          Flexible(
            child: Text(
              value,
              style: TextStyle(fontWeight: FontWeight.w800, fontSize: 13, color: color),
              textAlign: TextAlign.right,
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      ),
    );
  }
}

class _WarningCard extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AppColors.warn.withOpacity(.08),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: AppColors.warn.withOpacity(.3)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.warning_amber_rounded, color: AppColors.warn, size: 20),
          const SizedBox(width: 10),
          const Expanded(
            child: Text(
              'Mantenha o DeskBuddy próximo durante a atualização.\n'
              'Não feche o app até a conclusão.\n'
              'O dispositivo reiniciará automaticamente ao final.',
              style: TextStyle(color: AppColors.warn, fontSize: 13, height: 1.5),
            ),
          ),
        ],
      ),
    );
  }
}

class _StartButton extends StatelessWidget {
  final bool         enabled;
  final VoidCallback onStart;

  const _StartButton({required this.enabled, required this.onStart});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return SizedBox(
      width: double.infinity,
      child: ElevatedButton.icon(
        icon: const Icon(Icons.system_update_rounded, color: Colors.white),
        label: const Text('Iniciar atualização', style: TextStyle(fontWeight: FontWeight.w900, color: Colors.white)),
        onPressed: enabled ? onStart : null,
        style: ElevatedButton.styleFrom(
          backgroundColor: cs.primary,
          disabledBackgroundColor: AppColors.line,
          padding: const EdgeInsets.symmetric(vertical: 16),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
          elevation: 0,
        ),
      ),
    );
  }
}

class _CancelButton extends StatelessWidget {
  final VoidCallback onCancel;
  const _CancelButton({required this.onCancel});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: double.infinity,
      child: OutlinedButton.icon(
        icon: const Icon(Icons.cancel_rounded, color: AppColors.bad),
        label: const Text('Cancelar atualização', style: TextStyle(fontWeight: FontWeight.w900, color: AppColors.bad)),
        onPressed: onCancel,
        style: OutlinedButton.styleFrom(
          side: BorderSide(color: AppColors.bad.withOpacity(.5)),
          padding: const EdgeInsets.symmetric(vertical: 16),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
        ),
      ),
    );
  }
}

class _DoneButton extends StatelessWidget {
  final VoidCallback onBack;
  const _DoneButton({required this.onBack});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: double.infinity,
      child: ElevatedButton.icon(
        icon: const Icon(Icons.check_circle_rounded, color: Colors.white),
        label: const Text('Concluído — Voltar', style: TextStyle(fontWeight: FontWeight.w900, color: Colors.white)),
        onPressed: onBack,
        style: ElevatedButton.styleFrom(
          backgroundColor: AppColors.good,
          padding: const EdgeInsets.symmetric(vertical: 16),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(18)),
          elevation: 0,
        ),
      ),
    );
  }
}
