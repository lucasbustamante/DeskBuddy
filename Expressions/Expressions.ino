#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "emotes.h"
#include "teste.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define NAME "kizmo"
#define BUTTON_PIN 37

#define MAX_INTERACTION_INTERVAL 100
#define EEPROM_SIZE 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

unsigned long lastInteractionTime = 0;

// Porcentagens de humor
int pctNormal = 50;
int pctFeliz = 30;
int pctTriste = 10;
int pctEntediado = 10;
int pctBravo = 0;
int pctApaixonado = 0;

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

void handleRoot() {
  String html = "<html><body><h1>Humor do DeskBuddy</h1>";
  html += "<p>Normal: " + String(pctNormal) + "%</p>";
  html += "<p>Feliz: " + String(pctFeliz) + "%</p>";
  html += "<p>Triste: " + String(pctTriste) + "%</p>";
  html += "<p>Entediado: " + String(pctEntediado) + "%</p>";
  html += "<p>Bravo: " + String(pctBravo) + "%</p>";
  html += "<p>Apaixonado: " + String(pctApaixonado) + "%</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
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
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(NAME, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - w) / 2;
  int16_t y = (SCREEN_HEIGHT - h) / 2;
  display.setCursor(x, y);
  display.println(NAME);
  display.display();
  delay(5000);
  display.clearDisplay();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // BLE
  BLEDevice::init(std::string("DeskBuddy: ") + NAME);
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(BLEUUID((uint16_t)0x1812));
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
    BLEUUID((uint16_t)0x2A4E),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pService->start();
  pServer->getAdvertising()->start();

  // Wi-Fi AP
  WiFi.softAP("DeskBuddy", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.begin();

  loadHumorFromEEPROM();
}

void loop() {
  server.handleClient();

  if (lastInteractionTime <= MAX_INTERACTION_INTERVAL) {
    lastInteractionTime++;
  }

  if (lastInteractionTime > MAX_INTERACTION_INTERVAL) {
    pctTriste = min(pctTriste + 1, 100);
    pctFeliz = max(pctFeliz - 1, 0);
    pctNormal = max(pctNormal - 1, 0);
    sad(0, 0, 75);
  } else {
    normal(0, 0, 75);
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    lastInteractionTime = 0;
    pctFeliz = min(pctFeliz + 5, 100);
    pctNormal = max(pctNormal - 2, 0);
    happy(0, 0, 75);
    delay(50);
    saveHumorToEEPROM();
  }

  // BLE Scan
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  BLEScanResults results = pBLEScan->start(1, false);

  int maxRSSI = -100;
  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice device = results.getDevice(i);
    if (device.getName().find("DeskBuddy:") != std::string::npos) {
      suspicion(0, 0, 75);
      int rssi = device.getRSSI();
      if (rssi > maxRSSI) {
        maxRSSI = rssi;
        lastInteractionTime = 0;
        pctNormal = min(pctNormal + 1, 100);
      }
    }
  }
}
