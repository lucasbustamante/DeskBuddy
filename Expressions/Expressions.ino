#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include "emotes.h"
#include "teste.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 2 // Pino ao qual o botão está conectado

#define MAX_INTERACTION_INTERVAL 280 // Tempo de interação, (1 = 5,4 sec. aproximadamente)

unsigned long lastInteractionTime = 0; // Variável para armazenar o tempo da última interação

void setup() {
  // Inicialize a comunicação com o display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Limpe o display
  display.clearDisplay();

  // Configure o pino do botão como entrada
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Inicialize o dispositivo BLE
  BLEDevice::init("ESP32_BEACON");

  // Configurar o dispositivo como um beacon
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(BLEUUID((uint16_t)0x1812));
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         BLEUUID((uint16_t)0x2A4E),
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pService->start();
  pServer->getAdvertising()->start();
  //Serial.println("Beacon iniciado...");
}

void loop() {
  lastInteractionTime++;

  // Verifica se passou muito tempo sem interação
  if (lastInteractionTime > MAX_INTERACTION_INTERVAL) {
    sad(0, 0, 75); // Atualiza o rosto para "sad"
  } else {
    normal(0, 0, 75); // Rosto normal
  }

  // Verifica se o botão foi pressionado
  if (digitalRead(BUTTON_PIN) != LOW) {
    lastInteractionTime = 0; // Atualiza o tempo da última interação
    happy(0, 0, 75);
    delay(50); // Pequeno atraso para evitar detecção múltipla do botão
  }

  // Escaneia beacons
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  BLEScanResults foundDevices = pBLEScan->start(1);

  int maxRSSI = -100; // Valor de RSSI mais baixo inicial

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    if (device.getName() == "ESP32_BEACON") {
      suspicion(0, 0, 75); // Longe
      int rssi = device.getRSSI();
      if (rssi > maxRSSI) {
        maxRSSI = rssi; // Atualiza o RSSI máximo encontrado
        lastInteractionTime = 0; // Atualiza o tempo da última interação
      }
    }
  }

  // Atualize o rosto na tela OLED com base na intensidade do sinal RSSI
  if (maxRSSI > -50) {
    loving(0, 0, 75); // Muito perto
  } else if (maxRSSI > -70) {
    happy(0, 0, 75); // Distância média
  } 
}