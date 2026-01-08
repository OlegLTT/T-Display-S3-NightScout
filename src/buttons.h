#pragma once
#include <Arduino.h>

class Button {
public:
  using BtnCb = void (*)();

  // callbacks
  BtnCb onSingle = nullptr;
  BtnCb onDouble = nullptr;
  BtnCb onLong   = nullptr;
  BtnCb onRepeat = nullptr;

  // timings (defaults as requested)
  uint32_t debounceMs    = 40;
  uint32_t longPressMs   = 5000;
  uint32_t doubleClickMs = 350;
  uint32_t repeatDelayMs = 600;
  uint32_t repeatEveryMs = 250;

  void begin(uint8_t pin, bool activeLow = true, bool usePullup = true);

  // manual polling
  void update();
  void updateAt(uint32_t now);

private:
  struct State {
    uint8_t  pin = 255;
    bool     activeLow = true;

    bool     lastLevel = true;
    uint32_t lastChangeMs = 0;

    bool     pressed = false;
    uint32_t pressStartMs = 0;
    bool     longFired = false;

    uint32_t nextRepeatMs = 0;

    bool     clickPending = false;
    uint32_t clickDeadlineMs = 0;
  };

  State _s;

  inline bool isPressedLevel(bool level) const {
    return _s.activeLow ? (level == LOW) : (level == HIGH);
  }

  void fireSingleIfDue(uint32_t now);
  void handle(uint32_t now);
};

namespace Buttons {

bool attach(Button* b);
bool detach(Button* b);

// вместо beginTask/stopTask:
void tick();                 // вызови в loop()
//bool isTaskRunning();        // всегда false (для совместимости)

} // namespace Buttons
