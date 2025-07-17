#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <EEPROM.h>
#include "emotes.h"
#include "teste.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define NAME "kizmo"
#define BUTTON_PIN 26           // Botão agora no pino 26
#define EEPROM_SIZE 64
#define MAX_INTERACTION_INTERVAL 100

#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define MAX_ENCONTRADOS 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BLECharacteristic *pCharacteristic;

unsigned long lastInteractionTime = 0;
unsigned long lastButtonTime = 0;

// Emoções (sempre total 100%)
int pctFeliz = 70, pctTriste = 10, pctEntediado = 10, pctBravo = 10, pctNormal = 0, pctApaixonado = 0;

// Estado do display
String estadoDisplay = "";
unsigned long estadoDisplayTimeout = 0;

// Buffer circular dos encontrados
String encontrados[MAX_ENCONTRADOS];
int idxEncontrado = 0;
int countEncontrados = 0;

// -------- EEPROM sempre inicializa --------
void inicializaEEPROMSempre() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, 70);   // felicidade
  EEPROM.write(1, 10);   // tristeza
  EEPROM.write(2, 10);   // tédio
  EEPROM.write(3, 10);   // raiva
  EEPROM.write(4, 0);    // normal
  EEPROM.write(5, 0);    // apaixonado
  EEPROM.commit();
}

// -------- LOAD/SAVE --------
void loadHumorFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  pctFeliz      = EEPROM.read(0);
  pctTriste     = EEPROM.read(1);
  pctEntediado  = EEPROM.read(2);
  pctBravo      = EEPROM.read(3);
  pctNormal     = EEPROM.read(4);
  pctApaixonado = EEPROM.read(5);
}

void saveHumorToEEPROM() {
  EEPROM.write(0, pctFeliz);
  EEPROM.write(1, pctTriste);
  EEPROM.write(2, pctEntediado);
  EEPROM.write(3, pctBravo);
  EEPROM.write(4, pctNormal);
  EEPROM.write(5, pctApaixonado);
  EEPROM.commit();
}

// -------- Helper para ajuste automático das emoções para totalizar 100% --------
void normalizaEmocoes() {
  int soma = pctFeliz + pctTriste + pctEntediado + pctBravo + pctNormal + pctApaixonado;
  if (soma != 100) {
    int dif = 100 - soma;
    pctNormal += dif;
    if (pctNormal < 0) pctNormal = 0;
    if (pctNormal > 100) pctNormal = 100;
  }
}

// ----------- BLE JSON ------------
String getDominantEmotion() {
  int lista[4] = {pctFeliz, pctTriste, pctEntediado, pctBravo};
  const char* nomes[4] = {"feliz", "triste", "entediado", "bravo"};
  int idx = 0;
  for (int i = 1; i < 4; i++) {
    if (lista[i] > lista[idx]) idx = i;
  }
  if (lista[idx] > 40) {
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
  json += "\"encontrados\":[";
  bool first = true;
  for (int i = 0; i < MAX_ENCONTRADOS; i++) {
    String nome = encontrados[(idxEncontrado + i) % MAX_ENCONTRADOS];
    if (nome.length() > 0) {
      bool jaIncluido = false;
      for (int j = 0; j < i; j++) {
        if (encontrados[(idxEncontrado + j) % MAX_ENCONTRADOS] == nome) {
          jaIncluido = true;
          break;
        }
      }
      if (!jaIncluido) {
        if (!first) json += ",";
        json += "\"" + nome + "\"";
        first = false;
      }
    }
  }
  json += "]";
  json += "}";
  return json;
}

// ---------- DISPLAY ----------
void showEmoteOnDisplay() {
  String dominante = getDominantEmotion();
  if (dominante == "feliz") {
    happy(0,0,75);
  } else if (dominante == "triste") {
    sad(0,0,75);
  } else if (dominante == "entediado") {
    suspicion(0,0,75);
  } else if (dominante == "bravo") {
    angry(0,0,75);
  } else {
    normal(0,0,75);
  }
}

// ---------- BLE CALLBACK ----------
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

// --------- Helper para buscar só nome puro ---------
String extraiNomeDeskBuddy(String full) {
  int idx = full.indexOf(":");
  if (idx != -1 && idx + 2 < full.length()) {
    return full.substring(idx + 2); // pula ": "
  }
  return full;
}

// --------- Helper para verificar se já existe ---------
bool jaTemNomeNoBuffer(String nome) {
  for (int i = 0; i < MAX_ENCONTRADOS; i++) {
    if (encontrados[i] == nome) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  Wire.begin(32, 33);

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

  inicializaEEPROMSempre();
  loadHumorFromEEPROM();

  BLEDevice::init("DeskBuddy");
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
}

void loop() {
  // --- Botão (carinho) ---
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonTime > 500)) {
    lastButtonTime = millis();

    // Felicidade +3%, triste/tédio/bravo -1% (se acima de zero)
    if (pctFeliz < 100) pctFeliz += 3;
    if (pctTriste > 0) pctTriste -= 1;
    if (pctEntediado > 0) pctEntediado -= 1;
    if (pctBravo > 0) pctBravo -= 1;
    normalizaEmocoes();
    saveHumorToEEPROM();
    estadoDisplay = "feliz";
    estadoDisplayTimeout = millis() + 2000;
    pCharacteristic->setValue(getHumorJSON().c_str());
    happy(0,0,75);
    estadoDisplay = "";
    pCharacteristic->setValue(getHumorJSON().c_str());
  }

  // --- Decaimento por inatividade (a cada 3 segundos para debug) ---
  static unsigned long lastDecay = millis();
  if (millis() - lastDecay > 500) { // 3 segundos para debug!
    lastDecay = millis();
    if (pctFeliz > 0) {
      pctFeliz -= 1;
      if (random(2) == 0) pctTriste += 1;
      else pctEntediado += 1;
      normalizaEmocoes();
      saveHumorToEEPROM();
      pCharacteristic->setValue(getHumorJSON().c_str());
    }
  }

  // ------------ ENCONTRADOS: BUFFER CIRCULAR ÚNICO, só nome puro, nunca duplica -----------
  BLEScan* pBLEScan = BLEDevice::getScan();
  BLEScanResults results = pBLEScan->start(1, false);

  int maxRSSI = -100;
  bool foundDeskBuddy = false;

  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice device = results.getDevice(i);
    std::string nameStd = device.getName();
    if (!nameStd.empty() && nameStd.find("DeskBuddy") != std::string::npos) {
      foundDeskBuddy = true;
      String nomePuro = extraiNomeDeskBuddy(String(nameStd.c_str()));
      if (nomePuro.length() > 0 && nomePuro != String(NAME) && !jaTemNomeNoBuffer(nomePuro)) {
        encontrados[idxEncontrado] = nomePuro;
        idxEncontrado = (idxEncontrado + 1) % MAX_ENCONTRADOS;
        if (countEncontrados < MAX_ENCONTRADOS) countEncontrados++;
      }

      int rssi = device.getRSSI();
      if (rssi > maxRSSI) {
        maxRSSI = rssi;
        lastInteractionTime = 0;
        pctNormal = min(pctNormal + 1, 100);
        saveHumorToEEPROM();
        estadoDisplay = "suspeito";
        estadoDisplayTimeout = millis() + 2000;
        pCharacteristic->setValue(getHumorJSON().c_str());
        suspicion(0, 0, 75);
        estadoDisplay = "";
        pCharacteristic->setValue(getHumorJSON().c_str());
      }
    }
  }
  // Display animado da emoção dominante (inclui normalização)
  if (!foundDeskBuddy) {
    if (lastInteractionTime <= MAX_INTERACTION_INTERVAL) lastInteractionTime++;
    showEmoteOnDisplay();
  }
}
