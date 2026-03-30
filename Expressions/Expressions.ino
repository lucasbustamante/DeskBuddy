// ─────────────────────────────────────────────────────────────────────────────
// Expressions.ino — DeskBuddy firmware com suporte a OTA via BLE
// TARGET: ESP32-S3
//
// ── PINAGEM ESP32-S3 ─────────────────────────────────────────────────────────
//   I2C SDA     → GPIO8
//   I2C SCL     → GPIO9
//   Botão       → GPIO4
//   Buzzer      → GPIO5
//   Bateria ADC → GPIO3  (ADC1_CH2)
//   LDR ADC     → GPIO6  (ADC1_CH5)
//   OLED        → 0x3C via I2C
//   MPU         → 0x68 via I2C
//
// ── CONFIGURAÇÃO ARDUINO IDE ─────────────────────────────────────────────────
//   Board           : ESP32S3 Dev Module
//   Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
//   PSRAM           : Disabled (ou conforme seu módulo)
//
// ── MODIFICAÇÕES OTA ─────────────────────────────────────────────────────────
//   1. #include "ota_ble.h"
//   2. OtaBleManager otaManager; (instância global)
//   3. otaManager.begin(pService); no setup (antes de pService->start())
//   4. getHumorJSON() inclui "fw_version" para o app exibir
//   5. loop() retorna cedo se OTA estiver ativo
// ─────────────────────────────────────────────────────────────────────────────

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEAddress.h>
#include <BLEAdvertisedDevice.h>

#include <EEPROM.h>
#include "images.h"
#include "controller.h"
#ifdef TAM_NOME
#undef TAM_NOME
#endif

#include "BuzzerScheduler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

// ── OTA ADD ──────────────────────────────────────────────────────────────────
#include "ota_ble.h"
#include <esp_ota_ops.h>  // esp_ota_mark_app_valid_cancel_rollback()
// Versão do firmware atual — atualize antes de gerar cada .bin
static constexpr const char* FW_VERSION = "1.0.2-s3";
// ─────────────────────────────────────────────────────────────────────────────

// ======================================================
//  ✅ CONFIG CENTRAL (AJUSTE TUDO AQUI)
// ======================================================
namespace CFG {

  // -------- Identidade --------
  static constexpr const char* NAME  = "Kizmo";
  static constexpr const char* SENHA = "oi23";

  // -------- Display / I2C --------
  static constexpr int SCREEN_WIDTH  = 128;
  static constexpr int SCREEN_HEIGHT = 64;
  static constexpr int OLED_RESET    = -1;
  static constexpr uint8_t OLED_ADDR = 0x3C;

  // ESP32-S3: pinos I2C livres (evita GPIO19/20 = USB D+/D-)
  static constexpr int I2C_SDA = 8;
  static constexpr int I2C_SCL = 9;

  // -------- IO --------
  // ESP32-S3: GPIO0 é strapping — usar GPIO1+ para botão
  static constexpr int BUTTON_PIN = 4;
  // ESP32-S3: LEDC disponível em qualquer GPIO, usando GPIO5
  static constexpr int BUZZER_PIN = 5;

  // -------- Bateria (ADC) --------
  // ESP32-S3: ADC1 (GPIO1–GPIO10). ADC2 NÃO funciona com BLE ativo.
  // Usando GPIO3 (ADC1_CH2) — longe dos pinos de strapping
  static constexpr int BAT_ADC_PIN = 3;

  static constexpr float BAT_R_TOP    = 330000.0f;
  static constexpr float BAT_R_BOTTOM = 100000.0f;

  static constexpr float BAT_FULL_V  = 3.70f;
  static constexpr float BAT_EMPTY_V = 2.50f;

  static constexpr int   BAT_LOW_PERCENT = 15;
  static constexpr unsigned long BAT_READ_EVERY_MS = 5000;

  static constexpr int   BAT_SAMPLE_COUNT = 7;
  static constexpr float BAT_EMA_ALPHA = 0.25f;
  static constexpr float BAT_GLITCH_JUMP_V = 0.35f;
  static constexpr int   BAT_LOW_STREAK_CYCLES = 3;
  static constexpr int   BAT_OK_STREAK_CYCLES  = 2;
  static constexpr int   BAT_HYSTERESIS_PERCENT = 5;

  // ======================================================
  //  LDR
  // ======================================================
  // ESP32-S3: ADC1_CH6 = GPIO6 — ainda no ADC1, seguro com BLE
  static constexpr int LDR_ADC_PIN = 6;
  static constexpr unsigned long LDR_READ_EVERY_MS = 300;
  static constexpr int LDR_DARK_MV_THRESHOLD  = 180;
  static constexpr int LDR_WAKE_MV_THRESHOLD  = 280;
  static constexpr unsigned long LDR_DARK_DEBOUNCE_MS = 2500;

  static constexpr uint8_t SLEEP_HOLD_CYCLES = 10;
  static constexpr unsigned long SLEEP_SOUND_LOOP_MS = 6000;

  // -------- EEPROM --------
  static constexpr int EEPROM_SIZE = 1024;

  // -------- BLE UUIDs (mantidos — NÃO alterar) --------
  static constexpr const char* SERVICE_UUID        = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  static constexpr const char* CHARACTERISTIC_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  static constexpr const char* LOVE_PROPOSAL_UUID  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
  static constexpr const char* LOVE_RESPONSE_UUID  = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";

  // ── OTA UUIDs ficam em ota_ble.h (OTA_CTRL_UUID / OTA_DATA_UUID / OTA_STATUS_UUID)

  // -------- Buffers / limites --------
  static constexpr int MAX_ENCONTRADOS = 10;
  static constexpr int TAM_NOME_MAX    = 16;

  // -------- Tempos base --------
  static constexpr unsigned long ANIM_INTERVAL       = 7000;
  static constexpr unsigned long DECAY_INTERVAL_MS   = 3000;
  static constexpr unsigned long BUTTON_DEBOUNCE_MS  = 500;

  // -------- Regras de relação --------
  static constexpr int INTERACOES_PRA_DEFINIR_RELACAO = 2;
  static constexpr int INTERACOES_PRA_SEGUNDA_CHANCE  = 6;
  static constexpr int INTERACOES_PRA_APAIXONAR       = 10;

  static constexpr int CHANCE_GOSTAR_PRIMEIRA_PERCENT = 70;
  static constexpr int CHANCE_GOSTAR_SEGUNDA_PERCENT  = 50;
  static constexpr int CHANCE_VIRAR_AMOR_PERCENT      = 25;

  static constexpr unsigned long LOVE_MISSING_DECAY_START_MS = 120000;
  static constexpr unsigned long LOVE_MISSING_STEP_MS        = 30000;
  static constexpr int LOVE_MISSING_DECAY_AMOUNT             = 3;
  static constexpr int LOVE_ON_ACCEPT_GAIN                   = 15;

  static constexpr int CARINHO_UP_FELIZ     = 3;
  static constexpr int CARINHO_DOWN_OUTRAS = 1;

  static constexpr int DECAY_FELIZ_SUB          = 5;
  static constexpr int DECAY_TRISTE_ADD         = 1;
  static constexpr int DECAY_ENTEDIADO_ADD      = 1;
  static constexpr int DECAY_AMOR_SEM_PARCEIRO  = 1;

  static constexpr unsigned long GLOBAL_SOUND_GAP_MS = 150;
  static constexpr unsigned long DISPLAY_SOUND_MIN_GAP_CYCLES = 1;
  static constexpr unsigned long AUTO_SOUND_MIN_GAP_CYCLES    = 5;

  static constexpr unsigned long HANDSHAKE_COOLDOWN_MS = 30000;

  static constexpr unsigned long SCAN_INTERVAL_DISCONNECTED_MS = 1200;
  static constexpr unsigned long SCAN_INTERVAL_CONNECTED_MS    = 6000;
  static constexpr uint32_t SCAN_DURATION_SEC                  = 1;

  static constexpr unsigned long MPU_READ_EVERY_MS = 40;
  static constexpr float WALK_STEP_G_THRESHOLD = 0.18f;
  static constexpr unsigned long WALK_STEP_MIN_INTERVAL_MS = 280;
  static constexpr unsigned long WALK_EVENT_COOLDOWN_MS    = 1800;
  static constexpr int WALK_STEPS_TO_TRIGGER               = 3;

  static constexpr float SHAKE_G_THRESHOLD = 1.10f;
  static constexpr unsigned long SHAKE_EVENT_COOLDOWN_MS = 2500;

  static constexpr int NAUSEA_DOWN_FELIZ  = 4;
  static constexpr int NAUSEA_UP_ENTEDIO  = 3;
  static constexpr int NAUSEA_UP_TRISTE   = 1;
  static constexpr int NAUSEA_UP_BRAVO    = 1;

} // namespace CFG

// ── OTA ADD: instância global do manager ─────────────────────────────────────
OtaBleManager otaManager;
// ─────────────────────────────────────────────────────────────────────────────

// =================== MPU (raw) ===================
static uint8_t MPU_ADDR = 0x68;

static uint8_t mpu_rd(uint8_t reg){
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  Wire.requestFrom(MPU_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

static void mpu_wr(uint8_t reg, uint8_t val){
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static int16_t mpu_rd16(uint8_t reg){
  uint8_t hi = mpu_rd(reg);
  uint8_t lo = mpu_rd(reg + 1);
  return (int16_t)((hi << 8) | lo);
}

static bool mpu_begin_raw(){
  mpu_wr(0x6B, 0x00);
  delay(10);
  if (mpu_rd(0x6B) != 0x00) return false;
  mpu_wr(0x1B, 0x00);
  mpu_wr(0x1C, 0x00);
  mpu_wr(0x1A, 0x03);
  return true;
}

static void mpu_read_g(float &ax_g, float &ay_g, float &az_g, float &gx_dps, float &gy_dps, float &gz_dps){
  int16_t ax = mpu_rd16(0x3B);
  int16_t ay = mpu_rd16(0x3D);
  int16_t az = mpu_rd16(0x3F);
  int16_t gx = mpu_rd16(0x43);
  int16_t gy = mpu_rd16(0x45);
  int16_t gz = mpu_rd16(0x47);
  ax_g  = (float)ax / 16384.0f;
  ay_g  = (float)ay / 16384.0f;
  az_g  = (float)az / 16384.0f;
  gx_dps = (float)gx / 131.0f;
  gy_dps = (float)gy / 131.0f;
  gz_dps = (float)gz / 131.0f;
}

// ======================================================
//  MULTITASK
// ======================================================
portMUX_TYPE buzzerMux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE uiMux = portMUX_INITIALIZER_UNLOCKED;

static char g_autoEmotion[16] = "normal";
static bool g_overrideActive = false;
static char g_overrideEmotion[16] = "";
static char g_currentShownEmotion[16] = "normal";

static bool g_screenSoundPending = false;
static char g_screenSoundEmotion[16] = "";
static bool g_screenSoundShort = false;

static TaskHandle_t uiTaskHandle = nullptr;

static const unsigned long DISPLAY_SOUND_MIN_GAP_MS = (unsigned long)CFG::DECAY_INTERVAL_MS * CFG::DISPLAY_SOUND_MIN_GAP_CYCLES;
static const unsigned long AUTO_SOUND_MIN_GAP_MS    = (unsigned long)CFG::DECAY_INTERVAL_MS * CFG::AUTO_SOUND_MIN_GAP_CYCLES;

// ======================================================
//  LDR — Estado de Dormir
// ======================================================
enum SleepState : uint8_t { AWAKE = 0, SLEEP_HOLD = 1, SLEEPING = 2, WAKE_HOLD = 3 };
static volatile bool g_displaySleeping = false;
static SleepState g_sleepState = AWAKE;
static unsigned long g_lastLdrReadMs = 0;
static int g_ldrMv = 0;
static unsigned long g_darkSinceMs = 0;
static unsigned long g_sleepPrepStartMs = 0;
static unsigned long g_sleepSoundStopMs = 0;
static int g_sleepHoldCyclesLeft = 0;
static int g_wakeHoldCyclesLeft  = 0;
static unsigned long g_lastSleepLoopSoundMs = 0;

// ======================================================
//  OBJETOS
// ======================================================
Adafruit_SSD1306 display(CFG::SCREEN_WIDTH, CFG::SCREEN_HEIGHT, &Wire, CFG::OLED_RESET);
BLECharacteristic *pCharacteristic = nullptr;
BLECharacteristic *pLoveProposal   = nullptr;
BLECharacteristic *pLoveResponse   = nullptr;

BuzzerScheduler buzzer;

volatile bool bleConnected = false;
unsigned long lastScanMs = 0;

// ======================================================
//  ESTADO HUMOR
// ======================================================
int pctFeliz = 70, pctTriste = 10, pctEntediado = 10, pctBravo = 10, pctNormal = 0, pctApaixonado = 0;

unsigned long lastInteractionTime = 0;
unsigned long lastButtonTime = 0;

String estadoDisplay = "";
unsigned long estadoDisplayTimeout = 0;

String forcedEmotion = "";

String encontrados[CFG::MAX_ENCONTRADOS];
int idxEncontrado = 0;
int countEncontrados = 0;

Relacao relacoes[CFG::MAX_ENCONTRADOS];

static unsigned long lastAnySoundMs     = 0;
static unsigned long lastDisplaySoundMs = 0;
static unsigned long lastAutoSoundMs    = 0;

static String currentPartner = "";
static unsigned long lastSeenPartnerMs = 0;

static unsigned long lastHandshakeAttemptMs[CFG::MAX_ENCONTRADOS] = {0};

// ======================================================
//  BATERIA
// ======================================================
static int g_batteryPercent = 100;
static float g_batteryVoltage = 0.0f;
static bool g_lowBattery = false;
static unsigned long lastBatteryReadMs = 0;
static float g_batteryVoltageFiltered = 0.0f;
static int   g_batteryPercentFiltered = 100;
static float g_lastBatteryRawV = 0.0f;
static int   g_batLowStreak = 0;
static int   g_batOkStreak  = 0;

// ======================================================
//  MPU9250
// ======================================================
static unsigned long lastMpuReadMs = 0;
static unsigned long lastStepMs = 0;
static int stepCounter = 0;
static unsigned long lastWalkTriggerMs = 0;
static unsigned long lastShakeTriggerMs = 0;
static bool mpuOk = false;

// ======================================================
//  Forward declarations
// ======================================================
static void requestOverrideEmotion(const char *emo, bool requestSound, bool shortVariant);
static void showEmoteOnDisplay();
void normalizaEmocoesAvancada(bool acaoFoiFeliz=false, bool acaoFoiTriste=false, bool acaoFoiBravo=false, bool acaoFoiEntediado=false, bool acaoFoiApaixonado=false);
static void saveHumorToEEPROM();

// ======================================================
//  LDR helpers
// ======================================================
static int readLdrMilliVolts() {
  return (int)analogReadMilliVolts(CFG::LDR_ADC_PIN);
}

static void displaySetSleeping(bool sleeping) {
  g_displaySleeping = sleeping;
  if (sleeping) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  } else {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.display();
  }
}

static void stopAllSoundsNow() {
  portENTER_CRITICAL(&buzzerMux);
  buzzer.stop();
  portEXIT_CRITICAL(&buzzerMux);
}

static void ensureSleepSoundLoop() {
  if (g_displaySleeping) return;
  const unsigned long now = millis();
  if (buzzer.isPlaying()) return;
  if (now - g_lastSleepLoopSoundMs < CFG::SLEEP_SOUND_LOOP_MS) return;
  portENTER_CRITICAL(&buzzerMux);
  buzzer.playSound(S_SLEEPY2);
  lastAnySoundMs = now;
  lastDisplaySoundMs = now;
  g_lastSleepLoopSoundMs = now;
  portEXIT_CRITICAL(&buzzerMux);
}

// ======================================================
//  BATERIA helpers
// ======================================================
static float readBatteryVoltageRobust() {
  float samples[CFG::BAT_SAMPLE_COUNT];
  for (int i = 0; i < CFG::BAT_SAMPLE_COUNT; i++) {
    uint32_t mv = analogReadMilliVolts(CFG::BAT_ADC_PIN);
    float v_adc = mv / 1000.0f;
    samples[i] = v_adc * (CFG::BAT_R_TOP + CFG::BAT_R_BOTTOM) / CFG::BAT_R_BOTTOM;
    delay(3);
  }
  for (int i = 0; i < CFG::BAT_SAMPLE_COUNT - 1; i++) {
    for (int j = i + 1; j < CFG::BAT_SAMPLE_COUNT; j++) {
      if (samples[j] < samples[i]) { float tmp = samples[i]; samples[i] = samples[j]; samples[j] = tmp; }
    }
  }
  return samples[CFG::BAT_SAMPLE_COUNT / 2];
}

static int batteryPercentFromVoltage(float vbat) {
  float p = (vbat - CFG::BAT_EMPTY_V) / (CFG::BAT_FULL_V - CFG::BAT_EMPTY_V) * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)(p + 0.5f);
}

static void updateBatteryIfNeeded() {
  unsigned long now = millis();
  if (now - lastBatteryReadMs < CFG::BAT_READ_EVERY_MS) return;
  lastBatteryReadMs = now;

  float rawV = readBatteryVoltageRobust();

  if (g_lastBatteryRawV > 0.5f) {
    float jump = fabs(rawV - g_lastBatteryRawV);
    if (jump >= CFG::BAT_GLITCH_JUMP_V) return;
  }
  g_lastBatteryRawV = rawV;

  if (g_batteryVoltageFiltered <= 0.1f) {
    g_batteryVoltageFiltered = rawV;
  } else {
    g_batteryVoltageFiltered = (CFG::BAT_EMA_ALPHA * rawV) + ((1.0f - CFG::BAT_EMA_ALPHA) * g_batteryVoltageFiltered);
  }

  g_batteryVoltage = g_batteryVoltageFiltered;
  g_batteryPercent = batteryPercentFromVoltage(g_batteryVoltage);
  g_batteryPercentFiltered = g_batteryPercent;

  bool wasLow = g_lowBattery;
  const int lowTh = CFG::BAT_LOW_PERCENT;
  const int okTh  = CFG::BAT_LOW_PERCENT + CFG::BAT_HYSTERESIS_PERCENT;

  if (g_batteryPercentFiltered < lowTh) { g_batLowStreak++; g_batOkStreak = 0; }
  else if (g_batteryPercentFiltered > okTh) { g_batOkStreak++; g_batLowStreak = 0; }
  else { g_batLowStreak = 0; g_batOkStreak = 0; }

  if (!g_lowBattery && g_batLowStreak >= CFG::BAT_LOW_STREAK_CYCLES) g_lowBattery = true;
  if (g_lowBattery && g_batOkStreak >= CFG::BAT_OK_STREAK_CYCLES) g_lowBattery = false;

  if (!wasLow && g_lowBattery) requestOverrideEmotion("fome", true, false);
}

// ======================================================
//  EEPROM
// ======================================================
void limpaTodaEEPROM() {
  EEPROM.begin(CFG::EEPROM_SIZE);
  for (int i = 0; i < CFG::EEPROM_SIZE; i++) EEPROM.write(i, 0x00);
  EEPROM.commit();
}

void salvaRelacoesEEPROM() { int addr = 10; EEPROM.put(addr, relacoes); EEPROM.commit(); }
void carregaRelacoesEEPROM() { int addr = 10; EEPROM.get(addr, relacoes); }

void salvaBufferEncontradosEEPROM() {
  int addr = 10 + sizeof(relacoes);
  EEPROM.put(addr, encontrados); addr += sizeof(encontrados);
  EEPROM.put(addr, idxEncontrado); addr += sizeof(idxEncontrado);
  EEPROM.put(addr, countEncontrados);
  EEPROM.commit();
}

void carregaBufferEncontradosEEPROM() {
  int addr = 10 + sizeof(relacoes);
  EEPROM.get(addr, encontrados); addr += sizeof(encontrados);
  EEPROM.get(addr, idxEncontrado); addr += sizeof(idxEncontrado);
  EEPROM.get(addr, countEncontrados);
}

void inicializaEEPROMSempre() {
  EEPROM.begin(CFG::EEPROM_SIZE);
  for (int i = 0; i <= 5; i++) EEPROM.write(i, (i == 0) ? 70 : 10);
  EEPROM.write(4, 0); EEPROM.write(5, 0);
  EEPROM.commit();
}

void loadHumorFromEEPROM() {
  EEPROM.begin(CFG::EEPROM_SIZE);
  pctFeliz      = EEPROM.read(0);
  pctTriste     = EEPROM.read(1);
  pctEntediado  = EEPROM.read(2);
  pctBravo      = EEPROM.read(3);
  pctNormal     = EEPROM.read(4);
  pctApaixonado = EEPROM.read(5);
  carregaRelacoesEEPROM();
  carregaBufferEncontradosEEPROM();
}

void saveHumorToEEPROM() {
  EEPROM.write(0, pctFeliz); EEPROM.write(1, pctTriste);
  EEPROM.write(2, pctEntediado); EEPROM.write(3, pctBravo);
  EEPROM.write(4, pctNormal); EEPROM.write(5, pctApaixonado);
  EEPROM.commit();
  salvaRelacoesEEPROM();
  salvaBufferEncontradosEEPROM();
}

// ======================================================
//  UTIL
// ======================================================
int buscaRelacao(const String& nome) {
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++)
    if (String(relacoes[i].nome) == nome) return i;
  return -1;
}

int defineRelacaoIndex(const String& nome) {
  int idx = buscaRelacao(nome);
  if (idx == -1) {
    for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) {
      if (relacoes[i].nome[0] == 0) {
        nome.toCharArray(relacoes[i].nome, CFG::TAM_NOME_MAX);
        relacoes[i] = { "", false, 0, false, false, false, 0 };
        nome.toCharArray(relacoes[i].nome, CFG::TAM_NOME_MAX);
        salvaRelacoesEEPROM();
        return i;
      }
    }
  }
  return idx;
}

String extraiNomeDeskBuddy(String full) {
  int idx = full.indexOf(":");
  if (idx != -1 && idx + 2 < full.length()) return full.substring(idx + 2);
  return full;
}

bool jaTemNomeNoBuffer(String nome) {
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) if (encontrados[i] == nome) return true;
  return false;
}

static inline void clampAll() {
  pctFeliz      = constrain(pctFeliz, 0, 100);
  pctTriste     = constrain(pctTriste, 0, 100);
  pctEntediado  = constrain(pctEntediado, 0, 100);
  pctBravo      = constrain(pctBravo, 0, 100);
  pctNormal     = constrain(pctNormal, 0, 100);
  pctApaixonado = constrain(pctApaixonado, 0, 100);
}

void normalizaEmocoesAvancada(bool acaoFoiFeliz, bool acaoFoiTriste, bool acaoFoiBravo, bool acaoFoiEntediado, bool acaoFoiApaixonado) {
  if (acaoFoiApaixonado && pctApaixonado > 50) {
    if (pctTriste > 0) pctTriste = max(0, pctTriste - 1);
    if (pctEntediado > 0) pctEntediado = max(0, pctEntediado - 1);
    if (pctBravo > 0) pctBravo = max(0, pctBravo - 1);
  }
  int maxNeg = max(pctTriste, pctBravo);
  if (acaoFoiFeliz && pctFeliz + maxNeg > 100) {
    int excesso = pctFeliz + maxNeg - 100;
    if (pctTriste >= pctBravo) pctTriste = max(0, pctTriste - excesso);
    else pctBravo = max(0, pctBravo - excesso);
  }
  clampAll();
}

static String getRealDominantEmotion() {
  if (forcedEmotion.length() > 0) return forcedEmotion;
  struct Pair { int v; const char* n; };
  Pair lista[] = {
    {pctApaixonado, "apaixonado"}, {pctFeliz, "feliz"},
    {pctTriste, "triste"}, {pctEntediado, "entediado"}, {pctBravo, "bravo"}
  };
  int best = 0;
  for (int i = 1; i < 5; i++) if (lista[i].v > lista[best].v) best = i;
  if (lista[best].v > 50) return String(lista[best].n);
  return "normal";
}

String getDominantEmotion() {
  if (forcedEmotion.length() > 0) return forcedEmotion;
  String realDom = getRealDominantEmotion();
  if (!g_lowBattery) return realDom;
  static const unsigned long FOME_SHOW_MS = 2000;
  static const unsigned long REAL_SHOW_MS = 2000;
  static const unsigned long CYCLE_MS = FOME_SHOW_MS + REAL_SHOW_MS;
  unsigned long t = millis() % CYCLE_MS;
  if (t < FOME_SHOW_MS) return "fome";
  return realDom;
}

// ── OTA ADD: inclui fw_version no JSON ───────────────────────────────────────
String getHumorJSON() {
  String json = "{";
  json += "\"feliz\":"      + String(pctFeliz)      + ",";
  json += "\"triste\":"     + String(pctTriste)     + ",";
  json += "\"entediado\":"  + String(pctEntediado)  + ",";
  json += "\"bravo\":"      + String(pctBravo)      + ",";
  json += "\"normal\":"     + String(pctNormal)     + ",";
  json += "\"apaixonado\":" + String(pctApaixonado) + ",";
  json += "\"dominante\":\"" + getDominantEmotion() + "\",";
  json += "\"nome\":\"" + String(CFG::NAME) + "\",";
  json += "\"senha\":\"" + String(CFG::SENHA) + "\",";
  json += "\"bateria_pct\":" + String(g_batteryPercent) + ",";
  json += "\"bateria_v\":"   + String(g_batteryVoltage, 3) + ",";
  json += "\"bateria_low\":" + String(g_lowBattery ? "true" : "false") + ",";
  json += "\"ble_app_conectado\":" + String(bleConnected ? "true" : "false") + ",";
  json += "\"sleeping\":"   + String(g_displaySleeping ? "true" : "false") + ",";
  json += "\"ldr_mv\":"     + String(g_ldrMv) + ",";
  json += "\"fw_version\":\"" + String(FW_VERSION) + "\","; // ← OTA ADD
  json += "\"parceiro\":\"" + currentPartner + "\",";
  json += "\"encontrados\":[";
  bool first = true;
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) {
    String nome = encontrados[i];
    if (nome.length() > 0) {
      if (!first) json += ",";
      json += "{\"nome\":\"" + nome + "\"";
      int idx = buscaRelacao(nome);
      if (idx != -1 && relacoes[idx].relacaoDefinida) {
        json += ",\"gosta\":" + String(relacoes[idx].gosta ? "true" : "false");
        json += ",\"apaixonado\":" + String(relacoes[idx].apaixonado ? "true" : "false");
        json += ",\"afinidade\":" + String(relacoes[idx].afinidade);
      }
      json += "}";
      first = false;
    }
  }
  json += "]}";
  return json;
}
// ─────────────────────────────────────────────────────────────────────────────

// ======================================================
//  UI Task helpers
// ======================================================
static void setAutoEmotion(const String &emo) {
  portENTER_CRITICAL(&uiMux);
  strncpy(g_autoEmotion, emo.c_str(), sizeof(g_autoEmotion) - 1);
  g_autoEmotion[sizeof(g_autoEmotion) - 1] = 0;
  portEXIT_CRITICAL(&uiMux);
}

static void requestOverrideEmotion(const char *emo, bool requestSound, bool shortVariant) {
  portENTER_CRITICAL(&uiMux);
  g_overrideActive = true;
  strncpy(g_overrideEmotion, emo, sizeof(g_overrideEmotion) - 1);
  g_overrideEmotion[sizeof(g_overrideEmotion) - 1] = 0;
  if (requestSound && !g_displaySleeping) {
    g_screenSoundPending = true;
    strncpy(g_screenSoundEmotion, emo, sizeof(g_screenSoundEmotion) - 1);
    g_screenSoundEmotion[sizeof(g_screenSoundEmotion) - 1] = 0;
    g_screenSoundShort = shortVariant;
  }
  portEXIT_CRITICAL(&uiMux);
}

static void runEmotionAnimation(const char *emo, int xx=0, int yy=0, int tt=75) {
  if (strcmp(emo, "apaixonado") == 0) loving(xx, yy, tt);
  else if (strcmp(emo, "feliz") == 0) happy(xx, yy, tt);
  else if (strcmp(emo, "triste") == 0) sad(xx, yy, tt);
  else if (strcmp(emo, "entediado") == 0) bored(xx, yy, tt);
  else if (strcmp(emo, "bravo") == 0) angry(xx, yy, tt);
  else if (strcmp(emo, "suspeita") == 0) suspicion(xx, yy, tt);
  else if (strcmp(emo, "fome") == 0) hunger(xx, yy, tt);
  else if (strcmp(emo, "enjoado") == 0) nauseous(xx, yy, tt);
  else if (strcmp(emo, "dormindo") == 0) zzz(xx, yy, tt);
  else normal(xx, yy, tt);
}

// ======================================================
//  BUZZER
// ======================================================
uint8_t soundForEmotion(const String &emocao, bool variantShort=false) {
  if (emocao == "feliz")      return variantShort ? S_HAPPY_SHORT : S_HAPPY;
  if (emocao == "triste")     return S_SAD;
  if (emocao == "entediado")  return S_SLEEPING;
  if (emocao == "bravo")      return S_MODE3;
  if (emocao == "apaixonado") return S_CUDDLY;
  if (emocao == "fome")       return S_ANGRY;
  if (emocao == "suspeita")   return S_CONNECTION;
  if (emocao == "enjoado")    return S_SURPRISE;
  return S_CONNECTION;
}

static inline bool canStartAnySound(unsigned long now, bool isDisplay) {
  if (buzzer.isPlaying()) return false;
  if (now - lastAnySoundMs < CFG::GLOBAL_SOUND_GAP_MS) return false;
  if (isDisplay) { if (now - lastDisplaySoundMs < DISPLAY_SOUND_MIN_GAP_MS) return false; }
  else { if (now - lastAutoSoundMs < AUTO_SOUND_MIN_GAP_MS) return false; }
  return true;
}

static void startSoundLocked(uint8_t snd, bool isDisplay) {
  const unsigned long now = millis();
  buzzer.playSound(snd);
  lastAnySoundMs = now;
  if (isDisplay) lastDisplaySoundMs = now;
  else lastAutoSoundMs = now;
}

static void tryPlayScreenSoundSynced(const char *emo, bool shortVariant) {
  if (g_displaySleeping) return;
  const unsigned long now = millis();
  portENTER_CRITICAL(&buzzerMux);
  bool ok = canStartAnySound(now, true);
  if (ok) startSoundLocked(soundForEmotion(String(emo), shortVariant), true);
  portEXIT_CRITICAL(&buzzerMux);
}

static String lastAutoEmotion = "";

static void tryPlayAutoDominantSound(const String &dominante) {
  if (g_displaySleeping) return;
  const unsigned long now = millis();
  if (now - lastDisplaySoundMs < DISPLAY_SOUND_MIN_GAP_MS) return;
  const bool emotionChanged = (dominante != lastAutoEmotion);
  if (emotionChanged) {
    portENTER_CRITICAL(&buzzerMux);
    bool ok = canStartAnySound(now, false);
    if (ok) { lastAutoEmotion = dominante; const bool sv = (dominante == "feliz" || dominante == "apaixonado"); startSoundLocked(soundForEmotion(dominante, sv), false); }
    else lastAutoEmotion = dominante;
    portEXIT_CRITICAL(&buzzerMux);
  } else {
    portENTER_CRITICAL(&buzzerMux);
    bool ok = canStartAnySound(now, false);
    if (ok) { const bool sv = (dominante == "feliz" || dominante == "apaixonado"); startSoundLocked(soundForEmotion(dominante, sv), false); }
    portEXIT_CRITICAL(&buzzerMux);
  }
}

static void uiTask(void *param) {
  (void)param;
  char emo[16];
  while (true) {
    // ── OTA ADD: pausa animação durante OTA ──────────────────────────────
    if (otaManager.isActive()) { vTaskDelay(50 / portTICK_PERIOD_MS); continue; }
    // ────────────────────────────────────────────────────────────────────
    if (g_displaySleeping) { vTaskDelay(20 / portTICK_PERIOD_MS); continue; }

    bool localOverride = false;
    bool localSoundPending = false;
    char localSoundEmo[16];
    bool localSoundShort = false;

    portENTER_CRITICAL(&uiMux);
    localOverride = g_overrideActive;
    if (localOverride) { strncpy(emo, g_overrideEmotion, sizeof(emo) - 1); emo[sizeof(emo)-1] = 0; g_overrideActive = false; }
    else { strncpy(emo, g_autoEmotion, sizeof(emo) - 1); emo[sizeof(emo)-1] = 0; }
    if (g_screenSoundPending) {
      localSoundPending = true;
      strncpy(localSoundEmo, g_screenSoundEmotion, sizeof(localSoundEmo) - 1);
      localSoundEmo[sizeof(localSoundEmo)-1] = 0;
      localSoundShort = g_screenSoundShort;
      g_screenSoundPending = false;
    }
    portEXIT_CRITICAL(&uiMux);

    portENTER_CRITICAL(&uiMux);
    strncpy(g_currentShownEmotion, emo, sizeof(g_currentShownEmotion) - 1);
    g_currentShownEmotion[sizeof(g_currentShownEmotion)-1] = 0;
    portEXIT_CRITICAL(&uiMux);

    if (localSoundPending) tryPlayScreenSoundSynced(emo, localSoundShort);
    runEmotionAnimation(emo, 0, 0, 75);
    vTaskDelay(1);
  }
}

void showEmoteOnDisplay() {
  if (g_displaySleeping) return;
  if (otaManager.isActive()) return; // ← OTA ADD: não altera display durante OTA
  String dominante = getDominantEmotion();
  setAutoEmotion(dominante);
  char shown[16];
  portENTER_CRITICAL(&uiMux);
  strncpy(shown, g_currentShownEmotion, sizeof(shown) - 1);
  shown[sizeof(shown)-1] = 0;
  portEXIT_CRITICAL(&uiMux);
  if (dominante == String(shown)) tryPlayAutoDominantSound(dominante);
}

// ======================================================
//  SERIAL
// ======================================================
void processSerialCommands() {
  static String input = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      input.trim();
      if (input.length() > 0) {
        input.toLowerCase();
        if (input == "feliz" || input == "triste" || input == "entediado" || input == "bravo" || input == "normal" || input == "apaixonado") {
          forcedEmotion = input;
          const bool sv = (forcedEmotion == "feliz" || forcedEmotion == "apaixonado");
          requestOverrideEmotion(forcedEmotion.c_str(), true, sv);
        } else if (input == "auto") {
          forcedEmotion = "";
          Serial.println("[EMOÇÃO FORÇADA] desativada");
        }
        showEmoteOnDisplay();
      }
      input = "";
    } else { input += c; }
  }
}

// ======================================================
//  AMOR / HANDSHAKE (inalterado)
// ======================================================
static inline bool localIsInLove() {
  return (currentPartner.length() > 0 && pctApaixonado > 0);
}

static void rebuildPartnerFromRelacoes() {
  currentPartner = "";
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) {
    if (relacoes[i].nome[0] != 0 && relacoes[i].apaixonado) {
      currentPartner = String(relacoes[i].nome); break;
    }
  }
  if (currentPartner.length() == 0) lastSeenPartnerMs = 0;
}

static void setPartnerLove(const String &nome) {
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) if (relacoes[i].nome[0] != 0) relacoes[i].apaixonado = false;
  int idx = defineRelacaoIndex(nome);
  relacoes[idx].apaixonado = true;
  currentPartner = nome;
  lastSeenPartnerMs = millis();
  pctApaixonado = min(100, pctApaixonado + CFG::LOVE_ON_ACCEPT_GAIN);
  saveHumorToEEPROM();
  if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
  salvaRelacoesEEPROM();
}

static void clearPartnerLove(const String &nome) {
  int idx = buscaRelacao(nome);
  if (idx != -1) relacoes[idx].apaixonado = false;
  if (currentPartner == nome) currentPartner = "";
  lastSeenPartnerMs = 0;
  saveHumorToEEPROM();
  if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
  salvaRelacoesEEPROM();
}

static void applyLoveDecayIfMissingPartner() {
  if (!localIsInLove()) return;
  const unsigned long now = millis();
  if (lastSeenPartnerMs == 0) return;
  unsigned long missing = now - lastSeenPartnerMs;
  if (missing < CFG::LOVE_MISSING_DECAY_START_MS) return;
  static unsigned long lastLoveDecayStepMs = 0;
  if (lastLoveDecayStepMs == 0) lastLoveDecayStepMs = now;
  if (now - lastLoveDecayStepMs >= CFG::LOVE_MISSING_STEP_MS) {
    lastLoveDecayStepMs = now;
    pctApaixonado = max(0, pctApaixonado - CFG::LOVE_MISSING_DECAY_AMOUNT);
    if (pctApaixonado == 0) clearPartnerLove(currentPartner);
    else { saveHumorToEEPROM(); if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str()); }
  }
}

static String lastLoveResp = "IDLE:";

static void setLoveResponse(const String &msg) {
  lastLoveResp = msg;
  if (pLoveResponse) { pLoveResponse->setValue(lastLoveResp.c_str()); pLoveResponse->notify(); }
}

static bool likesThisNameLocally(const String &otherName) {
  int idx = buscaRelacao(otherName);
  if (idx == -1) return false;
  if (!relacoes[idx].relacaoDefinida) return false;
  return relacoes[idx].gosta;
}

class LoveProposalCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String msg = pChar->getValue();
    msg.trim();
    if (!msg.startsWith("PROPOSE:")) { setLoveResponse("REJECT:?"); return; }
    String proposer = msg.substring(String("PROPOSE:").length());
    proposer.trim();
    if (!likesThisNameLocally(proposer)) { setLoveResponse("REJECT:" + proposer); return; }
    if (localIsInLove() && currentPartner != proposer) { setLoveResponse("REJECT:" + proposer); return; }
    int r = random(100);
    if (r < CFG::CHANCE_VIRAR_AMOR_PERCENT) {
      setPartnerLove(proposer); setLoveResponse("LOVE:" + proposer);
      requestOverrideEmotion("apaixonado", true, true);
    } else {
      int idx = defineRelacaoIndex(proposer);
      relacoes[idx].afinidade = min(100, relacoes[idx].afinidade + 10);
      pctFeliz = min(100, pctFeliz + 2);
      normalizaEmocoesAvancada(true, false, false, false, false);
      saveHumorToEEPROM();
      if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
      salvaRelacoesEEPROM();
      setLoveResponse("FRIEND:" + proposer);
      requestOverrideEmotion("feliz", true, true);
    }
  }
};

static bool doHandshakeWith(BLEAdvertisedDevice *device, const String &nomePuro) {
  if (!device) return false;
  if (localIsInLove() && currentPartner != nomePuro) return false;
  int idx = buscaRelacao(nomePuro);
  if (idx < 0) return false;
  unsigned long now = millis();
  if (now - lastHandshakeAttemptMs[idx] < CFG::HANDSHAKE_COOLDOWN_MS) return false;
  lastHandshakeAttemptMs[idx] = now;
  BLEClient *client = BLEDevice::createClient();
  bool ok = false;
  if (!client->connect(device)) { delete client; return false; }
  BLERemoteService *svc = client->getService(BLEUUID(CFG::SERVICE_UUID));
  if (!svc) { client->disconnect(); delete client; return false; }
  BLERemoteCharacteristic *chP = svc->getCharacteristic(BLEUUID(CFG::LOVE_PROPOSAL_UUID));
  BLERemoteCharacteristic *chR = svc->getCharacteristic(BLEUUID(CFG::LOVE_RESPONSE_UUID));
  if (!chP || !chR) { client->disconnect(); delete client; return false; }
  String proposal = String("PROPOSE:") + String(CFG::NAME);
  chP->writeValue((uint8_t*)proposal.c_str(), proposal.length(), true);
  String resp = chR->readValue();
  resp.trim();
  if (resp.startsWith("LOVE:")) { setPartnerLove(nomePuro); requestOverrideEmotion("apaixonado", true, true); ok = true; }
  else if (resp.startsWith("FRIEND:")) {
    int idr = defineRelacaoIndex(nomePuro);
    relacoes[idr].afinidade = min(100, relacoes[idr].afinidade + 10);
    pctFeliz = min(100, pctFeliz + 2);
    normalizaEmocoesAvancada(true, false, false, false, false);
    saveHumorToEEPROM();
    if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
    salvaRelacoesEEPROM();
    requestOverrideEmotion("feliz", true, true);
    ok = true;
  }
  client->disconnect(); delete client;
  return ok;
}

// ======================================================
//  LÓGICA SOCIAL
// ======================================================
void aplicaEfeitoGosta(Relacao &rel, const String &nomePuro) {
  rel.afinidade = min(100, rel.afinidade + 2);
  if (pctTriste > 0) pctTriste--; if (pctEntediado > 0) pctEntediado--; if (pctBravo > 0) pctBravo--;
  pctFeliz = min(100, pctFeliz + 1);
  if (localIsInLove() && currentPartner != nomePuro) { normalizaEmocoesAvancada(true, false, false, false, false); return; }
  if (localIsInLove() && currentPartner == nomePuro) {
    pctApaixonado = min(100, pctApaixonado + 2); pctFeliz = min(100, pctFeliz + 1);
    lastSeenPartnerMs = millis(); normalizaEmocoesAvancada(true, false, false, false, true); return;
  }
  normalizaEmocoesAvancada(true, false, false, false, false);
}

void aplicaEfeitoNaoGosta(Relacao &rel, const String &nomePuro) {
  rel.afinidade = max(-100, rel.afinidade - 2);
  pctEntediado = min(100, pctEntediado + 1); pctBravo = min(100, pctBravo + 1);
  if (localIsInLove() && currentPartner == nomePuro) { pctApaixonado = max(0, pctApaixonado - 5); pctTriste = min(100, pctTriste + 2); }
  normalizaEmocoesAvancada(false, true, true, true, false);
}

void aplicaEfeitoSuspeita(Relacao &rel) { rel.afinidade = constrain(rel.afinidade + (random(3) - 1), -100, 100); }

// ======================================================
//  BLE Server callbacks
// ======================================================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleConnected = true; Serial.println("BLE Conectado");
    delay(20); pServer->getAdvertising()->start();
  }
  void onDisconnect(BLEServer* pServer) override {
    bleConnected = false; Serial.println("BLE Desconectado");
    delay(50); pServer->getAdvertising()->start();
  }
};

// ======================================================
//  MPU eventos
// ======================================================
static void applyCarinhoEvent(const char* originTag) {
  bool af = false, at = false, ab = false, ae = false;
  if (pctFeliz < 100) { pctFeliz += CFG::CARINHO_UP_FELIZ; af = true; }
  if (pctTriste > 0)  { pctTriste -= CFG::CARINHO_DOWN_OUTRAS; at = true; }
  if (pctBravo > 0)   { pctBravo  -= CFG::CARINHO_DOWN_OUTRAS; ab = true; }
  if (pctEntediado > 0) { pctEntediado -= CFG::CARINHO_DOWN_OUTRAS; ae = true; }
  normalizaEmocoesAvancada(af, at, ab, ae, false);
  saveHumorToEEPROM(); if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
  requestOverrideEmotion("feliz", true, true);
}

static void applyNauseaEvent(const char* originTag) {
  pctFeliz = max(0, pctFeliz - CFG::NAUSEA_DOWN_FELIZ);
  pctEntediado = min(100, pctEntediado + CFG::NAUSEA_UP_ENTEDIO);
  pctTriste = min(100, pctTriste + CFG::NAUSEA_UP_TRISTE);
  pctBravo  = min(100, pctBravo  + CFG::NAUSEA_UP_BRAVO);
  normalizaEmocoesAvancada(false, true, true, true, false);
  saveHumorToEEPROM(); if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
  requestOverrideEmotion("enjoado", true, true);
}

static void updateMpuAndDetectEvents() {
  const unsigned long now = millis();
  if (now - lastMpuReadMs < CFG::MPU_READ_EVERY_MS) return;
  lastMpuReadMs = now;
  float ax, ay, az, gx, gy, gz;
  mpu_read_g(ax, ay, az, gx, gy, gz);
  const float amag = sqrtf(ax*ax + ay*ay + az*az);
  const float dyn  = fabsf(amag - 1.0f);
  if (dyn > CFG::SHAKE_G_THRESHOLD) {
    if (now - lastShakeTriggerMs > CFG::SHAKE_EVENT_COOLDOWN_MS) { lastShakeTriggerMs = now; stepCounter = 0; applyNauseaEvent("SHAKE"); }
    return;
  }
  if (dyn > CFG::WALK_STEP_G_THRESHOLD) {
    if (now - lastStepMs > CFG::WALK_STEP_MIN_INTERVAL_MS) {
      lastStepMs = now; stepCounter++;
      if (stepCounter >= CFG::WALK_STEPS_TO_TRIGGER) {
        if (now - lastWalkTriggerMs > CFG::WALK_EVENT_COOLDOWN_MS) { lastWalkTriggerMs = now; stepCounter = 0; applyCarinhoEvent("WALK"); }
      }
    }
  } else { if (stepCounter > 0 && (now - lastStepMs) > 1200) stepCounter = 0; }
}

// ======================================================
//  LDR: máquina de estados do dormir
// ======================================================
static void updateLdrSleepIfNeeded() {
  const unsigned long now = millis();
  if (now - g_lastLdrReadMs < CFG::LDR_READ_EVERY_MS) return;
  g_lastLdrReadMs = now;
  g_ldrMv = readLdrMilliVolts();
  const bool isDark = (g_ldrMv <= CFG::LDR_DARK_MV_THRESHOLD);
  const bool isBrightEnoughToWake = (g_ldrMv >= CFG::LDR_WAKE_MV_THRESHOLD);

  if (g_sleepState == AWAKE) {
    if (isDark) {
      if (g_darkSinceMs == 0) g_darkSinceMs = now;
      if (now - g_darkSinceMs >= CFG::LDR_DARK_DEBOUNCE_MS) {
        g_sleepState = SLEEP_HOLD; g_sleepHoldCyclesLeft = CFG::SLEEP_HOLD_CYCLES;
        g_sleepPrepStartMs = now; g_lastSleepLoopSoundMs = 0;
        if (g_displaySleeping) displaySetSleeping(false);
        requestOverrideEmotion("dormindo", false, true);
        ensureSleepSoundLoop();
      }
    } else { g_darkSinceMs = 0; }
  }
  else if (g_sleepState == SLEEP_HOLD) {
    if (isBrightEnoughToWake) { g_sleepState = AWAKE; g_darkSinceMs = 0; stopAllSoundsNow(); showEmoteOnDisplay(); return; }
    requestOverrideEmotion("dormindo", false, true); ensureSleepSoundLoop();
    if (g_sleepHoldCyclesLeft > 0) g_sleepHoldCyclesLeft--;
    if (g_sleepHoldCyclesLeft <= 0) { stopAllSoundsNow(); displaySetSleeping(true); g_sleepState = SLEEPING; }
  }
  else if (g_sleepState == SLEEPING) {
    if (isBrightEnoughToWake) {
      displaySetSleeping(false); g_sleepState = WAKE_HOLD; g_wakeHoldCyclesLeft = CFG::SLEEP_HOLD_CYCLES;
      g_darkSinceMs = 0; g_lastSleepLoopSoundMs = 0;
      requestOverrideEmotion("dormindo", false, true); ensureSleepSoundLoop();
    } else { stopAllSoundsNow(); }
  }
  else {
    if (isDark) { if (g_darkSinceMs == 0) g_darkSinceMs = now; } else { g_darkSinceMs = 0; }
    requestOverrideEmotion("dormindo", false, true); ensureSleepSoundLoop();
    if (g_wakeHoldCyclesLeft > 0) g_wakeHoldCyclesLeft--;
    if (g_wakeHoldCyclesLeft <= 0) { stopAllSoundsNow(); g_sleepState = AWAKE; showEmoteOnDisplay(); }
  }
}

// ======================================================
//  SETUP
// ======================================================
void buzzerMaxIfSupported() {}

void setup() {
  Serial.begin(115200);

  // ── ESP32-S3: cancela o rollback automático do bootloader ─────────────────
  // O bootloader IDF v5 do S3 tem "app rollback" habilitado por padrão.
  // Se o novo firmware não chamar esta função logo no boot, o bootloader
  // assume que o app falhou e reverte para o firmware anterior na próxima
  // reinicialização — fazendo o OTA parecer que "não funcionou".
  // Esta chamada deve ser a PRIMEIRA coisa no setup(), antes de qualquer delay.
  esp_ota_mark_app_valid_cancel_rollback();
  // ─────────────────────────────────────────────────────────────────────────

  Wire.begin(CFG::I2C_SDA, CFG::I2C_SCL);
  Wire.setClock(400000);

  auto probe = [&](uint8_t a){ Wire.beginTransmission(a); return (Wire.endTransmission() == 0); };
  if (probe(0x68)) MPU_ADDR = 0x68;
  else if (probe(0x69)) MPU_ADDR = 0x69;
  else MPU_ADDR = 0x68;

  mpuOk = mpu_begin_raw();

  // ── EEPROM: inicializa só no primeiro boot (magic byte na posição 9) ───────
  // limpaTodaEEPROM() foi removido — apagava tudo a cada boot, inclusive
  // após OTA, perdendo relações e humor salvos.
  EEPROM.begin(CFG::EEPROM_SIZE);
  const uint8_t EEPROM_MAGIC = 0xDB; // 0xDB = DeskBuddy
  if (EEPROM.read(9) != EEPROM_MAGIC) {
    // Primeiro boot real: inicializa valores padrão
    Serial.println("[EEPROM] Primeiro boot — inicializando valores padrão.");
    for (int i = 0; i < CFG::EEPROM_SIZE; i++) EEPROM.write(i, 0x00);
    EEPROM.write(0, 70); // pctFeliz
    EEPROM.write(1, 10); // pctTriste
    EEPROM.write(2, 10); // pctEntediado
    EEPROM.write(3, 10); // pctBravo
    EEPROM.write(4, 0);  // pctNormal
    EEPROM.write(5, 0);  // pctApaixonado
    EEPROM.write(9, EEPROM_MAGIC); // marca como inicializado
    EEPROM.commit();
  } else {
    Serial.println("[EEPROM] Boot normal — carregando dados salvos.");
  }
  // ─────────────────────────────────────────────────────────────────────────

  if (!display.begin(SSD1306_SWITCHCAPVCC, CFG::OLED_ADDR)) { while (true); }
  display.clearDisplay();
  display.setTextSize(4); display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(CFG::NAME, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((CFG::SCREEN_WIDTH - w) / 2, (CFG::SCREEN_HEIGHT - h) / 2);
  display.println(CFG::NAME); display.display();
  delay(3000); display.clearDisplay();

  pinMode(CFG::BUTTON_PIN, INPUT_PULLUP);
  // ESP32-S3: analogSetAttenuation() configura todos os pinos ADC1 de uma vez.
  // ADC_11db (ADC_ATTEN_DB_12 na IDF v5) → range 0–3.3V
  analogSetAttenuation(ADC_11db);
  pinMode(CFG::BAT_ADC_PIN, INPUT);
  pinMode(CFG::LDR_ADC_PIN, INPUT);

  // ESP32-S3 é dual-core Xtensa LX7:
  // Core 0 = protocolo BLE/WiFi (reservado pelo sistema)
  // Core 1 = aplicação → uiTask deve rodar aqui
  xTaskCreatePinnedToCore(uiTask, "uiTask", 4096, nullptr, 1, &uiTaskHandle, 1);
  buzzer.begin(CFG::BUZZER_PIN);
  loadHumorFromEEPROM();
  rebuildPartnerFromRelacoes();

  BLEDevice::init(String("DeskBuddy: ") + CFG::NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pServer->getAdvertising()->setMinPreferred(0x06);
  pServer->getAdvertising()->setMinPreferred(0x12);

  BLEService *pService = pServer->createService(
    BLEUUID(CFG::SERVICE_UUID),
    40  // ← OTA ADD: aumenta slots de characteristics (de default para 40)
  );

  pCharacteristic = pService->createCharacteristic(CFG::CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  pCharacteristic->setValue(getHumorJSON().c_str());

  pLoveProposal = pService->createCharacteristic(CFG::LOVE_PROPOSAL_UUID, BLECharacteristic::PROPERTY_WRITE);
  pLoveProposal->setCallbacks(new LoveProposalCallbacks());

  pLoveResponse = pService->createCharacteristic(CFG::LOVE_RESPONSE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pLoveResponse->setValue(lastLoveResp.c_str());

  // ── OTA ADD: registra características OTA antes de iniciar o serviço ─────
  otaManager.begin(pService);
  // ─────────────────────────────────────────────────────────────────────────

  pService->start();
  pServer->getAdvertising()->start();
  BLEDevice::getScan()->setActiveScan(true);

  showEmoteOnDisplay();

  Serial.println("[DeskBuddy] Pronto! Firmware v" + String(FW_VERSION));
  Serial.println("[OTA] Aguardando atualização via BLE...");
}

// ======================================================
//  LOOP
// ======================================================
void loop() {
  // ── OTA ADD: processa reboot pendente (seguro fora de callback BLE) ───────
  otaManager.tick();

  // Se OTA está recebendo dados, bloqueia o loop normal
  if (otaManager.isActive()) {
    portENTER_CRITICAL(&buzzerMux);
    buzzer.update();
    portEXIT_CRITICAL(&buzzerMux);
    delay(10);
    return;
  }
  // ─────────────────────────────────────────────────────────────────────────

  processSerialCommands();
  updateLdrSleepIfNeeded();
  updateBatteryIfNeeded();
  if (mpuOk) updateMpuAndDetectEvents();

  portENTER_CRITICAL(&buzzerMux);
  buzzer.update();
  portEXIT_CRITICAL(&buzzerMux);

  applyLoveDecayIfMissingPartner();

  if (!g_displaySleeping && digitalRead(CFG::BUTTON_PIN) == LOW && (millis() - lastButtonTime > CFG::BUTTON_DEBOUNCE_MS)) {
    lastButtonTime = millis();
    bool af = false, at = false, ab = false, ae = false;
    if (pctFeliz < 100) { pctFeliz += CFG::CARINHO_UP_FELIZ; af = true; }
    if (pctTriste > 0)  { pctTriste -= CFG::CARINHO_DOWN_OUTRAS; at = true; }
    if (pctBravo > 0)   { pctBravo  -= CFG::CARINHO_DOWN_OUTRAS; ab = true; }
    if (pctEntediado > 0) { pctEntediado -= CFG::CARINHO_DOWN_OUTRAS; ae = true; }
    normalizaEmocoesAvancada(af, at, ab, ae, false);
    saveHumorToEEPROM();
    if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
    requestOverrideEmotion("feliz", true, true);
  }

  static unsigned long lastDecay = millis();
  if (millis() - lastDecay > CFG::DECAY_INTERVAL_MS && forcedEmotion.length() == 0) {
    lastDecay = millis();
    if (pctApaixonado > 0 && !localIsInLove()) pctApaixonado = max(0, pctApaixonado - CFG::DECAY_AMOR_SEM_PARCEIRO);
    if (pctFeliz > 0) {
      pctFeliz -= CFG::DECAY_FELIZ_SUB;
      if (random(2) == 0) pctTriste += CFG::DECAY_TRISTE_ADD;
      else pctEntediado += CFG::DECAY_ENTEDIADO_ADD;
      normalizaEmocoesAvancada(false, true, false, true, false);
    } else {
      if (random(2) == 0 && pctTriste < 100) pctTriste += CFG::DECAY_TRISTE_ADD;
      else if (pctEntediado < 100) pctEntediado += CFG::DECAY_ENTEDIADO_ADD;
      normalizaEmocoesAvancada(false, true, false, true, false);
    }
    saveHumorToEEPROM();
    if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
  }

  static BLEScan* pBLEScan = BLEDevice::getScan();
  bool ranScan = false;
  BLEScanResults* results = nullptr;
  unsigned long scanEvery = bleConnected ? CFG::SCAN_INTERVAL_CONNECTED_MS : CFG::SCAN_INTERVAL_DISCONNECTED_MS;
  if (millis() - lastScanMs >= scanEvery) {
    lastScanMs = millis(); ranScan = true;
    pBLEScan->setActiveScan(true);
    if (bleConnected) { pBLEScan->setInterval(240); pBLEScan->setWindow(45); }
    else { pBLEScan->setInterval(120); pBLEScan->setWindow(80); }
    results = pBLEScan->start(CFG::SCAN_DURATION_SEC, false);
  }

  bool emInteracao = false;
  if (ranScan && results) {
    for (int i = 0; i < results->getCount(); i++) {
      BLEAdvertisedDevice device = results->getDevice(i);
      String nameStd = device.getName();
      if (nameStd.length() > 0 && nameStd.indexOf("DeskBuddy") != -1) {
        String nomePuro = extraiNomeDeskBuddy(nameStd);
        if (nomePuro.length() > 0 && nomePuro != String(CFG::NAME) && !jaTemNomeNoBuffer(nomePuro)) {
          encontrados[idxEncontrado] = nomePuro;
          idxEncontrado = (idxEncontrado + 1) % CFG::MAX_ENCONTRADOS;
          if (countEncontrados < CFG::MAX_ENCONTRADOS) countEncontrados++;
          salvaBufferEncontradosEEPROM();
        }
        int idx = defineRelacaoIndex(nomePuro);
        Relacao &rel = relacoes[idx];
        unsigned long nowInteract = millis();
        if (nowInteract - lastInteractionTime < CFG::DECAY_INTERVAL_MS) break;
        lastInteractionTime = nowInteract;
        rel.contador++;
        if (localIsInLove() && currentPartner == nomePuro) lastSeenPartnerMs = millis();
        if (!rel.relacaoDefinida && rel.contador >= CFG::INTERACOES_PRA_DEFINIR_RELACAO) {
          rel.gosta = (random(100) < CFG::CHANCE_GOSTAR_PRIMEIRA_PERCENT);
          rel.relacaoDefinida = true; rel.segundaChanceConcedida = false;
        } else if (rel.relacaoDefinida && !rel.gosta && !rel.segundaChanceConcedida && rel.contador >= CFG::INTERACOES_PRA_SEGUNDA_CHANCE) {
          rel.gosta = (random(100) < CFG::CHANCE_GOSTAR_SEGUNDA_PERCENT);
          rel.segundaChanceConcedida = true;
        }
        if (rel.relacaoDefinida) {
          if (rel.gosta) {
            aplicaEfeitoGosta(rel, nomePuro);
            if (!localIsInLove() || currentPartner == nomePuro) {
              if (rel.contador >= CFG::INTERACOES_PRA_APAIXONAR && !rel.apaixonado) {
                (void)doHandshakeWith(&device, nomePuro); rebuildPartnerFromRelacoes();
              }
            }
            if (localIsInLove() && currentPartner == nomePuro) requestOverrideEmotion("apaixonado", true, true);
            else requestOverrideEmotion("feliz", true, true);
          } else {
            aplicaEfeitoNaoGosta(rel, nomePuro); requestOverrideEmotion("bravo", true, false);
          }
        } else {
          aplicaEfeitoSuspeita(rel); requestOverrideEmotion("suspeita", true, false);
        }
        salvaRelacoesEEPROM(); saveHumorToEEPROM();
        if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
        emInteracao = true; break;
      }
    }
  }
  if (ranScan) pBLEScan->clearResults();
  if (!emInteracao) showEmoteOnDisplay();
}