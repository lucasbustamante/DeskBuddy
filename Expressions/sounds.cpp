#include "sounds.h"
#include <Arduino.h>

#define BUZZER_PIN 15 // Pino ao qual o buzzer está conectado

void sounds(int x, int y, int size) {
  // Produz um som grave e irregular para simular um robô bravo
  for (int i = 0; i < 100; i++) {
    int freq = random(100, 200); // Frequências baixas aleatórias
    tone(BUZZER_PIN, freq);
    delay(random(50, 200)); // Duração aleatória do som
    noTone(BUZZER_PIN);
    delay(random(10, 50)); // Pequena pausa entre os sons
  }
  
  // Pausa mais longa para dar a sensação de "respirar"

  // Adicione o código para atualizar o rosto para "suspeita" aqui
  // Exemplo: display.drawBitmap(x, y, suspicionBitmap, size, size, WHITE);
  // display.display();
}
