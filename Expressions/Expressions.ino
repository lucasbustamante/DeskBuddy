#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include "emotes.h"
#include "teste.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define NAME "kizmo"
#define BUTTON_PIN 37  // Botão A do M5StickC Plus

#define MAX_INTERACTION_INTERVAL 100

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long lastInteractionTime = 0;

void setup() {
  // Configura pinos personalizados para I2C (OLED externo)
  Wire.begin(32, 33);

  // Inicializa o display
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
  display.display();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  BLEDevice::init(std::string("DeskBuddy: ") + NAME);

  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(BLEUUID((uint16_t)0x1812));
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
    BLEUUID((uint16_t)0x2A4E),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pService->start();
  pServer->getAdvertising()->start();

  Serial.begin(115200);
}

void loop() {
  if (lastInteractionTime <= MAX_INTERACTION_INTERVAL) {
    lastInteractionTime++;
  }

  if (lastInteractionTime > MAX_INTERACTION_INTERVAL) {
    sad(0, 0, 75);
  } else {
    normal(0, 0, 75);
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    lastInteractionTime = 0;
    happy(0, 0, 75);
    delay(50);
  }

  // BLE Scan
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  BLEScanResults results = pBLEScan->start(1, false);  // Agora não trava

  int maxRSSI = -100;

  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice device = results.getDevice(i);
    if (device.getName().find("DeskBuddy:") != std::string::npos) {
      suspicion(0, 0, 75);
      int rssi = device.getRSSI();
      if (rssi > maxRSSI) {
        maxRSSI = rssi;
        lastInteractionTime = 0;
      }
    }
  }

  // Exemplo de lógica com RSSI:
  // if (maxRSSI > -50) {
  //   loving(0, 0, 75);
  // } else if (maxRSSI > -70) {
  //   happy(0, 0, 75);
  // }
}
