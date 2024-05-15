#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "emotes.h" // Inclua o arquivo que contém o array emotes0

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
display.clearDisplay();
display.drawBitmap(xx, yy,emotes0,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes1,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes2,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes3,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes4,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes5,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes6,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes7,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes8,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes9,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes10,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes11,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes12,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes13,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes14,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes15,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes16,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes17,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes18,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes19,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes20,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes21,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes22,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes23,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes24,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes25,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes26,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes27,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes28,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes29,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes30,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes31,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes32,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes33,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes34,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes35,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes36,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes37,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes38,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes39,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes40,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes41,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes42,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes43,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes44,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes45,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes46,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes47,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes48,128,64, 1);
display.display();
delay(tt);

display.clearDisplay();
display.drawBitmap(xx, yy,emotes49,128,64, 1);
display.display();
delay(tt);



}
