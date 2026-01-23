#pragma once
#include <Arduino.h>
#include "Sounds.h"

// Non-blocking scheduler for passive buzzer sounds (based on CuteBuzzerSounds patterns)
class BuzzerScheduler {
public:
  void begin(uint8_t pin);

  // Immediately starts a sound sequence (interrupts any current playback)
  void playSound(uint8_t soundId);

  // Stops playback
  void stop();

  // Must be called frequently from loop()
  void update();

  bool isPlaying() const { return _active; }

private:
  struct Step {
    uint16_t freq;   // Hz, 0 = silence
    uint16_t durMs;  // tone duration
    uint16_t gapMs;  // silence after tone
  };

  static const uint16_t MAX_STEPS = 256;
  Step _steps[MAX_STEPS];
  uint16_t _count = 0;
  uint16_t _idx = 0;

  uint8_t _pin = 255;
  bool _active = false;
  bool _inGap = false;
  uint32_t _t0 = 0;

  void clearSteps();
  void addTone(float freq, uint16_t durMs, uint16_t gapMs);
  void addDelay(uint16_t ms);
  void addBend(float initF, float finalF, float prop, uint16_t durMs, uint16_t gapMs);

  void startCurrentStep();
};
