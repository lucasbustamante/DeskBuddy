#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include "emotes.h"
#include "teste.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define NAME "Kizmo"
#define OLED_RESET 0  
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 2
#define MAX_INTERACTION_INTERVAL 10  // (1 = aproximadamente 5,4 segundos)

// Timeout para considerar que não há mais detecção BLE (em milissegundos)
const unsigned long BLE_DETECTION_TIMEOUT = 2000;

volatile unsigned long lastInteractionTime = 0;  // Contador de interação
volatile int globalMaxRSSI = -100;
volatile unsigned long bleLastDetectedTime = 0;  // Timestamp da última detecção BLE
BLEScan* pBLEScan;

// Tarefa que realiza a varredura BLE de forma assíncrona
void scanBLETask(void * parameter) {
  while (true) {
    BLEScanResults* foundDevices = pBLEScan->start(1, false);
    bool detected = false;
    for (int i = 0; i < foundDevices->getCount(); i++) {
      BLEAdvertisedDevice device = foundDevices->getDevice(i);
      if (device.getName().indexOf("DeskBuddy:") != -1) {
        detected = true;
        int rssi = device.getRSSI();
        if (rssi > globalMaxRSSI) {
          globalMaxRSSI = rssi;
          lastInteractionTime = 0;  // Reinicia o contador quando há uma detecção forte
        }
      }
    }
    if (detected) {
      bleLastDetectedTime = millis();  // Atualiza o timestamp se algum dispositivo for detectado
    }
    pBLEScan->clearResults();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // Inicializa o display OLED e exibe o nome por 5 segundos
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
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
  
  // Configura o pino do botão (utilizando INPUT_PULLUP, ligue o botão entre o pino e o GND)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Inicializa o BLE e configura o dispositivo como beacon
  BLEDevice::init(String("DeskBuddy: ") + NAME);
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(BLEUUID((uint16_t)0x1812));
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         BLEUUID((uint16_t)0x2A4E),
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pService->start();
  pServer->getAdvertising()->start();
  
  // Configura o scanner BLE
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  
  // Cria a tarefa de scanner BLE na core 1 para não bloquear o loop principal
  xTaskCreatePinnedToCore(scanBLETask, "scanBLETask", 10000, NULL, 1, NULL, 1);
}

void loop() {
  // Incrementa o contador de interação (a cada ciclo, por simplicidade)
  if (lastInteractionTime <= MAX_INTERACTION_INTERVAL) {
    lastInteractionTime++;
  }
  
  // Verifica se o botão foi pressionado
  if (digitalRead(BUTTON_PIN) != LOW) {
    lastInteractionTime = 0;
    display.clearDisplay();
    happy(0, 0, 75);  // Exibe a expressão happy
    display.display();
    delay(50);  // Pequeno atraso para evitar múltiplas leituras
    return;      // Prioriza a ação do botão
  }
  
  // Se um dispositivo BLE foi detectado recentemente, exibe a expressão suspicious
  if (millis() - bleLastDetectedTime < BLE_DETECTION_TIMEOUT) {
    display.clearDisplay();
    suspicion(0, 0, 75);
    display.display();
        lastInteractionTime = 0; // Atualiza o tempo da última interação
      
  } else {
    // Caso contrário, utiliza o contador de interação:
    // Se passou muito tempo sem interação, exibe sad; senão, exibe normal.
    display.clearDisplay();
    if (lastInteractionTime > MAX_INTERACTION_INTERVAL) {
      sad(0, 0, 75);
    } else {
      normal(0, 0, 75);
    }
    display.display();
  }
  
  delay(100);  // Delay para evitar atualizações excessivamente rápidas
}
