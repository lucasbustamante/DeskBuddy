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

void suspicion(int xx, int yy, int tt) {
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes50, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes51, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes52, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes53, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes54, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes55, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes56, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes57, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes58, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes59, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes60, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes61, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes62, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes63, 128, 64, 1);
  display.display();
  delay(tt);
  for (int i = 0; i < 5; i++) {
    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes64, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes65, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes66, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes67, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes68, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes69, 128, 64, 1);
  display.display();
  delay(tt);

  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes70, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes71, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes72, 128, 64, 1);
  display.display();
  delay(tt);
}

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes73, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes74, 128, 64, 1);
  display.display();
  delay(tt);

}
/*
void loving (int xx, int yy, int tt) {
      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes77, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes78, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes79, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes80, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes81, 128, 64, 1);
  display.display();
  delay(tt);
  for (int i = 0; i < 5; i++) {
      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes82, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes83, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes84, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes85, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes86, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes87, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes88, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes89, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes90, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes91, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes92, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes93, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes94, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes95, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes96, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes97, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes98, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes99, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes100, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes101, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes102, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes103, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes104, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes105, 128, 64, 1);
  display.display();
  delay(tt);

        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes106, 128, 64, 1);
  display.display();
  delay(tt);
  }
        display.clearDisplay();
  display.drawBitmap(xx, yy, emotes107, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes108, 128, 64, 1);
  display.display();
  delay(tt);
  
      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes109, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes110, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes111, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes112, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes113, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes114, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes115, 128, 64, 1);
  display.display();
  delay(tt);
  }
  */

  void angry (int xx, int yy, int tt) {
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes133, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes134, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes135, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes136, 128, 64, 1);
  display.display();
  delay(tt);
  for (int i = 0; i < 15; i++) {
    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes137, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes138, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes139, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes140, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes141, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes142, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes143, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes144, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes145, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes146, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes147, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes148, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes149, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes150, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes151, 128, 64, 1);
  display.display();
  delay(tt);
  }
    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes152, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes153, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes154, 128, 64, 1);
  display.display();
  delay(tt);

    display.clearDisplay();
  display.drawBitmap(xx, yy, emotes155, 128, 64, 1);
  display.display();
  delay(tt);
  }


  void happy (int xx, int yy, int tt){
  display.clearDisplay();
  display.drawBitmap(xx, yy, emotes159, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes160, 128, 64, 1);
  display.display();
  delay(tt);
  for (int i = 0; i < 4; i++) {
      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes161, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes162, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes163, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes164, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes165, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes166, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes167, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes168, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes169, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes170, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes171, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes172, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes173, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes174, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes175, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes176, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes177, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes178, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes179, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes180, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes181, 128, 64, 1);
  display.display();
  delay(tt);
  }
      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes182, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes158, 128, 64, 1);
  display.display();
  delay(tt);

      display.clearDisplay();
  display.drawBitmap(xx, yy, emotes159, 128, 64, 1);
  display.display();
  delay(tt);
  }