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

#define NAME "Silve"
#define SENHA "oi24"
#define BUTTON_PIN 6
#define EEPROM_SIZE 1024

#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define MAX_ENCONTRADOS 10
#define TAM_NOME 16
#define BUZZER_PIN 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BLECharacteristic *pCharacteristic;
BuzzerScheduler buzzer;
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
};
Relacao relacoes[MAX_ENCONTRADOS];

// -- NOVO: controle de animações periódicas --
unsigned long lastBravoAnim = 0;   // última vez que bateu bravo
unsigned long lastFelizAnim = 0;   // última vez que balançou feliz
const unsigned long ANIM_INTERVAL = 7000; // 10 segundos

// === UTILITÁRIOS GERAIS E EMOÇÃO ===
void limpaTodaEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0x00);
  }
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
        salvaRelacoesEEPROM();
        return i;
      }
    }
  }
  return idx;
}

void inicializaEEPROMSempre() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, 70);
  EEPROM.write(1, 10);
  EEPROM.write(2, 10);
  EEPROM.write(3, 10);
  EEPROM.write(4, 0);
  EEPROM.write(5, 0);
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

void normalizaEmocoesAvancada(bool acaoFoiFeliz = false, bool acaoFoiTriste = false, bool acaoFoiBravo = false, bool acaoFoiEntediado = false) {
  int maxNeg = max(pctTriste, pctBravo);
  if (acaoFoiFeliz && pctFeliz + maxNeg > 100) {
    int excesso = pctFeliz + maxNeg - 100;
    if (pctTriste >= pctBravo) {
      pctTriste = max(0, pctTriste - excesso);
    } else {
      pctBravo = max(0, pctBravo - excesso);
    }
  }
  pctFeliz = constrain(pctFeliz, 0, 100);
  pctTriste = constrain(pctTriste, 0, 100);
  pctBravo = constrain(pctBravo, 0, 100);
  pctEntediado = constrain(pctEntediado, 0, 100);
}

String getDominantEmotion() {
  if (forcedEmotion.length() > 0) return forcedEmotion;
  int lista[4] = {pctFeliz, pctTriste, pctEntediado, pctBravo};
  const char* nomes[4] = {"feliz", "triste", "entediado", "bravo"};
  int idx = 0;
  for (int i = 1; i < 4; i++) {
    if (lista[i] > lista[idx]) idx = i;
  }
  if (lista[idx] > 50) {
    return String(nomes[idx]);
  } else {
    return "normal";
  }
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
      }
      json += "}";
      first = false;
    }
  }
  json += "]";
  json += "}";
  return json;
}

// --- SOM NO BUZZER (NÃO-BLOQUEANTE) ---
// Baseado nos padrões da biblioteca CuteBuzzerSounds (mas sem delay, para não travar o display)
String lastSoundEmotion = "";
unsigned long lastEmotionSoundAt = 0;

uint8_t soundForEmotion(const String &emocao, bool variantShort=false) {
  if (emocao == "feliz")        return variantShort ? S_HAPPY_SHORT : S_HAPPY;
  if (emocao == "triste")       return S_SAD;
  if (emocao == "entediado")    return S_SLEEPING;
  if (emocao == "bravo")        return S_MODE3;
  if (emocao == "apaixonado")   return S_CUDDLY;
  // normal / fallback
  return S_CONNECTION;
}

void playEmotionSoundNow(const String &emocao, bool variantShort=false) {
  buzzer.playSound(soundForEmotion(emocao, variantShort));
  lastEmotionSoundAt = millis();
}

void showEmoteOnDisplay() {
  String dominante = getDominantEmotion();

  // desenha a emoção (as animações internas chamam smartDelay(), que mantém o buzzer rodando)
  if (dominante == "feliz") {
    happy(0,0,75);
  } else if (dominante == "triste") {
    sad(0,0,75);
  } else if (dominante == "entediado") {
    bored(0,0,75);
  } else if (dominante == "apaixonado") {
    loving(0,0,75);
  } else if (dominante == "bravo") {
    angry(0,0,75);
  } else {
    normal(0,0,75);
  }

  // som ao trocar de emoção dominante
  if (dominante != lastSoundEmotion) {
    lastSoundEmotion = dominante;
    playEmotionSoundNow(dominante);
  }
}


class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE Conectado");
  }
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE Desconectado");
    delay(200);
    pServer->getAdvertising()->start();
  }
};

String extraiNomeDeskBuddy(String full) {
  int idx = full.indexOf(":");
  if (idx != -1 && idx + 2 < full.length()) {
    return full.substring(idx + 2);
  }
  return full;
}

bool jaTemNomeNoBuffer(String nome) {
  for (int i = 0; i < MAX_ENCONTRADOS; i++) {
    if (encontrados[i] == nome) return true;
  }
  return false;
}

// --- Serial: comandos para forçar emoção (feliz/triste/entediado/bravo/normal/auto) ---
void processSerialCommands() {
  static String input = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      input.trim();
      if (input.length() > 0) {
        input.toLowerCase();
        if (input == "feliz") {
          forcedEmotion = "feliz";
          Serial.println("[EMOÇÃO FORÇADA] feliz");
        } else if (input == "triste") {
          forcedEmotion = "triste";
          Serial.println("[EMOÇÃO FORÇADA] triste");
        } else if (input == "entediado") {
          forcedEmotion = "entediado";
          Serial.println("[EMOÇÃO FORÇADA] entediado");
        } else if (input == "bravo") {
          forcedEmotion = "bravo";
          Serial.println("[EMOÇÃO FORÇADA] bravo");
        } else if (input == "normal") {
          forcedEmotion = "normal";
          Serial.println("[EMOÇÃO FORÇADA] normal");
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

void setup() {
  Serial.begin(115200);
  Wire.begin(2, 3);

  limpaTodaEEPROM();

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

  loadHumorFromEEPROM();

  BLEDevice::init(String("DeskBuddy: ") + NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
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

  Serial.println("Digite FELIZ, TRISTE, ENTEDIADO, BRAVO, NORMAL ou AUTO para forçar a emoção.");
  }

void loop() {
  processSerialCommands();
  buzzer.update();

  String dom = getDominantEmotion();

  // Som periódico bravo
  if (dom == "bravo" && millis() - lastBravoAnim > ANIM_INTERVAL) {
    playEmotionSoundNow("bravo");
    lastBravoAnim = millis();
  }

  // Som periódico feliz
  if (dom == "feliz" && millis() - lastFelizAnim > ANIM_INTERVAL) {
    playEmotionSoundNow("feliz", true);
    lastFelizAnim = millis();
  }

  // Botão de carinho
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonTime > 500)) {
    lastButtonTime = millis();

    bool alterouFeliz = false, alterouTriste = false, alterouBravo = false;
    if (pctFeliz < 100) { pctFeliz += 3; alterouFeliz = true; }
    if (pctTriste > 0) { pctTriste -= 1; alterouTriste = true; }
    if (pctBravo > 0) { pctBravo -= 1; alterouBravo = true; }
    if (pctEntediado > 0) pctEntediado -= 1;
    normalizaEmocoesAvancada(alterouFeliz, alterouTriste, alterouBravo, false);
    saveHumorToEEPROM();
    estadoDisplay = "feliz";
    estadoDisplayTimeout = millis() + 2000;
    pCharacteristic->setValue(getHumorJSON().c_str());
    happy(0,0,75);
    estadoDisplay = "";
    pCharacteristic->setValue(getHumorJSON().c_str());
    Serial.println("[CARINHO] Botão pressionado. Felicidade +3, Tristeza/Bravo/Entediado -1.");
    playEmotionSoundNow("feliz", true);
    lastFelizAnim = millis(); // reinicia o timer do balançar para não dar duplo trigger
  }

  // Decaimento por inatividade (3 segundos debug)
  static unsigned long lastDecay = millis();
  if (millis() - lastDecay > 3000 && forcedEmotion.length() == 0) {
    lastDecay = millis();
    if (pctFeliz > 0) {
      pctFeliz -= 5;
      if (random(2) == 0) pctTriste += 1;
      else pctEntediado += 1;
      normalizaEmocoesAvancada(false, true, false, false);
      saveHumorToEEPROM();
      pCharacteristic->setValue(getHumorJSON().c_str());
      Serial.println("[DECAIMENTO] Felicidade -5, Tristeza ou Tédio +1.");
    } else {
      if (random(2) == 0 && pctTriste < 100) pctTriste += 1;
      else if (pctEntediado < 100) pctEntediado += 1;
      normalizaEmocoesAvancada(false, true, false, true);
      saveHumorToEEPROM();
      pCharacteristic->setValue(getHumorJSON().c_str());
      Serial.println("[DECAIMENTO] Tristeza ou Tédio +1.");
    }
  }

  BLEScan* pBLEScan = BLEDevice::getScan();
  BLEScanResults* results = pBLEScan->start(1, false);

  String nomeBuddyAtual = "";
  bool emInteracao = false;

  for (int i = 0; i < results->getCount(); i++) {
    BLEAdvertisedDevice device = results->getDevice(i);
    String nameStd = device.getName();
    if (nameStd.length() > 0 && nameStd.indexOf("DeskBuddy") != -1) {
      String nomePuro = extraiNomeDeskBuddy(nameStd);
      nomeBuddyAtual = nomePuro;

      if (nomePuro.length() > 0 && nomePuro != String(NAME) && !jaTemNomeNoBuffer(nomePuro)) {
        encontrados[idxEncontrado] = nomePuro;
        idxEncontrado = (idxEncontrado + 1) % MAX_ENCONTRADOS;
        if (countEncontrados < MAX_ENCONTRADOS) countEncontrados++;
        salvaBufferEncontradosEEPROM();
      }

      int idx = defineRelacaoIndex(nomePuro);
      Relacao &rel = relacoes[idx];
      rel.contador++;
      salvaRelacoesEEPROM();

      Serial.print("[BUDDY] Encontrado: ");
      Serial.print(nomePuro);
      Serial.print(" | Interações: ");
      Serial.println(rel.contador);

      if (!rel.relacaoDefinida && rel.contador >= 2) {
        int sorte = random(100);
        rel.gosta = (sorte < 70);
        rel.relacaoDefinida = true;
        rel.segundaChanceConcedida = false;
        salvaRelacoesEEPROM();
        if (rel.gosta) {
          Serial.println("[RELACAO] Após 2 interações: GOSTA (70%)");
        } else {
          Serial.println("[RELACAO] Após 2 interações: NÃO GOSTA (30%) - Segunda chance após 4 interações.");
        }
      }
      else if (rel.relacaoDefinida && !rel.gosta && !rel.segundaChanceConcedida && rel.contador >= 6) {
        int sorte2 = random(100);
        rel.gosta = (sorte2 < 50);
        rel.segundaChanceConcedida = true;
        salvaRelacoesEEPROM();
        if (rel.gosta) {
          Serial.println("[RELACAO] Segunda chance após 4 interações: AGORA GOSTA (50%)");
        } else {
          Serial.println("[RELACAO] Segunda chance após 4 interações: CONTINUA NÃO GOSTANDO (50%)");
        }
      }

      if (rel.relacaoDefinida && (!rel.gosta || (rel.segundaChanceConcedida && !rel.gosta))) {
        pctEntediado = min(100, pctEntediado + 1);
        pctBravo = min(100, pctBravo + 1);
        angry(0, 0, 75);
        saveHumorToEEPROM();
        pCharacteristic->setValue(getHumorJSON().c_str());
        Serial.print("[EMOCAO] NÃO gosta de ");
        Serial.print(nomePuro);
        Serial.print(". Tédio e raiva +1 (");
        Serial.print(pctEntediado);
        Serial.print("/");
        Serial.print(pctBravo);
        Serial.println(")");
        lastBravoAnim = millis();
      } else if (rel.relacaoDefinida && rel.gosta) {
        if (pctTriste > 0) pctTriste--;
        if (pctEntediado > 0) pctEntediado--;
        if (pctBravo > 0) pctBravo--;
        pctFeliz = min(100, pctFeliz + 1);
        happy(0, 0, 75);
        saveHumorToEEPROM();
        pCharacteristic->setValue(getHumorJSON().c_str());
        Serial.print("[EMOCAO] Gosta de ");
        Serial.print(nomePuro);
        Serial.print(". Felicidade +1 (");
        Serial.print(pctFeliz);
        Serial.println("), tristeza/tédio/raiva -1.");
        lastFelizAnim = millis();
      } else {
        suspicion(0, 0, 75);
    
      }

      emInteracao = true;
      break;
    }
  }

  if (!emInteracao) {
    showEmoteOnDisplay();
  }
}