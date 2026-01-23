#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <EEPROM.h>
#include "images.h"
#include "controller.h"

#include "BuzzerScheduler.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define NAME "Kizmo"
#define SENHA "oi23"
#define BUTTON_PIN 6
#define EEPROM_SIZE 1024

#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define MAX_ENCONTRADOS 10
#define TAM_NOME 16

#define BUZZER_PIN 10

// ======== AJUSTES IMPORTANTES ========
const unsigned long ANIM_INTERVAL = 7000; // 7s
const unsigned long DECAY_INTERVAL_MS = 3000;

const int INTERACOES_PRA_DEFINIR_RELACAO = 2;
const int INTERACOES_PRA_SEGUNDA_CHANCE  = 6;
const int INTERACOES_PRA_APAIXONAR       = 10;
const int CHANCE_APAIXONAR_PERCENT       = 25;

const int CARINHO_UP_FELIZ = 3;
const int CARINHO_DOWN_OUTRAS = 1;

// ======== OBJETOS ========
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BLECharacteristic *pCharacteristic;
BuzzerScheduler buzzer;

// ======== BLE connection state ========
volatile bool bleConnected = false;

// ======== Dominant emotion sound repeat control ========
String lastDominantEmotion = "";
uint8_t dominantSameCycles = 0;

// ======== BLE scan scheduler ========
unsigned long lastScanMs = 0;

// ======== ESTADO ========
int pctFeliz = 70, pctTriste = 10, pctEntediado = 10, pctBravo = 10, pctNormal = 0, pctApaixonado = 0;

unsigned long lastInteractionTime = 0;
unsigned long lastButtonTime = 0;

String estadoDisplay = "";
unsigned long estadoDisplayTimeout = 0;

String forcedEmotion = "";

String encontrados[MAX_ENCONTRADOS];
int idxEncontrado = 0;
int countEncontrados = 0;

struct Relacao {
  char nome[TAM_NOME];
  bool gosta;
  int contador;
  bool relacaoDefinida;
  bool segundaChanceConcedida;

  bool apaixonado;
  int afinidade;
};
Relacao relacoes[MAX_ENCONTRADOS];

// Controle de sons/animações periódicas
unsigned long lastBravoAnim = 0;
unsigned long lastFelizAnim = 0;
unsigned long lastLoveAnim  = 0;

// ======== EEPROM ========
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

// ======== UTIL ========
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

// ======== BUZZER (não-bloqueante) ========
String lastSoundEmotion = "";
unsigned long lastEmotionSoundAt = 0;

uint8_t soundForEmotion(const String &emocao, bool variantShort=false) {
  if (emocao == "feliz")        return variantShort ? S_HAPPY_SHORT : S_HAPPY;
  if (emocao == "triste")       return S_SAD;
  if (emocao == "entediado")    return S_SLEEPING;
  if (emocao == "bravo")        return S_MODE3;
  if (emocao == "apaixonado")   return S_CUDDLY;
  return S_CONNECTION;
}

void playEmotionSoundNow(const String &emocao, bool variantShort=false) {
  buzzer.playSound(soundForEmotion(emocao, variantShort));
  lastEmotionSoundAt = millis();
}

// >>> NOVO: som baseado na emoção MOSTRADA na tela (mesmo que não seja dominante)
uint8_t soundForScreenEmotion(const String &telaEmocao, bool variantShort=false) {
  // Mapeia emoções de “tela”
  if (telaEmocao == "feliz")        return soundForEmotion("feliz", variantShort);
  if (telaEmocao == "bravo")        return soundForEmotion("bravo", false);
  if (telaEmocao == "apaixonado")   return soundForEmotion("apaixonado", variantShort);
  if (telaEmocao == "triste")       return soundForEmotion("triste", false);
  if (telaEmocao == "entediado")    return soundForEmotion("entediado", false);

  // “suspeita”/neutro: usa um som de “conexão/curioso”
  if (telaEmocao == "suspeita")     return S_CONNECTION;

  // fallback
  return S_CONNECTION;
}

void playScreenEmotionSoundNow(const String &telaEmocao, bool variantShort=false) {
  buzzer.playSound(soundForScreenEmotion(telaEmocao, variantShort));
  lastEmotionSoundAt = millis();
}
// <<< FIM NOVO

void buzzerMaxIfSupported() {
  // intencionalmente vazio pra não quebrar compilação
}

// ======== DISPLAY ========
void showEmoteOnDisplay() {
  String dominante = getDominantEmotion();

  bool shouldPlay = false;
  bool emotionChanged = (dominante != lastDominantEmotion);

  if (emotionChanged) {
    lastDominantEmotion = dominante;
    dominantSameCycles = 0;
    shouldPlay = true;
  } else {
    dominantSameCycles++;
    if (dominantSameCycles >= 3) {
      dominantSameCycles = 0;
      shouldPlay = true;
    }
  }

  if (shouldPlay) {
    if (emotionChanged || !buzzer.isPlaying()) {
      bool shortVariant = (dominante == "feliz" || dominante == "apaixonado");
      playEmotionSoundNow(dominante, shortVariant);
    }
  }

  if (dominante == "apaixonado") {
    loving(0,0,75);
  } else if (dominante == "feliz") {
    happy(0,0,75);
  } else if (dominante == "triste") {
    sad(0,0,75);
  } else if (dominante == "entediado") {
    bored(0,0,75);
  } else if (dominante == "bravo") {
    angry(0,0,75);
  } else {
    normal(0,0,75);
  }
}

// ======== BLE ========
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    Serial.println("BLE Conectado");
  }
  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    Serial.println("BLE Desconectado");
    delay(50);
    pServer->getAdvertising()->start();
  }
};

// ======== SERIAL (forçar emoção) ========
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

// ======== LÓGICA SOCIAL / AMOR ========
void aplicaEfeitoGosta(Relacao &rel, const String &nomePuro) {
  rel.afinidade = min(100, rel.afinidade + 2);

  if (pctTriste > 0) pctTriste--;
  if (pctEntediado > 0) pctEntediado--;
  if (pctBravo > 0) pctBravo--;
  pctFeliz = min(100, pctFeliz + 1);

  if (!rel.apaixonado && rel.contador >= INTERACOES_PRA_APAIXONAR) {
    int chance = CHANCE_APAIXONAR_PERCENT;
    if (rel.afinidade >= 30) chance += 10;
    if (rel.afinidade >= 60) chance += 10;
    chance = min(chance, 80);

    int sorte = random(100);
    if (sorte < chance) {
      rel.apaixonado = true;
      pctApaixonado = min(100, pctApaixonado + 15);
      Serial.print("[AMOR] ");
      Serial.print(nomePuro);
      Serial.print(" -> APAIXONOU! (chance ");
      Serial.print(chance);
      Serial.println("%)");
      playEmotionSoundNow("apaixonado");
      lastLoveAnim = millis();
    }
  }

  if (rel.apaixonado) {
    pctApaixonado = min(100, pctApaixonado + 2);
    pctFeliz = min(100, pctFeliz + 1);
  }

  normalizaEmocoesAvancada(true, false, false, false, rel.apaixonado);
}

void aplicaEfeitoNaoGosta(Relacao &rel, const String &nomePuro) {
  rel.afinidade = max(-100, rel.afinidade - 2);

  pctEntediado = min(100, pctEntediado + 1);
  pctBravo     = min(100, pctBravo + 1);

  if (rel.apaixonado) {
    pctApaixonado = max(0, pctApaixonado - 3);
    pctTriste = min(100, pctTriste + 2);
  }

  normalizaEmocoesAvancada(false, true, true, true, false);
}

void aplicaEfeitoSuspeita(Relacao &rel) {
  rel.afinidade = constrain(rel.afinidade + (random(3) - 1), -100, 100);
}

// ======== SETUP/LOOP ========
void setup() {
  Serial.begin(115200);
  Wire.begin(2, 3);

  // Se você NÃO quer resetar tudo sempre, comenta a linha abaixo:
  limpaTodaEEPROM();
  // inicializaEEPROMSempre(); // alternativa

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

  buzzer.begin(BUZZER_PIN);
  buzzerMaxIfSupported();

  loadHumorFromEEPROM();

  BLEDevice::init(String("DeskBuddy: ") + NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pServer->getAdvertising()->setMinPreferred(0x06);
  pServer->getAdvertising()->setMinPreferred(0x12);

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );
  pCharacteristic->setValue(getHumorJSON().c_str());

  pService->start();
  pServer->getAdvertising()->start();
  BLEDevice::getScan()->setActiveScan(true);

  showEmoteOnDisplay();

  Serial.println("Digite: FELIZ/TRISTE/ENTEDIADO/BRAVO/NORMAL/APAIXONADO ou AUTO.");
}

void loop() {
  processSerialCommands();
  buzzer.update();

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
    // som + tela (carinho sempre é feliz)
    playScreenEmotionSoundNow("feliz", true);
    happy(0,0,75);

    Serial.println("[CARINHO] Felicidade +3, Tristeza/Bravo/Tédio -1.");
    lastFelizAnim = millis();
  }

  // Decaimento por inatividade (se não estiver forçando emoção)
  static unsigned long lastDecay = millis();
  if (millis() - lastDecay > DECAY_INTERVAL_MS && forcedEmotion.length() == 0) {
    lastDecay = millis();

    if (pctApaixonado > 0) pctApaixonado = max(0, pctApaixonado - 1);

    if (pctFeliz > 0) {
      pctFeliz -= 5;
      if (random(2) == 0) pctTriste += 1;
      else pctEntediado += 1;
      normalizaEmocoesAvancada(false, true, false, true, false);
      Serial.println("[DECAIMENTO] Felicidade -5, Tristeza ou Tédio +1, Amor -1.");
    } else {
      if (random(2) == 0 && pctTriste < 100) pctTriste += 1;
      else if (pctEntediado < 100) pctEntediado += 1;
      normalizaEmocoesAvancada(false, true, false, true, false);
      Serial.println("[DECAIMENTO] Tristeza ou Tédio +1, Amor -1.");
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
        rel.contador++;

        Serial.print("[BUDDY] Encontrado: ");
        Serial.print(nomePuro);
        Serial.print(" | Interações: ");
        Serial.println(rel.contador);

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

            if (rel.apaixonado || pctApaixonado > 50) {
              // >>> NOVO: som da emoção que vai aparecer na tela
              playScreenEmotionSoundNow("apaixonado", true);
              loving(0, 0, 75);
              lastLoveAnim = millis();
            } else {
              // >>> NOVO: som da emoção que vai aparecer na tela
              playScreenEmotionSoundNow("feliz", true);
              happy(0, 0, 75);
              lastFelizAnim = millis();
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

            // >>> NOVO: som da emoção que vai aparecer na tela (bravo)
            playScreenEmotionSoundNow("bravo", false);
            angry(0, 0, 75);
            lastBravoAnim = millis();

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

          // >>> NOVO: som da emoção que vai aparecer na tela (suspeita)
          playScreenEmotionSoundNow("suspeita", false);
          suspicion(0, 0, 75);
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
