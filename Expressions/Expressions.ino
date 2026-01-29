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

  static constexpr int I2C_SDA = 2;
  static constexpr int I2C_SCL = 3;

  // -------- IO --------
  static constexpr int BUTTON_PIN = 6;
  static constexpr int BUZZER_PIN = 10;

  // -------- Bateria (ADC) --------
  static constexpr int BAT_ADC_PIN = 1; // pino 1 (ADC)

  static constexpr float BAT_R_TOP    = 330000.0f;
  static constexpr float BAT_R_BOTTOM = 100000.0f;

  static constexpr float BAT_FULL_V  = 3.70f; // 100%
  static constexpr float BAT_EMPTY_V = 2.50f; // 0%

  static constexpr int   BAT_LOW_PERCENT = 15;
  static constexpr unsigned long BAT_READ_EVERY_MS = 5000;

  // ======================================================
  //  ✅ GLDR (LDR) - Luminosidade / "Dormir"
  //  Ligação: LDR -> 3V3, resistor -> GND, meio -> GPIO8 (ADC)
  // ======================================================
  static constexpr int LDR_ADC_PIN = 8;                 // ✅ você pediu pino 8
  static constexpr unsigned long LDR_READ_EVERY_MS = 300; // leitura da luz
  // Como seu divisor fica "baixo no escuro", usamos mV baixo = escuro.
  static constexpr int LDR_DARK_MV_THRESHOLD  = 180;    // entra em "dormir" abaixo disso
  static constexpr int LDR_WAKE_MV_THRESHOLD  = 280;    // acorda acima disso (histerese)
  static constexpr unsigned long LDR_DARK_DEBOUNCE_MS = 2500; // precisa ficar escuro por X ms

  // Sequência de "dormir":
  // 1) toca som + animação (placeholder: apaixonado)
  // 2) depois apaga a tela (DISPLAYOFF) e para sons
  static constexpr unsigned long SLEEP_PREP_ANIM_MS = 2500;   // quanto tempo mostra o "dormindo" (apaixonado)
  static constexpr unsigned long SLEEP_PREP_SOUND_MS = 900;   // quanto tempo deixa o som tocar (depois silencia)

  // -------- EEPROM --------
  static constexpr int EEPROM_SIZE = 1024;

  // -------- BLE UUIDs (mantidos) --------
  static constexpr const char* SERVICE_UUID        = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  static constexpr const char* CHARACTERISTIC_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // READ status JSON

  static constexpr const char* LOVE_PROPOSAL_UUID  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // WRITE: "PROPOSE:<meuNome>"
  static constexpr const char* LOVE_RESPONSE_UUID  = "6e400004-b5a3-f393-e0a9-e50e24dcca9e"; // READ/NOTIFY

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

  // -------- Chances --------
  static constexpr int CHANCE_GOSTAR_PRIMEIRA_PERCENT = 70;
  static constexpr int CHANCE_GOSTAR_SEGUNDA_PERCENT  = 50;
  static constexpr int CHANCE_VIRAR_AMOR_PERCENT      = 25;

  // -------- Amor --------
  static constexpr unsigned long LOVE_MISSING_DECAY_START_MS = 120000;
  static constexpr unsigned long LOVE_MISSING_STEP_MS        = 30000;
  static constexpr int LOVE_MISSING_DECAY_AMOUNT             = 3;
  static constexpr int LOVE_ON_ACCEPT_GAIN                   = 15;

  // -------- Efeitos do botão (carinho) --------
  static constexpr int CARINHO_UP_FELIZ     = 3;
  static constexpr int CARINHO_DOWN_OUTRAS = 1;

  // -------- Decaimento --------
  static constexpr int DECAY_FELIZ_SUB          = 5;
  static constexpr int DECAY_TRISTE_ADD         = 1;
  static constexpr int DECAY_ENTEDIADO_ADD      = 1;
  static constexpr int DECAY_AMOR_SEM_PARCEIRO  = 1;

  // -------- Sons --------
  static constexpr unsigned long GLOBAL_SOUND_GAP_MS = 150;
  static constexpr unsigned long DISPLAY_SOUND_MIN_GAP_CYCLES = 1;
  static constexpr unsigned long AUTO_SOUND_MIN_GAP_CYCLES    = 5;

  // -------- Handshake cooldown --------
  static constexpr unsigned long HANDSHAKE_COOLDOWN_MS = 30000;

  // -------- Scan BLE --------
  static constexpr unsigned long SCAN_INTERVAL_DISCONNECTED_MS = 1200;
  static constexpr unsigned long SCAN_INTERVAL_CONNECTED_MS    = 6000;
  static constexpr uint32_t SCAN_DURATION_SEC                  = 1;

  // ======================================================
  //  ✅ MPU9250: DETECÇÃO DE CAMINHADA E AGITAÇÃO
  // ======================================================
  static constexpr unsigned long MPU_READ_EVERY_MS = 40; // ~25Hz
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

// =================== MPU (raw) - ACC/GYRO via registradores ===================
static uint8_t MPU_ADDR = 0x68;

// Leitura I2C
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

// Init básico (acorda + ranges comuns)
static bool mpu_begin_raw(){
  // acorda
  mpu_wr(0x6B, 0x00);
  delay(10);
  if (mpu_rd(0x6B) != 0x00) return false;

  // gyro FS_SEL=0 => ±250 dps (0x1B)
  mpu_wr(0x1B, 0x00);
  // accel AFS_SEL=0 => ±2g (0x1C)
  mpu_wr(0x1C, 0x00);

  // (opcional) DLPF
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
//  MULTITASK (DISPLAY SEM TRAVAR)
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
//  ✅ GLDR (LDR) - Estado de "Dormir"
// ======================================================
enum SleepState : uint8_t { AWAKE = 0, SLEEP_PREP = 1, SLEEPING = 2 };
static volatile bool g_displaySleeping = false;  // usado também pela uiTask
static SleepState g_sleepState = AWAKE;
static unsigned long g_lastLdrReadMs = 0;
static int g_ldrMv = 0;
static unsigned long g_darkSinceMs = 0;
static unsigned long g_sleepPrepStartMs = 0;
static unsigned long g_sleepSoundStopMs = 0;

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

// ======================================================
//  MPU9250: estado
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
//  ✅ GLDR (LDR) helpers
// ======================================================
static int readLdrMilliVolts() {
  return (int)analogReadMilliVolts(CFG::LDR_ADC_PIN);
}

static void displaySetSleeping(bool sleeping) {
  g_displaySleeping = sleeping;
  if (sleeping) {
    // apaga tela de verdade (economiza bem)
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  } else {
    display.ssd1306_command(SSD1306_DISPLAYON);
    // limpa só pra não voltar “lixo”
    display.clearDisplay();
    display.display();
  }
}

static void stopAllSoundsNow() {
  portENTER_CRITICAL(&buzzerMux);
  buzzer.stop(); // BuzzerScheduler tem stop() (se a sua versão não tiver, me fala que eu ajusto)
  portEXIT_CRITICAL(&buzzerMux);
}

// ======================================================
//  BATERIA helpers
// ======================================================
static float readBatteryVoltage() {
  uint32_t mv = analogReadMilliVolts(CFG::BAT_ADC_PIN);
  float v_adc = mv / 1000.0f;
  float v_bat = v_adc * (CFG::BAT_R_TOP + CFG::BAT_R_BOTTOM) / CFG::BAT_R_BOTTOM;
  return v_bat;
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

  g_batteryVoltage = readBatteryVoltage();
  g_batteryPercent = batteryPercentFromVoltage(g_batteryVoltage);

  bool wasLow = g_lowBattery;
  g_lowBattery = (g_batteryPercent < CFG::BAT_LOW_PERCENT);

  Serial.print("[BATERIA] V=");
  Serial.print(g_batteryVoltage, 2);
  Serial.print("V  %=");
  Serial.print(g_batteryPercent);
  Serial.println(g_lowBattery ? "  (LOW -> FOME)" : "");

  if (!wasLow && g_lowBattery) {
    requestOverrideEmotion("fome", true, false);
  }
}

// ======================================================
//  EEPROM
// ======================================================
void limpaTodaEEPROM() {
  EEPROM.begin(CFG::EEPROM_SIZE);
  for (int i = 0; i < CFG::EEPROM_SIZE; i++) EEPROM.write(i, 0x00);
  EEPROM.commit();
}

void salvaRelacoesEEPROM() {
  int addr = 10;
  EEPROM.put(addr, relacoes);
  EEPROM.commit();
}

void carregaRelacoesEEPROM() {
  int addr = 10;
  EEPROM.get(addr, relacoes);
}

void salvaBufferEncontradosEEPROM() {
  int addr = 10 + sizeof(relacoes);
  EEPROM.put(addr, encontrados);
  addr += sizeof(encontrados);
  EEPROM.put(addr, idxEncontrado);
  addr += sizeof(idxEncontrado);
  EEPROM.put(addr, countEncontrados);
  EEPROM.commit();
}

void carregaBufferEncontradosEEPROM() {
  int addr = 10 + sizeof(relacoes);
  EEPROM.get(addr, encontrados);
  addr += sizeof(encontrados);
  EEPROM.get(addr, idxEncontrado);
  addr += sizeof(idxEncontrado);
  EEPROM.get(addr, countEncontrados);
}

void inicializaEEPROMSempre() {
  EEPROM.begin(CFG::EEPROM_SIZE);
  EEPROM.write(0, 70);
  EEPROM.write(1, 10);
  EEPROM.write(2, 10);
  EEPROM.write(3, 10);
  EEPROM.write(4, 0);
  EEPROM.write(5, 0);
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
  EEPROM.write(0, pctFeliz);
  EEPROM.write(1, pctTriste);
  EEPROM.write(2, pctEntediado);
  EEPROM.write(3, pctBravo);
  EEPROM.write(4, pctNormal);
  EEPROM.write(5, pctApaixonado);
  EEPROM.commit();

  salvaRelacoesEEPROM();
  salvaBufferEncontradosEEPROM();
}

// ======================================================
//  UTIL
// ======================================================
int buscaRelacao(const String& nome) {
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) {
    if (String(relacoes[i].nome) == nome) return i;
  }
  return -1;
}

int defineRelacaoIndex(const String& nome) {
  int idx = buscaRelacao(nome);
  if (idx == -1) {
    for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) {
      if (relacoes[i].nome[0] == 0) {
        nome.toCharArray(relacoes[i].nome, CFG::TAM_NOME_MAX);
        relacoes[i].gosta = false;
        relacoes[i].contador = 0;
        relacoes[i].relacaoDefinida = false;
        relacoes[i].segundaChanceConcedida = false;

        relacoes[i].apaixonado = false;
        relacoes[i].afinidade = 0;

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
    {pctApaixonado, "apaixonado"},
    {pctFeliz,      "feliz"},
    {pctTriste,     "triste"},
    {pctEntediado,  "entediado"},
    {pctBravo,      "bravo"}
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
  static const unsigned long REAL_SHOW_MS   = 2000;
  static const unsigned long CYCLE_MS       = FOME_SHOW_MS + REAL_SHOW_MS;

  unsigned long t = millis() % CYCLE_MS;
  if (t < FOME_SHOW_MS) return "fome";
  return realDom;
}

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
  json += "\"bateria_v\":" + String(g_batteryVoltage, 3) + ",";
  json += "\"bateria_low\":" + String(g_lowBattery ? "true" : "false") + ",";
  json += "\"ble_app_conectado\":" + String(bleConnected ? "true" : "false") + ",";
  json += "\"sleeping\":" + String(g_displaySleeping ? "true" : "false") + ",";
  json += "\"ldr_mv\":" + String(g_ldrMv) + ",";
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

  // ✅ Se a tela está “dormindo”, não agenda som (pra ficar silencioso)
  if (requestSound && !g_displaySleeping) {
    g_screenSoundPending = true;
    strncpy(g_screenSoundEmotion, emo, sizeof(g_screenSoundEmotion) - 1);
    g_screenSoundEmotion[sizeof(g_screenSoundEmotion) - 1] = 0;
    g_screenSoundShort = shortVariant;
  }
  portEXIT_CRITICAL(&uiMux);
}

static void runEmotionAnimation(const char *emo, int xx=0, int yy=0, int tt=75) {
  if (strcmp(emo, "apaixonado") == 0) {
    loving(xx, yy, tt);
  } else if (strcmp(emo, "feliz") == 0) {
    happy(xx, yy, tt);
  } else if (strcmp(emo, "triste") == 0) {
    sad(xx, yy, tt);
  } else if (strcmp(emo, "entediado") == 0) {
    bored(xx, yy, tt);
  } else if (strcmp(emo, "bravo") == 0) {
    angry(xx, yy, tt);
  } else if (strcmp(emo, "suspeita") == 0) {
    suspicion(xx, yy, tt);
  } else if (strcmp(emo, "fome") == 0) {
    hunger(xx, yy, tt);
  } else if (strcmp(emo, "enjoado") == 0) {
    nauseous(xx, yy, tt);
  } else {
    normal(xx, yy, tt);
  }
}

// ======================================================
//  BUZZER
// ======================================================
uint8_t soundForEmotion(const String &emocao, bool variantShort=false) {
  if (emocao == "feliz")        return variantShort ? S_HAPPY_SHORT : S_HAPPY;
  if (emocao == "triste")       return S_SAD;
  if (emocao == "entediado")    return S_SLEEPING;
  if (emocao == "bravo")        return S_MODE3;
  if (emocao == "apaixonado")   return S_CUDDLY;
  if (emocao == "fome")       return S_ANGRY;
  if (emocao == "suspeita")     return S_CONNECTION;
  if (emocao == "enjoado")      return S_SURPRISE;

  return S_CONNECTION;
}

static inline bool canStartAnySound(unsigned long now, bool isDisplay) {
  if (buzzer.isPlaying()) return false;
  if (now - lastAnySoundMs < CFG::GLOBAL_SOUND_GAP_MS) return false;

  if (isDisplay) {
    if (now - lastDisplaySoundMs < DISPLAY_SOUND_MIN_GAP_MS) return false;
  } else {
    if (now - lastAutoSoundMs < AUTO_SOUND_MIN_GAP_MS) return false;
  }
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
  if (g_displaySleeping) return; // ✅ dormindo = sem som
  const unsigned long now = millis();
  portENTER_CRITICAL(&buzzerMux);
  bool ok = canStartAnySound(now, true);
  if (ok) startSoundLocked(soundForEmotion(String(emo), shortVariant), true);
  portEXIT_CRITICAL(&buzzerMux);
}

static String lastAutoEmotion = "";

static void tryPlayAutoDominantSound(const String &dominante) {
  if (g_displaySleeping) return; // ✅ dormindo = sem som

  const unsigned long now = millis();
  if (now - lastDisplaySoundMs < DISPLAY_SOUND_MIN_GAP_MS) return;

  const bool emotionChanged = (dominante != lastAutoEmotion);

  if (emotionChanged) {
    portENTER_CRITICAL(&buzzerMux);
    bool ok = canStartAnySound(now, false);
    if (ok) {
      lastAutoEmotion = dominante;
      const bool shortVariant = (dominante == "feliz" || dominante == "apaixonado");
      startSoundLocked(soundForEmotion(dominante, shortVariant), false);
    } else {
      lastAutoEmotion = dominante;
    }
    portEXIT_CRITICAL(&buzzerMux);
  } else {
    portENTER_CRITICAL(&buzzerMux);
    bool ok = canStartAnySound(now, false);
    if (ok) {
      const bool shortVariant = (dominante == "feliz" || dominante == "apaixonado");
      startSoundLocked(soundForEmotion(dominante, shortVariant), false);
    }
    portEXIT_CRITICAL(&buzzerMux);
  }
}

static void uiTask(void *param) {
  (void)param;
  char emo[16];

  while (true) {
    // ✅ Se está dormindo, não renderiza (tela está OFF mesmo)
    if (g_displaySleeping) {
      vTaskDelay(20 / portTICK_PERIOD_MS);
      continue;
    }

    bool localOverride = false;
    bool localSoundPending = false;
    char localSoundEmo[16];
    bool localSoundShort = false;

    portENTER_CRITICAL(&uiMux);
    localOverride = g_overrideActive;
    if (localOverride) {
      strncpy(emo, g_overrideEmotion, sizeof(emo) - 1);
      emo[sizeof(emo) - 1] = 0;
      g_overrideActive = false;
    } else {
      strncpy(emo, g_autoEmotion, sizeof(emo) - 1);
      emo[sizeof(emo) - 1] = 0;
    }

    if (g_screenSoundPending) {
      localSoundPending = true;
      strncpy(localSoundEmo, g_screenSoundEmotion, sizeof(localSoundEmo) - 1);
      localSoundEmo[sizeof(localSoundEmo) - 1] = 0;
      localSoundShort = g_screenSoundShort;
      g_screenSoundPending = false;
    }
    portEXIT_CRITICAL(&uiMux);

    portENTER_CRITICAL(&uiMux);
    strncpy(g_currentShownEmotion, emo, sizeof(g_currentShownEmotion) - 1);
    g_currentShownEmotion[sizeof(g_currentShownEmotion) - 1] = 0;
    portEXIT_CRITICAL(&uiMux);

    if (localSoundPending) {
      tryPlayScreenSoundSynced(emo, localSoundShort);
    }

    runEmotionAnimation(emo, 0, 0, 75);
    vTaskDelay(1);
  }
}

void showEmoteOnDisplay() {
  if (g_displaySleeping) return; // ✅ dormindo: não tenta animar/sons
  String dominante = getDominantEmotion();
  setAutoEmotion(dominante);

  char shown[16];
  portENTER_CRITICAL(&uiMux);
  strncpy(shown, g_currentShownEmotion, sizeof(shown) - 1);
  shown[sizeof(shown) - 1] = 0;
  portEXIT_CRITICAL(&uiMux);

  if (dominante == String(shown)) {
    tryPlayAutoDominantSound(dominante);
  }
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
          Serial.print("[EMOÇÃO FORÇADA] ");
          Serial.println(forcedEmotion);

          const bool shortVariant = (forcedEmotion == "feliz" || forcedEmotion == "apaixonado");
          requestOverrideEmotion(forcedEmotion.c_str(), true, shortVariant);
        } else if (input == "auto") {
          forcedEmotion = "";
          Serial.println("[EMOÇÃO FORÇADA] desativada (modo automático)");
        } else {
          Serial.print("Comando desconhecido: ");
          Serial.println(input);
        }
        showEmoteOnDisplay();
      }
      input = "";
    } else {
      input += c;
    }
  }
}

// ======================================================
//  AMOR RECÍPROCO / HANDSHAKE (mantido)
// ======================================================
static inline bool localIsInLove() {
  return (currentPartner.length() > 0 && pctApaixonado > 0);
}

static void rebuildPartnerFromRelacoes() {
  currentPartner = "";
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) {
    if (relacoes[i].nome[0] != 0 && relacoes[i].apaixonado) {
      currentPartner = String(relacoes[i].nome);
      break;
    }
  }
  if (currentPartner.length() == 0) lastSeenPartnerMs = 0;
}

static void setPartnerLove(const String &nome) {
  for (int i = 0; i < CFG::MAX_ENCONTRADOS; i++) {
    if (relacoes[i].nome[0] != 0) relacoes[i].apaixonado = false;
  }
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
    Serial.print("[AMOR] Sem ver parceiro (");
    Serial.print(currentPartner);
    Serial.print(") há ");
    Serial.print(missing / 1000);
    Serial.print("s -> Amor ");
    Serial.println(pctApaixonado);

    if (pctApaixonado == 0) {
      Serial.println("[AMOR] Amor zerou -> terminar relacionamento.");
      clearPartnerLove(currentPartner);
    } else {
      saveHumorToEEPROM();
      if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
    }
  }
}

static String lastLoveResp = "IDLE:";

static void setLoveResponse(const String &msg) {
  lastLoveResp = msg;
  if (pLoveResponse) {
    pLoveResponse->setValue(lastLoveResp.c_str());
    pLoveResponse->notify();
  }
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

    if (!msg.startsWith("PROPOSE:")) {
      setLoveResponse("REJECT:?");
      return;
    }

    String proposer = msg.substring(String("PROPOSE:").length());
    proposer.trim();

    Serial.print("[HANDSHAKE] Recebido PROPOSE de ");
    Serial.println(proposer);

    if (!likesThisNameLocally(proposer)) {
      Serial.println("[HANDSHAKE] Eu NÃO gosto -> REJECT");
      setLoveResponse("REJECT:" + proposer);
      return;
    }

    if (localIsInLove() && currentPartner != proposer) {
      Serial.print("[HANDSHAKE] Já apaixonado por ");
      Serial.print(currentPartner);
      Serial.println(" -> REJECT");
      setLoveResponse("REJECT:" + proposer);
      return;
    }

    int r = random(100);
    if (r < CFG::CHANCE_VIRAR_AMOR_PERCENT) {
      Serial.println("[HANDSHAKE] Resultado: LOVE");
      setPartnerLove(proposer);
      setLoveResponse("LOVE:" + proposer);
      requestOverrideEmotion("apaixonado", true, true);
    } else {
      Serial.println("[HANDSHAKE] Resultado: FRIEND");
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

  Serial.print("[HANDSHAKE] Tentando com ");
  Serial.println(nomePuro);

  BLEClient *client = BLEDevice::createClient();
  bool ok = false;

  if (!client->connect(device)) {
    Serial.println("[HANDSHAKE] Falhou conectar.");
    delete client;
    return false;
  }

  BLERemoteService *svc = client->getService(BLEUUID(CFG::SERVICE_UUID));
  if (!svc) {
    Serial.println("[HANDSHAKE] Serviço não encontrado.");
    client->disconnect();
    delete client;
    return false;
  }

  BLERemoteCharacteristic *chProposal = svc->getCharacteristic(BLEUUID(CFG::LOVE_PROPOSAL_UUID));
  BLERemoteCharacteristic *chResp     = svc->getCharacteristic(BLEUUID(CFG::LOVE_RESPONSE_UUID));

  if (!chProposal || !chResp) {
    Serial.println("[HANDSHAKE] Características proposal/resp não encontradas.");
    client->disconnect();
    delete client;
    return false;
  }

  String proposal = String("PROPOSE:") + String(CFG::NAME);
  chProposal->writeValue((uint8_t*)proposal.c_str(), proposal.length(), true);

  String resp = chResp->readValue();
  resp.trim();

  Serial.print("[HANDSHAKE] Resposta: ");
  Serial.println(resp);

  if (resp.startsWith("LOVE:")) {
    setPartnerLove(nomePuro);
    requestOverrideEmotion("apaixonado", true, true);
    ok = true;
  } else if (resp.startsWith("FRIEND:")) {
    int idr = defineRelacaoIndex(nomePuro);
    relacoes[idr].afinidade = min(100, relacoes[idr].afinidade + 10);
    pctFeliz = min(100, pctFeliz + 2);
    normalizaEmocoesAvancada(true, false, false, false, false);
    saveHumorToEEPROM();
    if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
    salvaRelacoesEEPROM();

    requestOverrideEmotion("feliz", true, true);
    ok = true;
  } else {
    ok = false;
  }

  client->disconnect();
  delete client;
  return ok;
}

// ======================================================
//  LÓGICA SOCIAL (mantida)
// ======================================================
void aplicaEfeitoGosta(Relacao &rel, const String &nomePuro) {
  rel.afinidade = min(100, rel.afinidade + 2);

  if (pctTriste > 0) pctTriste--;
  if (pctEntediado > 0) pctEntediado--;
  if (pctBravo > 0) pctBravo--;
  pctFeliz = min(100, pctFeliz + 1);

  if (localIsInLove() && currentPartner != nomePuro) {
    normalizaEmocoesAvancada(true, false, false, false, false);
    return;
  }

  if (localIsInLove() && currentPartner == nomePuro) {
    pctApaixonado = min(100, pctApaixonado + 2);
    pctFeliz = min(100, pctFeliz + 1);
    lastSeenPartnerMs = millis();
    normalizaEmocoesAvancada(true, false, false, false, true);
    return;
  }

  normalizaEmocoesAvancada(true, false, false, false, false);
}

void aplicaEfeitoNaoGosta(Relacao &rel, const String &nomePuro) {
  rel.afinidade = max(-100, rel.afinidade - 2);

  pctEntediado = min(100, pctEntediado + 1);
  pctBravo     = min(100, pctBravo + 1);

  if (localIsInLove() && currentPartner == nomePuro) {
    pctApaixonado = max(0, pctApaixonado - 5);
    pctTriste = min(100, pctTriste + 2);
  }

  normalizaEmocoesAvancada(false, true, true, true, false);
}

void aplicaEfeitoSuspeita(Relacao &rel) {
  rel.afinidade = constrain(rel.afinidade + (random(3) - 1), -100, 100);
}

// ======================================================
//  BLE Server callbacks
// ======================================================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleConnected = true;
    Serial.println("BLE Conectado");
    // ✅ mantém o DeskBuddy "achável" mesmo com o app conectado
    delay(20);
    pServer->getAdvertising()->start();
  }
  void onDisconnect(BLEServer* pServer) override {
    bleConnected = false;
    Serial.println("BLE Desconectado");
    delay(50);
    pServer->getAdvertising()->start();
  }
};

// ======================================================
//  ✅ MPU9250: eventos
// ======================================================
static void applyCarinhoEvent(const char* originTag) {
  bool alterouFeliz = false, alterouTriste = false, alterouBravo = false, alterouEnt = false;

  if (pctFeliz < 100) { pctFeliz += CFG::CARINHO_UP_FELIZ; alterouFeliz = true; }
  if (pctTriste > 0) { pctTriste -= CFG::CARINHO_DOWN_OUTRAS; alterouTriste = true; }
  if (pctBravo  > 0) { pctBravo  -= CFG::CARINHO_DOWN_OUTRAS; alterouBravo  = true; }
  if (pctEntediado > 0) { pctEntediado -= CFG::CARINHO_DOWN_OUTRAS; alterouEnt = true; }

  normalizaEmocoesAvancada(alterouFeliz, alterouTriste, alterouBravo, alterouEnt, false);
  saveHumorToEEPROM();
  if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());

  requestOverrideEmotion("feliz", true, true);

  Serial.print("[CARINHO/MPU] ");
  Serial.print(originTag);
  Serial.println(" -> Felicidade +3, Tristeza/Bravo/Tédio -1.");
}

static void applyNauseaEvent(const char* originTag) {
  pctFeliz = max(0, pctFeliz - CFG::NAUSEA_DOWN_FELIZ);
  pctEntediado = min(100, pctEntediado + CFG::NAUSEA_UP_ENTEDIO);
  pctTriste = min(100, pctTriste + CFG::NAUSEA_UP_TRISTE);
  pctBravo  = min(100, pctBravo  + CFG::NAUSEA_UP_BRAVO);

  normalizaEmocoesAvancada(false, true, true, true, false);
  saveHumorToEEPROM();
  if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());

  requestOverrideEmotion("enjoado", true, true);

  Serial.print("[ENJOADO/MPU] ");
  Serial.print(originTag);
  Serial.println(" -> Felicidade -, Entediado/Triste/Bravo +.");
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
    if (now - lastShakeTriggerMs > CFG::SHAKE_EVENT_COOLDOWN_MS) {
      lastShakeTriggerMs = now;
      stepCounter = 0;
      applyNauseaEvent("SHAKE");
    }
    return;
  }

  if (dyn > CFG::WALK_STEP_G_THRESHOLD) {
    if (now - lastStepMs > CFG::WALK_STEP_MIN_INTERVAL_MS) {
      lastStepMs = now;
      stepCounter++;

      if (stepCounter >= CFG::WALK_STEPS_TO_TRIGGER) {
        if (now - lastWalkTriggerMs > CFG::WALK_EVENT_COOLDOWN_MS) {
          lastWalkTriggerMs = now;
          stepCounter = 0;
          applyCarinhoEvent("WALK");
        }
      }
    }
  } else {
    if (stepCounter > 0 && (now - lastStepMs) > 1200) stepCounter = 0;
  }
}

// ======================================================
//  ✅ GLDR (LDR): update / máquina de estados do dormir
// ======================================================
static void updateLdrSleepIfNeeded() {
  const unsigned long now = millis();
  if (now - g_lastLdrReadMs < CFG::LDR_READ_EVERY_MS) return;
  g_lastLdrReadMs = now;

  g_ldrMv = readLdrMilliVolts();

  const bool isDark = (g_ldrMv <= CFG::LDR_DARK_MV_THRESHOLD);
  const bool isBrightEnoughToWake = (g_ldrMv >= CFG::LDR_WAKE_MV_THRESHOLD);

  // Debug leve (comenta se quiser)
  // Serial.print("[LDR] mV="); Serial.print(g_ldrMv); Serial.print(" state="); Serial.println((int)g_sleepState);

  if (g_sleepState == AWAKE) {
    if (isDark) {
      if (g_darkSinceMs == 0) g_darkSinceMs = now;
      if (now - g_darkSinceMs >= CFG::LDR_DARK_DEBOUNCE_MS) {
        // entra no preparo do dormir
        g_sleepState = SLEEP_PREP;
        g_sleepPrepStartMs = now;
        g_sleepSoundStopMs = now + CFG::SLEEP_PREP_SOUND_MS;

        // garante tela ligada para mostrar a “animação de dormir”
        if (g_displaySleeping) displaySetSleeping(false);

        // placeholder: usar apaixonado como “dormindo”
        requestOverrideEmotion("apaixonado", true, true);
        Serial.print("[SLEEP] Escuro detectado (mV=");
        Serial.print(g_ldrMv);
        Serial.println(") -> SLEEP_PREP (mostra + som, depois apaga tela)");
      }
    } else {
      g_darkSinceMs = 0;
    }
  }
  else if (g_sleepState == SLEEP_PREP) {
    // Se clareou durante o preparo, cancela e volta
    if (isBrightEnoughToWake) {
      g_sleepState = AWAKE;
      g_darkSinceMs = 0;
      Serial.println("[SLEEP] Clareou durante SLEEP_PREP -> volta AWAKE.");
      showEmoteOnDisplay();
      return;
    }

    // para o som depois de um tempo (mesmo antes de apagar a tela)
    if (!g_displaySleeping && now >= g_sleepSoundStopMs) {
      stopAllSoundsNow();
    }

    // depois do tempo de animação, apaga tela e entra “SLEEPING”
    if (now - g_sleepPrepStartMs >= CFG::SLEEP_PREP_ANIM_MS) {
      stopAllSoundsNow();
      displaySetSleeping(true);
      g_sleepState = SLEEPING;
      Serial.println("[SLEEP] Tela OFF. (BLE continua, resto continua)");
    }
  }
  else { // SLEEPING
    // Se voltou a luz, acorda
    if (isBrightEnoughToWake) {
      displaySetSleeping(false);
      g_sleepState = AWAKE;
      g_darkSinceMs = 0;
      Serial.println("[SLEEP] Acordou (luz voltou) -> Tela ON.");
      showEmoteOnDisplay();
    } else {
      // garante silêncio total dormindo
      stopAllSoundsNow();
    }
  }
}

// ======================================================
//  SETUP / LOOP
// ======================================================
void buzzerMaxIfSupported() {
  // intencionalmente vazio
}

void setup() {
  Serial.begin(115200);
  Wire.begin(CFG::I2C_SDA, CFG::I2C_SCL);

  Serial.println("I2C scan...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found 0x");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("Scan done.");

  Wire.setClock(400000);

  auto probe = [&](uint8_t a){
    Wire.beginTransmission(a);
    return (Wire.endTransmission() == 0);
  };

  if (probe(0x68)) MPU_ADDR = 0x68;
  else if (probe(0x69)) MPU_ADDR = 0x69;
  else MPU_ADDR = 0x68;

  mpuOk = mpu_begin_raw();

  if (!mpuOk) {
    Serial.println("[MPU] Falhou init raw. Vou continuar SEM MPU.");
  } else {
    Serial.println("[MPU] OK (raw ACC/GYRO em 0x68).");
  }

  // Se você NÃO quer resetar tudo sempre, comenta:
  limpaTodaEEPROM();
  // inicializaEEPROMSempre();

  if (!display.begin(SSD1306_SWITCHCAPVCC, CFG::OLED_ADDR)) {
    Serial.println(F("Erro ao inicializar o display OLED"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(CFG::NAME, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (CFG::SCREEN_WIDTH - w) / 2;
  int16_t y = (CFG::SCREEN_HEIGHT - h) / 2;
  display.setCursor(x, y);
  display.println(CFG::NAME);
  display.display();
  delay(3000);
  display.clearDisplay();

  pinMode(CFG::BUTTON_PIN, INPUT_PULLUP);

  pinMode(CFG::BAT_ADC_PIN, INPUT);
  analogSetPinAttenuation(CFG::BAT_ADC_PIN, ADC_11db);

  // ✅ LDR no pino 8 (ADC)
  pinMode(CFG::LDR_ADC_PIN, INPUT);
  analogSetPinAttenuation(CFG::LDR_ADC_PIN, ADC_11db);

  // Inicia task de UI
  xTaskCreatePinnedToCore(uiTask, "uiTask", 4096, nullptr, 1, &uiTaskHandle, 0);

  buzzer.begin(CFG::BUZZER_PIN);
  buzzerMaxIfSupported();

  loadHumorFromEEPROM();
  rebuildPartnerFromRelacoes();

  if (mpuOk) Serial.println("[MPU] OK (tentando ler dados)");

  BLEDevice::init(String("DeskBuddy: ") + CFG::NAME);

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pServer->getAdvertising()->setMinPreferred(0x06);
  pServer->getAdvertising()->setMinPreferred(0x12);

  BLEService *pService = pServer->createService(CFG::SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CFG::CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );
  pCharacteristic->setValue(getHumorJSON().c_str());

  pLoveProposal = pService->createCharacteristic(
    CFG::LOVE_PROPOSAL_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pLoveProposal->setCallbacks(new LoveProposalCallbacks());

  pLoveResponse = pService->createCharacteristic(
    CFG::LOVE_RESPONSE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pLoveResponse->setValue(lastLoveResp.c_str());

  pService->start();
  pServer->getAdvertising()->start();

  BLEDevice::getScan()->setActiveScan(true);

  showEmoteOnDisplay();

  Serial.println("Digite: FELIZ/TRISTE/ENTEDIADO/BRAVO/NORMAL/APAIXONADO ou AUTO.");
  Serial.println("MPU: caminhando => carinho. Agitar forte => enjoado.");
  Serial.println("LDR (GPIO8): escuro -> 'dormir' (placeholder apaixonado) -> tela OFF (BLE continua).");
}

void loop() {
  processSerialCommands();

  // ✅ LDR / dormir (primeiro, pra evitar render/som quando já está apagando)
  updateLdrSleepIfNeeded();

  updateBatteryIfNeeded();

  // ✅ MPU events (walking/shake)
  if (mpuOk) updateMpuAndDetectEvents();

  portENTER_CRITICAL(&buzzerMux);
  buzzer.update();
  portEXIT_CRITICAL(&buzzerMux);

  applyLoveDecayIfMissingPartner();

  // Botão de carinho (mantido)
  if (!g_displaySleeping && digitalRead(CFG::BUTTON_PIN) == LOW && (millis() - lastButtonTime > CFG::BUTTON_DEBOUNCE_MS)) {
    lastButtonTime = millis();

    bool alterouFeliz = false, alterouTriste = false, alterouBravo = false, alterouEnt = false;

    if (pctFeliz < 100) { pctFeliz += CFG::CARINHO_UP_FELIZ; alterouFeliz = true; }
    if (pctTriste > 0) { pctTriste -= CFG::CARINHO_DOWN_OUTRAS; alterouTriste = true; }
    if (pctBravo  > 0) { pctBravo  -= CFG::CARINHO_DOWN_OUTRAS; alterouBravo  = true; }
    if (pctEntediado > 0) { pctEntediado -= CFG::CARINHO_DOWN_OUTRAS; alterouEnt = true; }

    normalizaEmocoesAvancada(alterouFeliz, alterouTriste, alterouBravo, alterouEnt, false);
    saveHumorToEEPROM();
    if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());

    requestOverrideEmotion("feliz", true, true);
    Serial.println("[CARINHO] Felicidade +3, Tristeza/Bravo/Tédio -1.");
  }

  // Decaimento por inatividade (se não estiver forçando emoção)
  static unsigned long lastDecay = millis();
  if (millis() - lastDecay > CFG::DECAY_INTERVAL_MS && forcedEmotion.length() == 0) {
    lastDecay = millis();

    if (pctApaixonado > 0 && !localIsInLove()) {
      pctApaixonado = max(0, pctApaixonado - CFG::DECAY_AMOR_SEM_PARCEIRO);
    }

    if (pctFeliz > 0) {
      pctFeliz -= CFG::DECAY_FELIZ_SUB;
      if (random(2) == 0) pctTriste += CFG::DECAY_TRISTE_ADD;
      else pctEntediado += CFG::DECAY_ENTEDIADO_ADD;
      normalizaEmocoesAvancada(false, true, false, true, false);
      Serial.println("[DECAIMENTO] Felicidade -5, Tristeza ou Tédio +1.");
    } else {
      if (random(2) == 0 && pctTriste < 100) pctTriste += CFG::DECAY_TRISTE_ADD;
      else if (pctEntediado < 100) pctEntediado += CFG::DECAY_ENTEDIADO_ADD;
      normalizaEmocoesAvancada(false, true, false, true, false);
      Serial.println("[DECAIMENTO] Tristeza ou Tédio +1.");
    }

    saveHumorToEEPROM();
    if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());
  }

  // Scan BLE (agendado)
  static BLEScan* pBLEScan = BLEDevice::getScan();

  bool ranScan = false;
  BLEScanResults* results = nullptr;

  unsigned long scanEvery = bleConnected ? CFG::SCAN_INTERVAL_CONNECTED_MS : CFG::SCAN_INTERVAL_DISCONNECTED_MS;
  if (millis() - lastScanMs >= scanEvery) {
    lastScanMs = millis();
    ranScan = true;

    pBLEScan->setActiveScan(true); // ✅ sempre ativo para pegar o nome no scan-response (necessário p/ interação Buddy↔Buddy mesmo com app conectado)
    // ✅ reduz uso de rádio quando já está conectado ao app
    if (bleConnected) {
      pBLEScan->setInterval(240);
      pBLEScan->setWindow(45);
    } else {
      pBLEScan->setInterval(120);
      pBLEScan->setWindow(80);
    }
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
        if (nowInteract - lastInteractionTime < CFG::DECAY_INTERVAL_MS) {
          Serial.println("[INTERACAO] Cooldown ativo, ignorando interação.");
          break;
        }
        lastInteractionTime = nowInteract;

        rel.contador++;

        Serial.print("[BUDDY] Encontrado: ");
        Serial.print(nomePuro);
        Serial.print(" | Interações: ");
        Serial.println(rel.contador);

        if (localIsInLove() && currentPartner == nomePuro) {
          lastSeenPartnerMs = millis();
        }

        if (!rel.relacaoDefinida && rel.contador >= CFG::INTERACOES_PRA_DEFINIR_RELACAO) {
          int sorte = random(100);
          rel.gosta = (sorte < CFG::CHANCE_GOSTAR_PRIMEIRA_PERCENT);
          rel.relacaoDefinida = true;
          rel.segundaChanceConcedida = false;

          if (rel.gosta) Serial.println("[RELACAO] Após 2 interações: GOSTA (70%)");
          else Serial.println("[RELACAO] Após 2 interações: NÃO GOSTA (30%) - Segunda chance após 6 interações.");
        }
        else if (rel.relacaoDefinida && !rel.gosta && !rel.segundaChanceConcedida && rel.contador >= CFG::INTERACOES_PRA_SEGUNDA_CHANCE) {
          int sorte2 = random(100);
          rel.gosta = (sorte2 < CFG::CHANCE_GOSTAR_SEGUNDA_PERCENT);
          rel.segundaChanceConcedida = true;

          if (rel.gosta) Serial.println("[RELACAO] Segunda chance: AGORA GOSTA (50%)");
          else Serial.println("[RELACAO] Segunda chance: CONTINUA NÃO GOSTANDO (50%)");
        }

        if (rel.relacaoDefinida) {
          if (rel.gosta) {
            aplicaEfeitoGosta(rel, nomePuro);

            if (!localIsInLove() || currentPartner == nomePuro) {
              if (rel.contador >= CFG::INTERACOES_PRA_APAIXONAR && !rel.apaixonado) {
                (void)doHandshakeWith(&device, nomePuro);
                rebuildPartnerFromRelacoes();
              }
            }

            if (localIsInLove() && currentPartner == nomePuro) {
              requestOverrideEmotion("apaixonado", true, true);
            } else {
              requestOverrideEmotion("feliz", true, true);
            }

            Serial.print("[EMOCAO] Gosta de ");
            Serial.print(nomePuro);
            Serial.print(". Feliz=");
            Serial.print(pctFeliz);
            Serial.print(" Amor=");
            Serial.print(pctApaixonado);
            Serial.print(" Afinidade=");
            Serial.println(rel.afinidade);

          } else {
            aplicaEfeitoNaoGosta(rel, nomePuro);

            requestOverrideEmotion("bravo", true, false);

            Serial.print("[EMOCAO] NÃO gosta de ");
            Serial.print(nomePuro);
            Serial.print(". Tédio=");
            Serial.print(pctEntediado);
            Serial.print(" Raiva=");
            Serial.print(pctBravo);
            Serial.print(" Amor=");
            Serial.print(pctApaixonado);
            Serial.print(" Afinidade=");
            Serial.println(rel.afinidade);
          }
        } else {
          aplicaEfeitoSuspeita(rel);
          requestOverrideEmotion("suspeita", true, false);
          Serial.println("[EMOCAO] Relação indefinida: SUSPEITA.");
        }

        salvaRelacoesEEPROM();
        saveHumorToEEPROM();
        if (pCharacteristic) pCharacteristic->setValue(getHumorJSON().c_str());

        emInteracao = true;
        break;
      }
    }
  }

  if (ranScan) pBLEScan->clearResults(); // limpa depois de processar (senão zera results antes)

  if (!emInteracao) {
    showEmoteOnDisplay();
  }
}