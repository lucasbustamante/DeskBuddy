#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "emotes.h" // Inclua o arquivo que contém o array emotes0
#include "teste.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  // Inicialize a comunicação com o display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Limpe o display
  display.clearDisplay();
}

void loop() {
  // Defina as coordenadas e o tempo de exibição
  int xx = 0;
  int yy = 0;
  int tt = 50; // Tempo em milissegundos

  // Exiba os emotes sequencialmente

sad(0, 0, 75);



}
