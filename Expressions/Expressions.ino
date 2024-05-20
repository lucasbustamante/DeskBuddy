#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "emotes.h"
#include "teste.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 2 // Pino ao qual o botão está conectado

void setup() {
  // Inicialize a comunicação com o display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Limpe o display
  display.clearDisplay();
  
  // Configure o pino do botão como entrada
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // Verifique o estado do botão
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState != LOW) {
    // Botão pressionado
    sad(0, 0, 75);
  } else {
    // Botão não pressionado
    normal(0, 0, 75);
  }

  // Pequeno atraso para debouncing do botão
  delay(50);
}
