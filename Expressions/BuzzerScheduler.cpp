#include "BuzzerScheduler.h"

void BuzzerScheduler::begin(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, OUTPUT);
  stop();
}

void BuzzerScheduler::clearSteps() {
  _count = 0;
  _idx = 0;
  _active = false;
  _inGap = false;
}

void BuzzerScheduler::addTone(float freq, uint16_t durMs, uint16_t gapMs) {
  if (_count >= MAX_STEPS) return;
  uint16_t f = (freq <= 0.0f) ? 0 : (uint16_t)lroundf(freq);
  _steps[_count++] = { f, durMs, gapMs == 0 ? (uint16_t)1 : gapMs };
}

void BuzzerScheduler::addDelay(uint16_t ms) {
  // Represent delay as a silent step (freq=0) with duration=0 and gap=ms
  if (_count >= MAX_STEPS) return;
  _steps[_count++] = { 0, 0, ms == 0 ? (uint16_t)1 : ms };
}

void BuzzerScheduler::addBend(float initF, float finalF, float prop, uint16_t durMs, uint16_t gapMs) {
  if (prop <= 1.0f) prop = 1.01f;

  if (initF < finalF) {
    float f = initF;
    while (f < finalF && _count < MAX_STEPS) {
      addTone(f, durMs, gapMs);
      f *= prop;
    }
  } else {
    float f = initF;
    while (f > finalF && _count < MAX_STEPS) {
      addTone(f, durMs, gapMs);
      f /= prop;
    }
  }
}

void BuzzerScheduler::startCurrentStep() {
  if (_idx >= _count) { stop(); return; }

  Step &s = _steps[_idx];
  _t0 = millis();
  _inGap = false;

  if (s.freq > 0 && s.durMs > 0) {
    tone(_pin, s.freq); // start tone; we'll stop it ourselves
  } else {
    noTone(_pin);
    _inGap = true; // immediate gap
  }
  _active = true;
}

void BuzzerScheduler::playSound(uint8_t soundId) {
  clearSteps();

  switch (soundId) {
    case S_CONNECTION:
      addTone(NOTE_E5, 50, 30);
      addTone(NOTE_E6, 55, 25);
      addTone(NOTE_A6, 60, 10);
      break;

    case S_BUTTON_PUSHED:
      addBend(NOTE_E6, NOTE_G6, 1.03f, 20, 2);
      addDelay(30);
      addBend(NOTE_E6, NOTE_D7, 1.04f, 10, 2);
      break;

    case S_MODE3:
      addTone(NOTE_E6, 50, 100);
      addTone(NOTE_G6, 50, 80);
      addTone(NOTE_D7, 300, 1);
      break;

    case S_SURPRISE:
      addBend(800, 2150, 1.02f, 10, 1);
      addBend(2149, 800, 1.03f, 7, 1);
      break;

    case S_CUDDLY:
      addBend(700, 900, 1.03f, 16, 4);
      addBend(899, 650, 1.01f, 18, 7);
      break;

    case S_SLEEPING:
      addBend(100, 500, 1.04f, 10, 10);
      addDelay(500);
      addBend(400, 100, 1.04f, 10, 1);
      break;

    case S_HAPPY:
      addBend(1500, 2500, 1.05f, 20, 8);
      addBend(2499, 1500, 1.05f, 25, 8);
      break;

    case S_SUPER_HAPPY:
      addBend(2000, 6000, 1.05f, 8, 3);
      addDelay(50);
      addBend(5999, 2000, 1.05f, 13, 2);
      break;

    case S_HAPPY_SHORT:
      addBend(1500, 2000, 1.05f, 15, 8);
      addDelay(100);
      addBend(1900, 2500, 1.05f, 10, 8);
      break;

    case S_SAD:
      addBend(880, 669, 1.02f, 20, 200);
      break;

    case S_CONFUSED:
      addBend(1000, 1700, 1.03f, 8, 2);
      addBend(1699, 500, 1.04f, 8, 3);
      addBend(1000, 1700, 1.05f, 9, 10);
      break;

    default:
      // fallback: short blip
      addTone(1200, 40, 10);
      break;
  }

  if (_count == 0) { stop(); return; }
  _idx = 0;
  startCurrentStep();
}

void BuzzerScheduler::stop() {
  noTone(_pin);
  _active = false;
  _inGap = false;
  _count = 0;
  _idx = 0;
}

void BuzzerScheduler::update() {
  if (!_active || _idx >= _count) return;

  Step &s = _steps[_idx];
  uint32_t now = millis();

  if (!_inGap) {
    // tone phase
    if (s.durMs == 0 || (now - _t0) >= s.durMs) {
      noTone(_pin);
      _inGap = true;
      _t0 = now;
    }
  } else {
    // gap phase
    if ((now - _t0) >= s.gapMs) {
      _idx++;
      if (_idx >= _count) { stop(); return; }
      startCurrentStep();
    }
  }
}
