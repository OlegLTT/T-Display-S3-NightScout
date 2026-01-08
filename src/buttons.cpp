#include "buttons.h"

// ---------- Button ----------

void Button::begin(uint8_t pin, bool activeLow, bool usePullup) {
  _s.pin = pin;
  _s.activeLow = activeLow;

  if (usePullup && activeLow) {
    pinMode(pin, INPUT_PULLUP);
  } else {
    pinMode(pin, INPUT);
  }

  _s.lastLevel = digitalRead(pin);
  _s.lastChangeMs = millis();

  _s.pressed = false;
  _s.pressStartMs = 0;
  _s.longFired = false;
  _s.nextRepeatMs = 0;

  _s.clickPending = false;
  _s.clickDeadlineMs = 0;
}

void Button::fireSingleIfDue(uint32_t now) {
  if (_s.clickPending && now >= _s.clickDeadlineMs) {
    _s.clickPending = false;
    if (onSingle) onSingle();
  }
}

void Button::handle(uint32_t now) {
  if (_s.pin == 255) return;

  bool level = digitalRead(_s.pin);

  // debounce
  if (level != _s.lastLevel) {
    _s.lastLevel = level;
    _s.lastChangeMs = now;
    return;
  }
  if (now - _s.lastChangeMs < debounceMs) return;

  fireSingleIfDue(now);

  bool down = isPressedLevel(level);

  // press
  if (down && !_s.pressed) {
    _s.pressed = true;
    _s.pressStartMs = now;
    _s.longFired = false;
    _s.nextRepeatMs = onRepeat ? (now + repeatDelayMs) : 0;
    return;
  }

  // hold
  if (down && _s.pressed) {
    if (!_s.longFired && onLong && now - _s.pressStartMs >= longPressMs) {
      _s.longFired = true;
      onLong();
    }
    if (onRepeat && _s.nextRepeatMs && now >= _s.nextRepeatMs) {
      onRepeat();
      _s.nextRepeatMs = now + repeatEveryMs;
    }
    return;
  }

  // release
  if (!down && _s.pressed) {
    _s.pressed = false;
    _s.nextRepeatMs = 0;

    if (_s.longFired) {
      _s.longFired = false;
      _s.clickPending = false;
      return;
    }

    if (!onDouble || doubleClickMs == 0) {
      if (onSingle) onSingle();
      return;
    }

    if (!_s.clickPending) {
      _s.clickPending = true;
      _s.clickDeadlineMs = now + doubleClickMs;
    } else {
      _s.clickPending = false;
      if (onDouble) onDouble();
    }
  }
}

void Button::updateAt(uint32_t now) {
  handle(now);
  fireSingleIfDue(now);
}

void Button::update() {
  updateAt(millis());
}

// ---------- Buttons (manual polling) ----------

namespace Buttons {

static constexpr size_t MAX_BTNS = 8;
static Button* g_btns[MAX_BTNS] = {nullptr};

static void tickAll(uint32_t now) {
  for (auto& b : g_btns) {
    if (b) b->updateAt(now);
  }
}

void tick() {
  tickAll(millis());
}

bool attach(Button* b) {
  if (!b) return false;
  for (auto& slot : g_btns) {
    if (slot == b) return true;
  }
  for (auto& slot : g_btns) {
    if (!slot) {
      slot = b;
      return true;
    }
  }
  return false;
}

bool detach(Button* b) {
  for (auto& slot : g_btns) {
    if (slot == b) {
      slot = nullptr;
      return true;
    }
  }
  return false;
}

// bool isTaskRunning() {
//   return false;
// }

} // namespace Buttons

