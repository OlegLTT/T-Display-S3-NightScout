// logs.h
#pragma once
#include <Arduino.h>
#include <stdarg.h>

enum class LogLevel : uint8_t { Info, Warn, Error };

template <size_t CAP, size_t MSG_LEN>
class LogBufferT {
public:
  struct Entry {
    uint32_t ms;
    LogLevel level;
    char msg[MSG_LEN];
  };

  void add(LogLevel lvl, const char* s) {
    Entry& e = _buf[_head];
    e.ms = millis();
    e.level = lvl;
    strlcpy(e.msg, s ? s : "", MSG_LEN);
    _head = (_head + 1) % CAP;
    if (_count < CAP) _count++;
     _version++;          // <── увеличиваем «версию»
  }

  void add(LogLevel lvl, const String& s) {
    add(lvl, s.c_str());
  }

  void addf(LogLevel lvl, const char* fmt, ...) {
    char tmp[MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    add(lvl, tmp);
  }

  size_t count() const { return _count; }

  Entry getNewestFirst(size_t i) const {
    // i=0 самый новый
    if (i >= _count) return Entry{0, LogLevel::Info, ""};
    int idx = (int)_head - 1 - (int)i;
    while (idx < 0) idx += CAP;
    return _buf[idx];
  }
  
  uint32_t version() const {      // <── public accessor
    return _version;
  }

private:
  Entry _buf[CAP]{};
  size_t _head = 0;
  size_t _count = 0;
  uint32_t _version = 0; 
};

// --- Backward compatibility layer ---
// Если ваш основной буфер именно такой:
using LogBuffer = LogBufferT<25, 96>;

// Чтобы старый код screens.cpp мог писать LogEntry e = ...
using LogEntry = LogBuffer::Entry;
