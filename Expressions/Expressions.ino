#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <EEPROM.h>

// Definições
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
#define BUTTON_PIN 26
#define EEPROM_SIZE 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Emoções (valores iniciais)
int pctNormal = 50, pctFeliz = 30, pctTriste = 10, pctEntediado = 10, pctBravo = 0, pctApaixonado = 0;

// BLE Service/Characteristic UUIDs (igual ao app)
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristic;

// BLE callback para reconectar sempre que desconectar
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE Conectado");
  }
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE Desconectado");
    delay(200);
    pServer->getAdvertising()->start(); // Volta a anunciar
  }
};

// EEPROM Helpers
void loadHumorFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  pctNormal     = EEPROM.read(0);
  pctFeliz      = EEPROM.read(1);
  pctTriste     = EEPROM.read(2);
  pctEntediado  = EEPROM.read(3);
  pctBravo      = EEPROM.read(4);
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

// Função que retorna JSON do humor
String getHumorJSON() {
  String json = "{";
  json += "\"normal\":"     + String(pctNormal)     + ",";
  json += "\"feliz\":"      + String(pctFeliz)      + ",";
  json += "\"triste\":"     + String(pctTriste)     + ",";
  json += "\"entediado\":"  + String(pctEntediado)  + ",";
  json += "\"bravo\":"      + String(pctBravo)      + ",";
  json += "\"apaixonado\":" + String(pctApaixonado);
  json += "}";
  return json;
}

// Mostra no display
void showEmotionsOnDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("DeskBuddy");
  display.println("Humor:");
  display.print("Normal: ");     display.println(pctNormal);
  display.print("Feliz: ");      display.println(pctFeliz);
  display.print("Triste: ");     display.println(pctTriste);
  display.print("Entediado: ");  display.println(pctEntediado);
  display.print("Bravo: ");      display.println(pctBravo);
  display.print("Apaixonado: "); display.println(pctApaixonado);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(32, 33); // Ajuste se necessário para seu display
  EEPROM.begin(EEPROM_SIZE);

  // Display Setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Erro ao inicializar o display OLED"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("DeskBuddy");
  display.display();
  delay(2000);
  display.clearDisplay();

  // Botão
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // BLE Setup
  BLEDevice::init("DeskBuddy");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
  );

  // Inicializa a characteristic com o humor atual
  loadHumorFromEEPROM();
  pCharacteristic->setValue(getHumorJSON().c_str());

  pService->start();
  pServer->getAdvertising()->start();

  showEmotionsOnDisplay();
}

unsigned long lastButtonTime = 0;
void loop() {
  // Atualiza humor pelo botão (exemplo: aumenta "feliz")
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonTime > 500)) {
    lastButtonTime = millis();
    pctFeliz = min(pctFeliz + 5, 100);
    pctNormal = max(pctNormal - 2, 0);
    saveHumorToEEPROM();
    pCharacteristic->setValue(getHumorJSON().c_str());
    showEmotionsOnDisplay();
  }

  // Simulação: a cada 15s muda humor (exemplo)
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 15000) {
    lastUpdate = millis();
    pctNormal = max(0, pctNormal - 1);
    pctTriste = min(100, pctTriste + 1);
    saveHumorToEEPROM();
    pCharacteristic->setValue(getHumorJSON().c_str());
    showEmotionsOnDisplay();
  }
}
