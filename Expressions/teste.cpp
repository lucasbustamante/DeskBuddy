// teste.cpp

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "emotes.h"
#include "teste.h"

extern Adafruit_SSD1306 display;

void normal(int xx, int yy, int tt) {

  for (int i = 0; i < 15; i++) {
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes0, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes1, 128, 64, 1);
  display.display();
  delay(tt);
}

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes2, 128, 64, 1);
  display.display();
  delay(tt);


  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes3, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes4, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes5, 128, 64, 1);
  display.display();
  delay(tt);


  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes6, 128, 64, 1);
  display.display();
  delay(tt);


  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes7, 128, 64, 1);
  display.display();
  delay(tt);
  
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes9, 128, 64, 1);
  display.display();
  delay(tt);
  
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes10, 128, 64, 1);
  display.display();
  delay(tt);
  
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes11, 128, 64, 1);
  display.display();
  delay(tt);
  
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes12, 128, 64, 1);
  display.display();
  delay(tt);
}

void sad(int xx, int yy, int tt) {

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes41, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes42, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes43, 128, 64, 1);
  display.display();
  delay(tt);


  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes44, 128, 64, 1);
  display.display();
  delay(tt);


  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes45, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes46, 128, 64, 1);
  display.display();
  delay(tt);

  for (int i = 0; i < 30; i++) {
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes47, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes48, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes49, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes48, 128, 64, 1);
  display.display();
  delay(tt);
  }

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes47, 128, 64, 1);
  display.display();
  delay(tt);
  

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes46, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes45, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes44, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes43, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes42, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes41, 128, 64, 1);
  display.display();
  delay(tt);
}