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

#include "BuzzerScheduler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ======================================================
//  DISPLAY / PINS / BLE UUIDs
// ======================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define NAME "Bliko"
#define SENHA "oi33"
#define BUTTON_PIN 6
#define EEPROM_SIZE 1024

// Serviço principal (mantido)
#define SERVICE_UUID            "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID     "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // READ status JSON (mantido)

// >>> NOVO: “handshake” amor/amizade (para validar reciprocidade)
#define LOVE_PROPOSAL_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // WRITE: "PROPOSE:<meuNome>"
#define LOVE_RESPONSE_UUID      "6e400004-b5a3-f393-e0a9-e50e24dcca9e" // READ/NOTIFY: "LOVE:<nome>" | "FRIEND:<nome>" | "REJECT:<nome>"

#define MAX_ENCONTRADOS 10
#ifndef TAM_NOME
#define TAM_NOME 16
#endif

#define BUZZER_PIN 10

// ======================================================
//  MULTITASK (DISPLAY SEM TRAVAR)
// ======================================================

// Protege acesso concorrente ao buzzer e às variáveis de UI
portMUX_TYPE buzzerMux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE uiMux = portMUX_INITIALIZER_UNLOCKED;

// Emoção "automática" (dominante) e override "one-shot" (eventos)
static char g_autoEmotion[16] = "normal";
static bool g_overrideActive = false;
static char g_overrideEmotion[16] = "";
// >>> qual emoção está sendo desenhada AGORA na tela (para bloquear som fora da tela)
static char g_currentShownEmotion[16] = "normal";

// >>> Som sempre inicia junto com a emoção (sincronizado na uiTask)
static bool g_screenSoundPending = false;
static char g_screenSoundEmotion[16] = "";
static bool g_screenSoundShort = false;

static TaskHandle_t uiTaskHandle = nullptr;

// ======================================================
//  AJUSTES IMPORTANTES (TEMPOS / REGRAS)
// ======================================================
const unsigned long ANIM_INTERVAL = 7000;      // 7s (mantido, se você usar em outros lugares)
const unsigned long DECAY_INTERVAL_MS = 3000;  // ciclo base (você usa em muita coisa)

// Sons: agora TODOS obedecem essas janelas.
// - DISPLAY_SOUND_MIN_GAP_MS: mínimo entre sons disparados por eventos de tela (interação/botão)
// - AUTO_SOUND_MIN_GAP_MS: mínimo entre sons automáticos (dominante)
static const unsigned long DISPLAY_SOUND_MIN_GAP_MS = (unsigned long)DECAY_INTERVAL_MS * 1;  // 1 ciclo
static const unsigned long AUTO_SOUND_MIN_GAP_MS    = (unsigned long)DECAY_INTERVAL_MS * 5;  // 5 ciclos

// Amor: regras novas
const int INTERACOES_PRA_DEFINIR_RELACAO = 2;
const int INTERACOES_PRA_SEGUNDA_CHANCE  = 6;
const int INTERACOES_PRA_APAIXONAR       = 10;

// quando ambos se gostam, rola um “handshake”: pode virar AMIZADE ou AMOR
const int CHANCE_VIRAR_AMOR_PERCENT      = 25; // dos que chegam no handshake (mutual like)

// Exclusividade + término
static const unsigned long LOVE_MISSING_DECAY_START_MS = 120000; // 2min sem ver parceiro começa a cair
static const unsigned long LOVE_MISSING_STEP_MS        = 30000;  // a cada 30s sem ver, cai mais
static const int LOVE_MISSING_DECAY_AMOUNT             = 3;      // cai 3 pontos por step
static const int LOVE_ON_ACCEPT_GAIN                   = 15;     // ao aceitar amor, sobe 15

const int CARINHO_UP_FELIZ = 3;
const int CARINHO_DOWN_OUTRAS = 1;

// ======================================================
//  OBJETOS
// ======================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BLECharacteristic *pCharacteristic = nullptr;     // status JSON (READ)
BLECharacteristic *pLoveProposal   = nullptr;     // proposal (WRITE)
BLECharacteristic *pLoveResponse   = nullptr;     // response (READ/NOTIFY)

BuzzerScheduler buzzer;

// BLE connection state
volatile bool bleConnected = false;

// BLE scan scheduler
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

String encontrados[MAX_ENCONTRADOS];
int idxEncontrado = 0;
int countEncontrados = 0;

Relacao relacoes[MAX_ENCONTRADOS];

// Controle de sons / timers gerais
static unsigned long lastAnySoundMs    = 0; // trava global anti “ciclar”
static unsigned long lastDisplaySoundMs = 0;
static unsigned long lastAutoSoundMs    = 0;

// Amor exclusivo (local)
static String currentPartner = "";           // nome do parceiro se apaixonado
static unsigned long lastSeenPartnerMs = 0;  // última vez que viu o parceiro

// Para evitar conectar/handshake toda hora
static unsigned long lastHandshakeAttemptMs[MAX_ENCONTRADOS] = {0};
static const unsigned long HANDSHAKE_COOLDOWN_MS = 30000; // 30s

// ======================================================
//  EEPROM
// ======================================================
void limpaTodaEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0x00);
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
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, 70); // feliz
  EEPROM.write(1, 10); // triste
  EEPROM.write(2, 10); // entediado
  EEPROM.write(3, 10); // bravo
  EEPROM.write(4, 0);  // normal
  EEPROM.write(5, 0);  // apaixonado
  EEPROM.commit();
}

void loadHumorFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
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
  for (int i = 0; i < MAX_ENCONTRADOS; i++) {
    if (String(relacoes[i].nome) == nome) return i;
  }
  return -1;
}

int defineRelacaoIndex(const String& nome) {
  int idx = buscaRelacao(nome);
  if (idx == -1) {
    for (int i = 0; i < MAX_ENCONTRADOS; i++) {
      if (relacoes[i].nome[0] == 0) {
        nome.toCharArray(relacoes[i].nome, TAM_NOME);
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
  for (int i = 0; i < MAX_ENCONTRADOS; i++) if (encontrados[i] == nome) return true;
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

void normalizaEmocoesAvancada(bool acaoFoiFeliz = false, bool acaoFoiTriste = false, bool acaoFoiBravo = false, bool acaoFoiEntediado = false, bool acaoFoiApaixonado = false) {
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

String getDominantEmotion() {
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

String getHumorJSON() {
  String json = "{";
  json += "\"feliz\":"      + String(pctFeliz)      + ",";
  json += "\"triste\":"     + String(pctTriste)     + ",";
  json += "\"entediado\":"  + String(pctEntediado)  + ",";
  json += "\"bravo\":"      + String(pctBravo)      + ",";
  json += "\"normal\":"     + String(pctNormal)     + ",";
  json += "\"apaixonado\":" + String(pctApaixonado) + ",";
  json += "\"dominante\":\"" + getDominantEmotion() + "\",";
  json += "\"nome\":\"" + String(NAME) + "\",";
  json += "\"senha\":\"" + String(SENHA) + "\",";
  json += "\"parceiro\":\"" + currentPartner + "\",";
  json += "\"encontrados\":[";
  bool first = true;

  for (int i = 0; i < MAX_ENCONTRADOS; i++) {
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

  // Som sincronizado com a emoção (toca no início da animação)
  if (requestSound) {
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
  } else {
    normal(xx, yy, tt);
  }
}

// ======================================================
//  BUZZER (anti “ciclar” + obedecer variáveis + sincronizado)
// ======================================================
uint8_t soundForEmotion(const String &emocao, bool variantShort=false) {
  if (emocao == "feliz")        return variantShort ? S_HAPPY_SHORT : S_HAPPY;
  if (emocao == "triste")       return S_SAD;
  if (emocao == "entediado")    return S_SLEEPING;
  if (emocao == "bravo")        return S_MODE3;
  if (emocao == "apaixonado")   return S_CUDDLY;
  if (emocao == "suspeita")     return S_CONNECTION;
  return S_CONNECTION;
}

// trava global para impedir “sons um atrás do outro”
static inline bool canStartAnySound(unsigned long now, bool isDisplay) {
  // se tá tocando, não inicia outro
  if (buzzer.isPlaying()) return false;

  // trava global: impede encavalamento
  if (now - lastAnySoundMs < 150) return false;

  // janelas por categoria
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

// Chamado SOMENTE pela uiTask (para garantir “som inicia com emoção”)
static void tryPlayScreenSoundSynced(const char *emo, bool shortVariant) {
  const unsigned long now = millis();
  portENTER_CRITICAL(&buzzerMux);
  bool ok = canStartAnySound(now, true);
  if (ok) {
    startSoundLocked(soundForEmotion(String(emo), shortVariant), true);
  }
  portEXIT_CRITICAL(&buzzerMux);
}

// Som automático (dominante) — também respeita intervalos
static String lastAutoEmotion = "";

static void tryPlayAutoDominantSound(const String &dominante) {
  const unsigned long now = millis();

  // não repete automaticamente se acabou de tocar um som de display “perto”
  if (now - lastDisplaySoundMs < DISPLAY_SOUND_MIN_GAP_MS) return;

  const bool emotionChanged = (dominante != lastAutoEmotion);

  // se mudou, tenta tocar imediatamente, mas obedecendo min gap
  if (emotionChanged) {
    portENTER_CRITICAL(&buzzerMux);
    bool ok = canStartAnySound(now, false);
    if (ok) {
      lastAutoEmotion = dominante;
      const bool shortVariant = (dominante == "feliz" || dominante == "apaixonado");
      startSoundLocked(soundForEmotion(dominante, shortVariant), false);
    } else {
      // marca a emoção mesmo assim (pra não “spammar”), e tenta depois pelo tempo
      lastAutoEmotion = dominante;
    }
    portEXIT_CRITICAL(&buzzerMux);
  } else {
    // repetição: só depois de AUTO_SOUND_MIN_GAP_MS
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
    bool localOverride = false;
    bool localSoundPending = false;
    char localSoundEmo[16];
    bool localSoundShort = false;

    portENTER_CRITICAL(&uiMux);
    localOverride = g_overrideActive;
    if (localOverride) {
      strncpy(emo, g_overrideEmotion, sizeof(emo) - 1);
      emo[sizeof(emo) - 1] = 0;
      g_overrideActive = false; // consome override (1 ciclo)
    } else {
      strncpy(emo, g_autoEmotion, sizeof(emo) - 1);
      emo[sizeof(emo) - 1] = 0;
    }

    // pega pedido de som sincronizado
    if (g_screenSoundPending) {
      localSoundPending = true;
      strncpy(localSoundEmo, g_screenSoundEmotion, sizeof(localSoundEmo) - 1);
      localSoundEmo[sizeof(localSoundEmo) - 1] = 0;
      localSoundShort = g_screenSoundShort;
      g_screenSoundPending = false; // consome
    }
    portEXIT_CRITICAL(&uiMux);

    // registra qual emoção está sendo desenhada AGORA na tela
portENTER_CRITICAL(&uiMux);
strncpy(g_currentShownEmotion, emo, sizeof(g_currentShownEmotion) - 1);
g_currentShownEmotion[sizeof(g_currentShownEmotion) - 1] = 0;
portEXIT_CRITICAL(&uiMux);

    // >>> GARANTIA: som inicia junto com a emoção
    if (localSoundPending) {
      // toca só se o som for da mesma emoção que vai iniciar agora
      if (strcmp(localSoundEmo, emo) == 0) {
        tryPlayScreenSoundSynced(emo, localSoundShort);
      } else {
        // se não bater (caso raro), tenta tocar pelo “emo atual”
        tryPlayScreenSoundSynced(emo, localSoundShort);
      }
    }

    runEmotionAnimation(emo, 0, 0, 75);

    vTaskDelay(1);
  }
}

// ======================================================
//  DISPLAY “AUTO” (som automático + animação automática)
// ======================================================
void showEmoteOnDisplay() {
  String dominante = getDominantEmotion();

  // sempre atualiza a emoção alvo do display (animação)
  setAutoEmotion(dominante);

  // >>> Só toca som automático se a emoção dominante for a mesma que está sendo mostrada AGORA
  char shown[16];
  portENTER_CRITICAL(&uiMux);
  strncpy(shown, g_currentShownEmotion, sizeof(shown) - 1);
  shown[sizeof(shown) - 1] = 0;
  portEXIT_CRITICAL(&uiMux);

// Se teve override recente (eventos), não dispara som automático aqui
// (porque a tela está priorizando o que foi pedido pelo evento)
// se a tela está mostrando outra emoção (ex: bravo), não toca som do dominante
if (dominante == String(shown)) {
  tryPlayAutoDominantSound(dominante);
}
}

// ======================================================
//  SERIAL (forçar emoção)
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

          // força override com som sincronizado
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
//  AMOR RECÍPROCO: “handshake” via BLE write/read
// ======================================================

static inline bool localIsInLove() {
  // se currentPartner preenchido e pctApaixonado > 0, consideramos “em amor”
  return (currentPartner.length() > 0 && pctApaixonado > 0);
}

static void rebuildPartnerFromRelacoes() {
  currentPartner = "";
  for (int i = 0; i < MAX_ENCONTRADOS; i++) {
    if (relacoes[i].nome[0] != 0 && relacoes[i].apaixonado) {
      currentPartner = String(relacoes[i].nome);
      break;
    }
  }
  if (currentPartner.length() == 0) {
    lastSeenPartnerMs = 0;
  }
}

static void setPartnerLove(const String &nome) {
  // desmarca qualquer outro (garantia)
  for (int i = 0; i < MAX_ENCONTRADOS; i++) {
    if (relacoes[i].nome[0] != 0) relacoes[i].apaixonado = false;
  }
  int idx = defineRelacaoIndex(nome);
  relacoes[idx].apaixonado = true;

  currentPartner = nome;
  lastSeenPartnerMs = millis();
  pctApaixonado = min(100, pctApaixonado + LOVE_ON_ACCEPT_GAIN);

  saveHumorToEEPROM();
  pCharacteristic->setValue(getHumorJSON().c_str());
  salvaRelacoesEEPROM();
}

static void clearPartnerLove(const String &nome) {
  int idx = buscaRelacao(nome);
  if (idx != -1) relacoes[idx].apaixonado = false;

  if (currentPartner == nome) currentPartner = "";
  lastSeenPartnerMs = 0;

  saveHumorToEEPROM();
  pCharacteristic->setValue(getHumorJSON().c_str());
  salvaRelacoesEEPROM();
}

static void applyLoveDecayIfMissingPartner() {
  if (!localIsInLove()) return;

  const unsigned long now = millis();
  if (lastSeenPartnerMs == 0) return;

  unsigned long missing = now - lastSeenPartnerMs;
  if (missing < LOVE_MISSING_DECAY_START_MS) return;

  static unsigned long lastLoveDecayStepMs = 0;
  if (lastLoveDecayStepMs == 0) lastLoveDecayStepMs = now;

  if (now - lastLoveDecayStepMs >= LOVE_MISSING_STEP_MS) {
    lastLoveDecayStepMs = now;
    pctApaixonado = max(0, pctApaixonado - LOVE_MISSING_DECAY_AMOUNT);
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
      pCharacteristic->setValue(getHumorJSON().c_str());
    }
  }
}

// ========== BLE handshake server side ==========
static String lastLoveResp = "IDLE:";

static void setLoveResponse(const String &msg) {
  lastLoveResp = msg;
  if (pLoveResponse) {
    pLoveResponse->setValue(lastLoveResp.c_str());
    // se alguém estiver conectado, tenta notificar
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
    String msg = pChar->getValue();  // <-- ERA std::string, aqui é String
    msg.trim();

    if (!msg.startsWith("PROPOSE:")) {
      setLoveResponse("REJECT:?");
      return;
    }

    String proposer = msg.substring(String("PROPOSE:").length());
    proposer.trim();

    Serial.print("[HANDSHAKE] Recebido PROPOSE de ");
    Serial.println(proposer);

    // Regras:
    // 1) Precisa ser recíproco: eu preciso gostar dele
    if (!likesThisNameLocally(proposer)) {
      Serial.println("[HANDSHAKE] Eu NÃO gosto -> REJECT");
      setLoveResponse("REJECT:" + proposer);
      return;
    }

    // 2) Exclusivo: se eu já tô apaixonado por outro, rejeita
    if (localIsInLove() && currentPartner != proposer) {
      Serial.print("[HANDSHAKE] Já apaixonado por ");
      Serial.print(currentPartner);
      Serial.println(" -> REJECT");
      setLoveResponse("REJECT:" + proposer);
      return;
    }

    // 3) Decide: AMOR ou AMIZADE (random)
    int r = random(100);
    if (r < CHANCE_VIRAR_AMOR_PERCENT) {
      Serial.println("[HANDSHAKE] Resultado: LOVE");
      setPartnerLove(proposer);
      setLoveResponse("LOVE:" + proposer);

      // emoção + som sincronizados
      requestOverrideEmotion("apaixonado", true, true);
    } else {
      Serial.println("[HANDSHAKE] Resultado: FRIEND");
      // “bons amigos”: aumenta afinidade, mas não apaixona
      int idx = defineRelacaoIndex(proposer);
      relacoes[idx].afinidade = min(100, relacoes[idx].afinidade + 10);
      pctFeliz = min(100, pctFeliz + 2);
      normalizaEmocoesAvancada(true, false, false, false, false);
      saveHumorToEEPROM();
      pCharacteristic->setValue(getHumorJSON().c_str());
      salvaRelacoesEEPROM();

      setLoveResponse("FRIEND:" + proposer);
      requestOverrideEmotion("feliz", true, true);
    }
  }
};

// ========== BLE handshake client side ==========
static bool doHandshakeWith(BLEAdvertisedDevice *device, const String &nomePuro) {
  if (!device) return false;

  if (localIsInLove() && currentPartner != nomePuro) return false;

  int idx = buscaRelacao(nomePuro);
  if (idx < 0) return false;

  unsigned long now = millis();
  if (now - lastHandshakeAttemptMs[idx] < HANDSHAKE_COOLDOWN_MS) return false;
  lastHandshakeAttemptMs[idx] = now;

  Serial.print("[HANDSHAKE] Tentando com ");
  Serial.println(nomePuro);

  BLEClient *client = BLEDevice::createClient();
  bool ok = false;

  // connect precisa de ponteiro
  if (!client->connect(device)) {
    Serial.println("[HANDSHAKE] Falhou conectar.");
    delete client;  // <-- deleteClient não existe
    return false;
  }

  BLERemoteService *svc = client->getService(BLEUUID(SERVICE_UUID));
  if (!svc) {
    Serial.println("[HANDSHAKE] Serviço não encontrado.");
    client->disconnect();
    delete client;
    return false;
  }

  BLERemoteCharacteristic *chProposal = svc->getCharacteristic(BLEUUID(LOVE_PROPOSAL_UUID));
  BLERemoteCharacteristic *chResp     = svc->getCharacteristic(BLEUUID(LOVE_RESPONSE_UUID));

  if (!chProposal || !chResp) {
    Serial.println("[HANDSHAKE] Características proposal/resp não encontradas.");
    client->disconnect();
    delete client;
    return false;
  }

  // envia proposta
  String proposal = String("PROPOSE:") + String(NAME);
  chProposal->writeValue((uint8_t*)proposal.c_str(), proposal.length(), true);

  // readValue aqui retorna String no seu core
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
    pCharacteristic->setValue(getHumorJSON().c_str());
    salvaRelacoesEEPROM();

    requestOverrideEmotion("feliz", true, true);
    ok = true;
  } else {
    ok = false; // REJECT
  }

  client->disconnect();
  delete client; // <-- deleteClient não existe
  return ok;
}

// ======================================================
//  LÓGICA SOCIAL (SEM “AMOR NÃO-RECÍPROCO”)
// ======================================================
void aplicaEfeitoGosta(Relacao &rel, const String &nomePuro) {
  rel.afinidade = min(100, rel.afinidade + 2);

  if (pctTriste > 0) pctTriste--;
  if (pctEntediado > 0) pctEntediado--;
  if (pctBravo > 0) pctBravo--;
  pctFeliz = min(100, pctFeliz + 1);

  // Se eu já estou apaixonado por alguém, não “acumula” amor por outro
  if (localIsInLove() && currentPartner != nomePuro) {
    normalizaEmocoesAvancada(true, false, false, false, false);
    return;
  }

  // Se for o parceiro atual, reforça
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

  // Se eu estava apaixonado por ele e agora “não gosta” (mudança improvável),
  // reduz amor mais rápido
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
  }
  void onDisconnect(BLEServer* pServer) override {
    bleConnected = false;
    Serial.println("BLE Desconectado");
    delay(50);
    pServer->getAdvertising()->start();
  }
};

// ======================================================
//  SETUP / LOOP
// ======================================================
void buzzerMaxIfSupported() {
  // intencionalmente vazio
}

void setup() {
  Serial.begin(115200);
  Wire.begin(2, 3);

  // Se você NÃO quer resetar tudo sempre, comenta:
  limpaTodaEEPROM();
  // inicializaEEPROMSempre();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Erro ao inicializar o display OLED"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(NAME, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - w) / 2;
  int16_t y = (SCREEN_HEIGHT - h) / 2;
  display.setCursor(x, y);
  display.println(NAME);
  display.display();
  delay(3000);
  display.clearDisplay();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Inicia task de UI (display), para que o BLE scan não congele a animação.
  xTaskCreatePinnedToCore(uiTask, "uiTask", 4096, nullptr, 1, &uiTaskHandle, 0);

  buzzer.begin(BUZZER_PIN);
  buzzerMaxIfSupported();

  loadHumorFromEEPROM();
  rebuildPartnerFromRelacoes();

  BLEDevice::init(String("DeskBuddy: ") + NAME);

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pServer->getAdvertising()->setMinPreferred(0x06);
  pServer->getAdvertising()->setMinPreferred(0x12);

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Status JSON
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );
  pCharacteristic->setValue(getHumorJSON().c_str());

  // Handshake amor/amizade
  pLoveProposal = pService->createCharacteristic(
    LOVE_PROPOSAL_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pLoveProposal->setCallbacks(new LoveProposalCallbacks());

  pLoveResponse = pService->createCharacteristic(
    LOVE_RESPONSE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pLoveResponse->setValue(lastLoveResp.c_str());

  pService->start();
  pServer->getAdvertising()->start();

  BLEDevice::getScan()->setActiveScan(true);

  showEmoteOnDisplay();

  Serial.println("Digite: FELIZ/TRISTE/ENTEDIADO/BRAVO/NORMAL/APAIXONADO ou AUTO.");
}

void loop() {
  processSerialCommands();

  // update buzzer
  portENTER_CRITICAL(&buzzerMux);
  buzzer.update();
  portEXIT_CRITICAL(&buzzerMux);

  // Aplica decay do amor se sumiu o parceiro
  applyLoveDecayIfMissingPartner();

  // Botão de carinho
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonTime > 500)) {
    lastButtonTime = millis();

    bool alterouFeliz = false, alterouTriste = false, alterouBravo = false, alterouEnt = false;

    if (pctFeliz < 100) { pctFeliz += CARINHO_UP_FELIZ; alterouFeliz = true; }
    if (pctTriste > 0) { pctTriste -= CARINHO_DOWN_OUTRAS; alterouTriste = true; }
    if (pctBravo  > 0) { pctBravo  -= CARINHO_DOWN_OUTRAS; alterouBravo  = true; }
    if (pctEntediado > 0) { pctEntediado -= CARINHO_DOWN_OUTRAS; alterouEnt = true; }

    normalizaEmocoesAvancada(alterouFeliz, alterouTriste, alterouBravo, alterouEnt, false);
    saveHumorToEEPROM();
    pCharacteristic->setValue(getHumorJSON().c_str());

    // >>> agora som SEMPRE começa junto com a emoção (na uiTask)
    requestOverrideEmotion("feliz", true, true);

    Serial.println("[CARINHO] Felicidade +3, Tristeza/Bravo/Tédio -1.");
  }

  // Decaimento por inatividade (se não estiver forçando emoção)
  static unsigned long lastDecay = millis();
  if (millis() - lastDecay > DECAY_INTERVAL_MS && forcedEmotion.length() == 0) {
    lastDecay = millis();

    if (pctApaixonado > 0 && !localIsInLove()) {
      // se não tem parceiro, amor vai caindo normalmente
      pctApaixonado = max(0, pctApaixonado - 1);
    }

    if (pctFeliz > 0) {
      pctFeliz -= 5;
      if (random(2) == 0) pctTriste += 1;
      else pctEntediado += 1;
      normalizaEmocoesAvancada(false, true, false, true, false);
      Serial.println("[DECAIMENTO] Felicidade -5, Tristeza ou Tédio +1.");
    } else {
      if (random(2) == 0 && pctTriste < 100) pctTriste += 1;
      else if (pctEntediado < 100) pctEntediado += 1;
      normalizaEmocoesAvancada(false, true, false, true, false);
      Serial.println("[DECAIMENTO] Tristeza ou Tédio +1.");
    }

    saveHumorToEEPROM();
    pCharacteristic->setValue(getHumorJSON().c_str());
  }

  // Scan BLE (agendado)
  static BLEScan* pBLEScan = BLEDevice::getScan();

  const unsigned long SCAN_INTERVAL_DISCONNECTED_MS = 1200;
  const unsigned long SCAN_INTERVAL_CONNECTED_MS    = 6000;

  bool ranScan = false;
  BLEScanResults* results = nullptr;

  unsigned long scanEvery = bleConnected ? SCAN_INTERVAL_CONNECTED_MS : SCAN_INTERVAL_DISCONNECTED_MS;
  if (millis() - lastScanMs >= scanEvery) {
    lastScanMs = millis();
    ranScan = true;

    pBLEScan->setActiveScan(!bleConnected);
    results = pBLEScan->start(1, false);
  }

  bool emInteracao = false;

  if (ranScan && results) {
    for (int i = 0; i < results->getCount(); i++) {
      BLEAdvertisedDevice device = results->getDevice(i);
      String nameStd = device.getName();

      if (nameStd.length() > 0 && nameStd.indexOf("DeskBuddy") != -1) {
        String nomePuro = extraiNomeDeskBuddy(nameStd);

        if (nomePuro.length() > 0 && nomePuro != String(NAME) && !jaTemNomeNoBuffer(nomePuro)) {
          encontrados[idxEncontrado] = nomePuro;
          idxEncontrado = (idxEncontrado + 1) % MAX_ENCONTRADOS;
          if (countEncontrados < MAX_ENCONTRADOS) countEncontrados++;
          salvaBufferEncontradosEEPROM();
        }

        int idx = defineRelacaoIndex(nomePuro);
        Relacao &rel = relacoes[idx];

        unsigned long nowInteract = millis();
        if (nowInteract - lastInteractionTime < DECAY_INTERVAL_MS) {
          Serial.println("[INTERACAO] Cooldown ativo, ignorando interação.");
          break;
        }
        lastInteractionTime = nowInteract;

        rel.contador++;

        Serial.print("[BUDDY] Encontrado: ");
        Serial.print(nomePuro);
        Serial.print(" | Interações: ");
        Serial.println(rel.contador);

        // Se é o parceiro, atualiza “visto”
        if (localIsInLove() && currentPartner == nomePuro) {
          lastSeenPartnerMs = millis();
        }

        // Define relação na 2ª interação
        if (!rel.relacaoDefinida && rel.contador >= INTERACOES_PRA_DEFINIR_RELACAO) {
          int sorte = random(100);
          rel.gosta = (sorte < 70);
          rel.relacaoDefinida = true;
          rel.segundaChanceConcedida = false;

          if (rel.gosta) Serial.println("[RELACAO] Após 2 interações: GOSTA (70%)");
          else Serial.println("[RELACAO] Após 2 interações: NÃO GOSTA (30%) - Segunda chance após 6 interações.");
        }
        // Segunda chance (se não gostava)
        else if (rel.relacaoDefinida && !rel.gosta && !rel.segundaChanceConcedida && rel.contador >= INTERACOES_PRA_SEGUNDA_CHANCE) {
          int sorte2 = random(100);
          rel.gosta = (sorte2 < 50);
          rel.segundaChanceConcedida = true;

          if (rel.gosta) Serial.println("[RELACAO] Segunda chance: AGORA GOSTA (50%)");
          else Serial.println("[RELACAO] Segunda chance: CONTINUA NÃO GOSTANDO (50%)");
        }

        // Aplica emoção conforme a relação
        if (rel.relacaoDefinida) {
          if (rel.gosta) {
            aplicaEfeitoGosta(rel, nomePuro);

            // >>> NOVO: amor só pode existir via HANDSHAKE recíproco
            // Só tenta handshake se:
            // - ambos já se gostam (o outro só confirma se ele gostar e aceitar)
            // - já passou do limiar de interações
            // - eu não estou apaixonado por outro
            if (!localIsInLove() || currentPartner == nomePuro) {
              if (rel.contador >= INTERACOES_PRA_APAIXONAR && !rel.apaixonado) {
                // tenta handshake (client) com o device encontrado
                (void)doHandshakeWith(&device, nomePuro);
                // rebuild caso tenha sido marcado via resposta
                rebuildPartnerFromRelacoes();
              }
            }

            // emoção de interação (se virou amor, mostra apaixonado; senão feliz)
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
        pCharacteristic->setValue(getHumorJSON().c_str());

        emInteracao = true;
        break;
      }
    }
  }

  if (!emInteracao) {
    showEmoteOnDisplay();
  }
}
