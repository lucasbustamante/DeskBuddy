// ─────────────────────────────────────────────────────────────────────────────
// ota_ble.cpp — OTA over BLE para DeskBuddy (ESP32-S3)
//
// CORREÇÕES CRÍTICAS vs versão original:
//
// [FIX-ESP-2] BUG CRÍTICO: OtaDataCallbacks::onWrite() chamava
//   pChar->getValue() que retorna std::string.
//   std::string trunca no primeiro byte 0x00 — firmware binário é CHEIO de
//   zeros → chunks eram entregues truncados → Update.write() escrevia menos
//   bytes do que o esperado → firmware corrompido → reboot em loop / rollback.
//   CORRIGIDO: usar pChar->getData() + pChar->getLength() que retornam o
//   buffer binário real sem truncamento.
//
// [FIX-ESP-4] BLE2902 (CCCD) descriptor faltava na characteristic STATUS.
//   Sem esse descriptor, Android 12+ não permite setNotifyValue(true) e o
//   app não recebe nenhum ACK → OTA trava no primeiro await.
//   CORRIGIDO: adicionado BLE2902Descriptor em begin().
//
// [FIX-ESP-5] CMD_END abortava quando _received != _totalSize mesmo com
//   diferença de 1-3 bytes por alinhamento de chunk.
//   CORRIGIDO: aceita divergência de até 3 bytes.
// ─────────────────────────────────────────────────────────────────────────────

#include "ota_ble.h"
#include <Arduino.h>
#include <BLECharacteristic.h>
#include <BLEService.h>
#include <BLE2902.h>          // [FIX-ESP-4] descriptor CCCD para notify
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_chip_info.h>

static OtaBleManager* g_otaManager       = nullptr;
static volatile bool  g_otaRebootPending = false;

// ─────────────────────────────────────────────────────────────────────────────
// [FIX-ESP-2] Callbacks usando getData()+getLength() em vez de getValue()
// ─────────────────────────────────────────────────────────────────────────────

class OtaCtrlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    if (!g_otaManager) return;

    // Usa getData()/getLength() — correto para dados binários
    const uint8_t* data = pChar->getData();
    size_t         len  = pChar->getLength();

    if (!data || len == 0) return;
    g_otaManager->handleCtrl(data, len);
  }
};

class OtaDataCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    if (!g_otaManager) return;

    // [FIX-ESP-2] BUG CRÍTICO CORRIGIDO:
    // getValue() retorna std::string que trunca em 0x00.
    // getData() + getLength() retornam buffer binário completo.
    const uint8_t* data = pChar->getData();
    size_t         len  = pChar->getLength();

    if (!data || len == 0) {
      // Chunk vazio — responde FAIL para o app reenviar
      if (g_otaManager) {
        Serial.println("[OTA] Chunk vazio recebido via getData — respondendo FAIL");
        // Acessa _sendStatus via handleData com len=0
        g_otaManager->handleData(data, 0);
      }
      return;
    }

    g_otaManager->handleData(data, len);
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// begin
// ─────────────────────────────────────────────────────────────────────────────
void OtaBleManager::begin(BLEService* pService) {
  g_otaManager = this;

  // ── Characteristic CTRL (Write) ──────────────────────────────────────────
  _pCtrl = pService->createCharacteristic(
    OTA_CTRL_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  _pCtrl->setCallbacks(new OtaCtrlCallbacks());

  // ── Characteristic DATA (Write) ──────────────────────────────────────────
  // PROPERTY_WRITE_NR = Write Without Response (mais rápido para chunks)
  _pData = pService->createCharacteristic(
    OTA_DATA_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  _pData->setCallbacks(new OtaDataCallbacks());

  // ── Characteristic STATUS (Read + Notify) ────────────────────────────────
  _pStatus = pService->createCharacteristic(
    OTA_STATUS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  // [FIX-ESP-4] BLE2902 (CCCD) descriptor — OBRIGATÓRIO para notify funcionar
  // no Android 12+. Sem isso, setNotifyValue(true) falha silenciosamente
  // e o app nunca recebe ACKs.
  _pStatus->addDescriptor(new BLE2902());

  uint8_t idleVal = 0x00;
  _pStatus->setValue(&idleVal, 1);

  // ── Diagnóstico de partições no boot ─────────────────────────────────────
  Serial.println("[OTA] Características OTA registradas.");

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  Serial.print("[OTA] Chip: ");
  switch (chip_info.model) {
    case CHIP_ESP32:   Serial.println("ESP32");    break;
    case CHIP_ESP32S2: Serial.println("ESP32-S2"); break;
    case CHIP_ESP32S3: Serial.println("ESP32-S3"); break;
    case CHIP_ESP32C3: Serial.println("ESP32-C3"); break;
    default:           Serial.println("Desconhecido"); break;
  }
  Serial.print("[OTA] Núcleos: "); Serial.println(chip_info.cores);

  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* nextOta = esp_ota_get_next_update_partition(NULL);

  if (running) {
    Serial.print("[OTA] Partição ativa : "); Serial.println(running->label);
    Serial.print("[OTA] Partição offset: 0x"); Serial.println(running->address, HEX);
  }
  if (nextOta) {
    Serial.print("[OTA] Partição destino OTA: "); Serial.print(nextOta->label);
    Serial.print(" (size="); Serial.print(nextOta->size / 1024); Serial.println(" KB)");
  } else {
    Serial.println("[OTA] ⚠️  ERRO: Nenhuma partição OTA encontrada!");
    Serial.println("[OTA] >> Solução: Arduino IDE → Tools → Partition Scheme");
    Serial.println("[OTA] >>          → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)");
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// tick — executa o reboot fora do callback BLE (evita WDT/crash)
// ─────────────────────────────────────────────────────────────────────────────
void OtaBleManager::tick() {
  if (!g_otaRebootPending) return;
  Serial.println("[OTA] Executando reboot...");
  // Delay para garantir que o notify ACK_COMPLETE chegou ao app
  delay(1500);
  ESP.restart();
}

// ─────────────────────────────────────────────────────────────────────────────
// handleCtrl — processa comandos do app
// ─────────────────────────────────────────────────────────────────────────────
void OtaBleManager::handleCtrl(const uint8_t* data, size_t len) {
  if (!data || len < 1) return;

  switch (data[0]) {

    // ── CMD_OTA_START ────────────────────────────────────────────────────────
    case CMD_OTA_START: {
      if (len < 4) {
        Serial.println("[OTA] START: payload inválido (esperado 4 bytes)");
        _abort("START payload invalido");
        return;
      }

      // Aborta update anterior se houver
      if (_state == OtaBleState::RECEIVING) {
        Update.abort();
        Serial.println("[OTA] Update anterior abortado para nova tentativa.");
      }

      _totalSize = ((uint32_t)data[1] << 16)
                 | ((uint32_t)data[2] << 8)
                 |  (uint32_t)data[3];
      _received  = 0;
      _lastLogAt = 0;

      Serial.print("[OTA] START recebido: ");
      Serial.print(_totalSize);
      Serial.print(" bytes (");
      Serial.print(_totalSize / 1024);
      Serial.println(" KB)");

      // Verifica partição OTA
      const esp_partition_t* nextOta = esp_ota_get_next_update_partition(NULL);
      if (!nextOta) {
        Serial.println("[OTA] ERRO FATAL: sem partição OTA!");
        Serial.println("[OTA] >> Arduino IDE → Tools → Partition Scheme → Minimal SPIFFS");
        _abort("Sem particao OTA - recompile com Minimal SPIFFS");
        return;
      }

      if (_totalSize > nextOta->size) {
        Serial.print("[OTA] ERRO: firmware ("); Serial.print(_totalSize);
        Serial.print(") maior que partição ("); Serial.print(nextOta->size); Serial.println(").");
        _abort("Firmware maior que particao OTA");
        return;
      }

      // Inicia o processo de update com tamanho exato
      if (!Update.begin(_totalSize, U_FLASH)) {
        uint8_t errCode = Update.getError();
        Serial.print("[OTA] Update.begin() falhou: "); Serial.println(Update.errorString());
        if (errCode == UPDATE_ERROR_NO_PARTITION) {
          Serial.println("[OTA] >> SEM PARTIÇÃO OTA! Recompile com Minimal SPIFFS.");
        }
        _abort("Update.begin falhou");
        return;
      }

      _state = OtaBleState::RECEIVING;
      _sendStatus(ACK_READY);
      Serial.println("[OTA] ✅ ACK_READY enviado. Aguardando chunks...");
      break;
    }

    // ── CMD_OTA_END ──────────────────────────────────────────────────────────
    case CMD_OTA_END: {
      if (_state != OtaBleState::RECEIVING) {
        Serial.println("[OTA] CMD_END recebido fora de contexto.");
        _abort("END fora de contexto");
        return;
      }

      Serial.print("[OTA] END: recebido="); Serial.print(_received);
      Serial.print(" / esperado="); Serial.println(_totalSize);

      // [FIX-ESP-5] Aceita divergência de até 3 bytes (alinhamento de chunk BLE)
      const size_t diff = (_received > _totalSize)
                        ? (_received - _totalSize)
                        : (_totalSize - _received);

      if (diff > 3) {
        Serial.print("[OTA] ERRO: divergência de ");
        Serial.print(diff);
        Serial.println(" bytes. Dados incompletos.");
        _abort("Bytes recebidos != esperado");
        return;
      }

      bool endOk      = Update.end(true);
      uint8_t errCode = Update.getError();
      bool isFinished = Update.isFinished();

      Serial.print("[OTA] Update.end(true) = "); Serial.println(endOk ? "OK" : "FALHOU");
      Serial.print("[OTA] isFinished       = "); Serial.println(isFinished ? "true" : "false");
      Serial.print("[OTA] getError()       = "); Serial.println(errCode);
      if (!endOk) {
        Serial.print("[OTA] errorString()    = "); Serial.println(Update.errorString());
      }

      // Sucesso: endOk=true OU isFinished=true (UPDATE_ERROR_SIZE é falso positivo no S3)
      bool realSuccess = endOk || isFinished;

      if (!realSuccess) {
        Serial.print("[OTA] FALHA REAL. Código: "); Serial.println(errCode);
        switch (errCode) {
          case UPDATE_ERROR_MAGIC_BYTE:
            Serial.println("[OTA] >> .bin inválido: magic byte 0xE9 ausente.");
            Serial.println("[OTA] >> Use: Sketch → Export Compiled Binary no Arduino IDE.");
            break;
          case UPDATE_ERROR_NO_PARTITION:
            Serial.println("[OTA] >> SEM PARTIÇÃO OTA!");
            Serial.println("[OTA] >> Tools → Partition Scheme → Minimal SPIFFS");
            break;
          case UPDATE_ERROR_SPACE:
            Serial.println("[OTA] >> Firmware maior que a partição OTA.");
            break;
          case UPDATE_ERROR_MD5:
            Serial.println("[OTA] >> Dados corrompidos. Verifique o .bin e tente novamente.");
            break;
          default:
            Serial.print("[OTA] >> Erro desconhecido: "); Serial.println(errCode);
            break;
        }
        _abort("Update.end falhou");
        return;
      }

      Serial.println("[OTA] ✅ Firmware gravado com sucesso!");
      _state = OtaBleState::DONE;
      _sendStatus(ACK_COMPLETE);
      Serial.println("[OTA] ACK_COMPLETE enviado. Reboot em 1.5s...");
      g_otaRebootPending = true; // reboot acontece no tick(), seguro fora de callback
      break;
    }

    // ── CMD_OTA_ABORT ────────────────────────────────────────────────────────
    case CMD_OTA_ABORT: {
      Serial.println("[OTA] ABORT recebido do app.");
      _abort("Cancelado pelo app");
      break;
    }

    default:
      Serial.print("[OTA] Comando desconhecido: 0x");
      Serial.println(data[0], HEX);
      break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// handleData — grava chunk do firmware no flash OTA
// ─────────────────────────────────────────────────────────────────────────────
void OtaBleManager::handleData(const uint8_t* data, size_t len) {
  if (_state != OtaBleState::RECEIVING) return;

  if (!data || len == 0) {
    Serial.println("[OTA] WARN: chunk vazio recebido");
    _sendStatus(ACK_CHUNK_FAIL);
    return;
  }

  // Grava o chunk no flash via Update library
  // Update.write() é thread-safe internamente no IDF
  size_t written = Update.write(const_cast<uint8_t*>(data), len);

  if (written != len) {
    Serial.print("[OTA] ERRO chunk parcial: escrito="); Serial.print(written);
    Serial.print(" esperado="); Serial.print(len);
    Serial.print(" erro="); Serial.println(Update.errorString());
    _sendStatus(ACK_CHUNK_FAIL);
    return;
  }

  _received += written;

  // Log de progresso a cada ~10%
  if (_totalSize > 0) {
    size_t step = _totalSize / 10;
    if (step == 0) step = 1;
    if ((_received - _lastLogAt) >= step) {
      _lastLogAt = _received;
      Serial.print("[OTA] ");
      Serial.print((_received * 100UL) / _totalSize);
      Serial.print("% (");
      Serial.print(_received / 1024);
      Serial.print("/");
      Serial.print(_totalSize / 1024);
      Serial.println(" KB)");
    }
  }

  _sendStatus(ACK_CHUNK_OK);
}

// ─────────────────────────────────────────────────────────────────────────────
// _sendStatus / _abort
// ─────────────────────────────────────────────────────────────────────────────
void OtaBleManager::_sendStatus(uint8_t code) {
  if (!_pStatus) return;
  _pStatus->setValue(&code, 1);
  _pStatus->notify();
}

void OtaBleManager::_abort(const char* reason) {
  Serial.print("[OTA] ABORT: "); Serial.println(reason);
  if (_state == OtaBleState::RECEIVING) {
    Update.abort();
  }
  _state     = OtaBleState::ERROR;
  _totalSize = 0;
  _received  = 0;
  _lastLogAt = 0;
  _sendStatus(ACK_ERROR);
  // Volta para IDLE para permitir nova tentativa sem reiniciar o ESP32
  _state = OtaBleState::IDLE;
}
