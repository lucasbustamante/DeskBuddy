#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ota_ble.h — OTA over BLE para DeskBuddy (ESP32-S3)
//
// CORREÇÕES APLICADAS:
//
// [FIX-ESP-1] BLECharacteristic::getValue() retorna std::string, não Arduino
//             String. Em versões mais recentes do ESP32 Arduino Core, o cast
//             direto pode truncar dados binários em bytes 0x00. Corrigido
//             usando getValue() com ponteiro+tamanho.
//
// [FIX-ESP-2] OtaDataCallbacks::onWrite() usava pChar->getValue() como String
//             o que truncava o chunk no primeiro byte 0x00 encontrado.
//             Firmware ESP32 binário tem muitos zeros — isso corrompía TODO
//             o OTA silenciosamente. Corrigido com getData()+getLength().
//
// [FIX-ESP-3] handleData() verificava written != len mas não logava o offset
//             real. Adicionado log detalhado para debug.
//
// [FIX-ESP-4] _sendStatus() não adicionava BLE2902 descriptor (CCCD) na
//             característica STATUS. Sem ele, o app não recebe notificações
//             no Android 12+. Adicionado em begin().
//
// [FIX-ESP-5] handleCtrl CMD_END: _received != _totalSize causava abort mesmo
//             quando a diferença era padding BLE (1-3 bytes). Relaxado para
//             aceitar diferença de até 3 bytes (chunk size boundary).
//
// PROTOCOLO:
//   App → [0x01, szH, szM, szL]   CMD_START (ctrl characteristic)
//   ESP → [0xA0]                   ACK_READY (status notify)
//   App → [chunk bytes...]         dados (data characteristic)
//   ESP → [0xA1]                   ACK_CHUNK_OK
//   ... repete para cada chunk ...
//   App → [0x02]                   CMD_END (ctrl characteristic)
//   ESP → [0xA3]                   ACK_COMPLETE → ESP reinicia via tick()
//
// IMPORTANTE:
//   - Chame otaManager.begin(pService) ANTES de pService->start()
//   - Chame otaManager.tick() no loop() principal
//   - O reboot NUNCA ocorre dentro de callback BLE (causa crash/WDT)
// ─────────────────────────────────────────────────────────────────────────────

#ifndef OTA_BLE_H
#define OTA_BLE_H

#include <BLECharacteristic.h>
#include <BLEDescriptor.h>
#include <Update.h>

// ── Comandos App → ESP32 ─────────────────────────────────────────────────────
static constexpr uint8_t CMD_OTA_START  = 0x01;
static constexpr uint8_t CMD_OTA_END    = 0x02;
static constexpr uint8_t CMD_OTA_ABORT  = 0x03;

// ── Respostas ESP32 → App ────────────────────────────────────────────────────
static constexpr uint8_t ACK_READY      = 0xA0;
static constexpr uint8_t ACK_CHUNK_OK   = 0xA1;
static constexpr uint8_t ACK_CHUNK_FAIL = 0xA2;
static constexpr uint8_t ACK_COMPLETE   = 0xA3;
static constexpr uint8_t ACK_ERROR      = 0xAF;

// ── UUIDs OTA (mesmo serviço base, novas characteristics) ────────────────────
static constexpr const char* OTA_CTRL_UUID   = "6e400010-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char* OTA_DATA_UUID   = "6e400011-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char* OTA_STATUS_UUID = "6e400012-b5a3-f393-e0a9-e50e24dcca9e";

// ─────────────────────────────────────────────────────────────────────────────

enum class OtaBleState : uint8_t {
  IDLE      = 0,
  RECEIVING = 1,
  DONE      = 2,
  ERROR     = 3
};

class OtaBleManager {
public:
  OtaBleManager() = default;

  // Registra as 3 características OTA no serviço BLE.
  // DEVE ser chamado antes de pService->start().
  void begin(BLEService* pService);

  // Chame no loop() principal para executar o reboot pendente.
  // O reboot NUNCA deve ocorrer dentro de um callback BLE.
  void tick();

  bool isActive() const { return _state == OtaBleState::RECEIVING; }
  bool isDone()   const { return _state == OtaBleState::DONE; }
  bool isIdle()   const { return _state == OtaBleState::IDLE; }

  // Chamados pelas classes de callback (público para acesso interno)
  void handleCtrl(const uint8_t* data, size_t len);
  void handleData(const uint8_t* data, size_t len);

private:
  BLECharacteristic* _pCtrl   = nullptr;
  BLECharacteristic* _pData   = nullptr;
  BLECharacteristic* _pStatus = nullptr;

  OtaBleState _state     = OtaBleState::IDLE;
  size_t      _totalSize = 0;
  size_t      _received  = 0;
  size_t      _lastLogAt = 0;

  void _sendStatus(uint8_t code);
  void _abort(const char* reason);
};

#endif // OTA_BLE_H
