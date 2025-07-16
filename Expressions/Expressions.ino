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
#define BUTTON_PIN 37        // Ajuste conforme seu hardware
#define EEPROM_SIZE 64
#define MAX_INTERACTION_INTERVAL 100

#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BLECharacteristic *pCharacteristic;

unsigned long lastInteractionTime = 0;
unsigned long lastButtonTime = 0;

// Porcentagens de humor
int pctNormal = 50, pctFeliz = 30, pctTriste = 10, pctEntediado = 10, pctBravo = 0, pctApaixonado = 0;

// Estado atual do display ("" = animação padrão dominante; "suspeito" = animando suspicion)
String estadoDisplay = "";            // Visível no app via JSON
unsigned long estadoDisplayTimeout = 0; // Timeout para animação especial (ms)

// Determina emoção dominante, respeitando animação especial temporária
String getDominantEmotion() {
  if (estadoDisplay != "" && millis() < estadoDisplayTimeout) return estadoDisplay;
  int humores[6] = {pctNormal, pctFeliz, pctTriste, pctEntediado, pctBravo, pctApaixonado};
  const char* nomes[6] = {"normal", "feliz", "triste", "entediado", "bravo", "apaixonado"};
  int idx = 0, maxValue = humores[0];
  for (int i = 1; i < 6; i++) {
    if (humores[i] > maxValue) {
      maxValue = humores[i];
      idx = i;
    }
  }
  return String(nomes[idx]);
}

// JSON do humor com dominante
String getHumorJSON() {
  String json = "{";
  json += "\"normal\":"     + String(pctNormal)     + ",";
  json += "\"feliz\":"      + String(pctFeliz)      + ",";
  json += "\"triste\":"     + String(pctTriste)     + ",";
  json += "\"entediado\":"  + String(pctEntediado)  + ",";
  json += "\"bravo\":"      + String(pctBravo)      + ",";
  json += "\"apaixonado\":" + String(pctApaixonado) + ",";
  json += "\"dominante\":\"" + getDominantEmotion() + "\"";
  json += "}";
  return json;
}

void loadHumorFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  pctNormal = EEPROM.read(0);
  pctFeliz = EEPROM.read(1);
  pctTriste = EEPROM.read(2);
  pctEntediado = EEPROM.read(3);
  pctBravo = EEPROM.read(4);
  pctApaixonado = EEPROM.read(5);
}

void saveHumorToEEPROM() {
  EEPROM.write(0, pctNormal);
  EEPROM.write(1, pctFeliz);
  EEPROM.write(2, pctTriste);
  EEPROM.write(3, pctEntediado);
  EEPROM.write(4, pctBravo);
  EEPROM.write(5, pctApaixonado);
  EEPROM.commit();
}

// Mostra animação no display conforme o humor dominante
void showEmoteOnDisplay() {
  int maxValue = pctNormal, idx = 0;
  int humores[6] = {pctNormal, pctFeliz, pctTriste, pctEntediado, pctBravo, pctApaixonado};
  for (int i = 1; i < 6; i++) {
    if (humores[i] > maxValue) {
      maxValue = humores[i];
      idx = i;
    }
  }
  switch(idx) {
    case 0: normal(0,0,75); break;
    case 1: happy(0,0,75); break;
    case 2: sad(0,0,75); break;
    case 3: hectic(0,0,75); break;
    case 4: angry(0,0,75); break;
    case 5: loving(0,0,75); break;
    default: normal(0,0,75);
  }
}

// BLE: reconectar se desconectar
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

void setup() {
  Serial.begin(115200);

  // Inicializa I2C
  Wire.begin(32, 33);

  // Inicializa display
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

  // Botão
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // BLE Server (compatível app Flutter)
  BLEDevice::init("DeskBuddy");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );
  loadHumorFromEEPROM();
  pCharacteristic->setValue(getHumorJSON().c_str());

  pService->start();
  pServer->getAdvertising()->start();

  // Inicial BLE Scan
  BLEDevice::getScan()->setActiveScan(true);

  showEmoteOnDisplay();
}

void loop() {
  // Botão para aumentar felicidade
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonTime > 500)) {
    lastButtonTime = millis();
    pctFeliz = min(pctFeliz + 5, 100);
    pctNormal = max(pctNormal - 2, 0);
    lastInteractionTime = 0;
    saveHumorToEEPROM();
    estadoDisplay = "feliz";
    estadoDisplayTimeout = millis() + 2000; // 2 segundos
    pCharacteristic->setValue(getHumorJSON().c_str());
    happy(0,0,75);
    estadoDisplay = "";
    pCharacteristic->setValue(getHumorJSON().c_str());
  }

  // Simulação: a cada 15s fica mais triste se ninguém interage
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 15000) {
    lastUpdate = millis();
    if (lastInteractionTime > MAX_INTERACTION_INTERVAL) {
      pctTriste = min(pctTriste + 1, 100);
      pctFeliz = max(pctFeliz - 1, 0);
      pctNormal = max(pctNormal - 1, 0);
      saveHumorToEEPROM();
      estadoDisplay = "triste";
      estadoDisplayTimeout = millis() + 2000; // 2 segundos
      pCharacteristic->setValue(getHumorJSON().c_str());
      sad(0,0,75);
      estadoDisplay = "";
      pCharacteristic->setValue(getHumorJSON().c_str());
    }
  }

  // BLE Scan: detectar outros DeskBuddy
  BLEScan* pBLEScan = BLEDevice::getScan();
  BLEScanResults results = pBLEScan->start(1, false);

  int maxRSSI = -100;
  bool foundDeskBuddy = false;
  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice device = results.getDevice(i);
    std::string name = device.getName();
    if (!name.empty() && name.find("DeskBuddy") != std::string::npos) {
      foundDeskBuddy = true;
      int rssi = device.getRSSI();
      if (rssi > maxRSSI) {
        maxRSSI = rssi;
        lastInteractionTime = 0;
        pctNormal = min(pctNormal + 1, 100);
        saveHumorToEEPROM();
        estadoDisplay = "suspeito";
        estadoDisplayTimeout = millis() + 2000; // 2 segundos
        pCharacteristic->setValue(getHumorJSON().c_str());
        suspicion(0, 0, 75);
        estadoDisplay = "";
        pCharacteristic->setValue(getHumorJSON().c_str());
      }
    }
  }
  // Se não achou outro DeskBuddy, animação padrão pelo humor dominante
  if (!foundDeskBuddy) {
    if (lastInteractionTime <= MAX_INTERACTION_INTERVAL) lastInteractionTime++;
    showEmoteOnDisplay();
  }
}
