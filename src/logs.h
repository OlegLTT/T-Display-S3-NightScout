// Кольцевой буфер сообщений и вывод журнала в Serial и на экран.
#pragma once
#include "app_constants.h"
#include <Arduino.h>
#include <stdarg.h>
#include <time.h>
#include <cstring>

namespace logs {
// snprintf/strlcpy считают байты: удаляем только незавершённый символ в конце.
inline void trimUtf8Tail(char* text) {
  const size_t length = strlen(text);
  if (!length) return;
  size_t lead = length - 1;
  while (lead && (static_cast<uint8_t>(text[lead]) & 0xC0) == 0x80) --lead;
  const uint8_t first = static_cast<uint8_t>(text[lead]);
  const size_t expected = (first & 0xF8) == 0xF0 ? 4 : (first & 0xF0) == 0xE0 ? 3
      : (first & 0xE0) == 0xC0 ? 2 : 1;
  if (length - lead < expected) text[lead] = 0;
}
}

enum class LogLevel : uint8_t { Info, Warn, Error };

template <size_t CAP, size_t MSG_LEN>
class LogBufferT {
public:
  static constexpr size_t kCap    = CAP;
  static constexpr size_t kMsgLen = MSG_LEN;

  struct Entry {
    LogLevel level;
    uint32_t seq;

    uint32_t ts;

    bool     ts_is_real;

    char msg[MSG_LEN];
  };

  void addWithTs(LogLevel lvl, uint32_t ts, bool ts_is_real, const char* s) {
    portENTER_CRITICAL(&_lock);
    Entry& e = _buf[_head];
    e.level = lvl;
    e.seq   = _seq++;
    e.ts    = ts;
    e.ts_is_real = ts_is_real;

    strlcpy(e.msg, s ? s : "", MSG_LEN);
    logs::trimUtf8Tail(e.msg);

    _head = (_head + 1) % CAP;
    if (_count < CAP) _count++;
    _version++;
    portEXIT_CRITICAL(&_lock);
  }

  void add(LogLevel lvl, const char* s) {
    time_t now = time(nullptr);
    uint32_t ts;
    bool ts_is_real;

    if (now > app_constants::valid_epoch_seconds) {
      ts = (uint32_t)now;
      ts_is_real = true;
    } else {
      ts = (uint32_t)(millis() / 1000);
      ts_is_real = false;
    }

    addWithTs(lvl, ts, ts_is_real, s);
  }

  void add(LogLevel lvl, const String& s) { add(lvl, s.c_str()); }

  void addf(LogLevel lvl, const char* fmt, ...) {
    char tmp[MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt ? fmt : "", ap);
    va_end(ap);
    add(lvl, tmp);
  }

  size_t count() const {
    portENTER_CRITICAL(&_lock); const size_t result = _count; portEXIT_CRITICAL(&_lock); return result;
  }

  Entry getNewestFirst(size_t i) const {
    portENTER_CRITICAL(&_lock);
    if (i >= _count) { portEXIT_CRITICAL(&_lock); return Entry{LogLevel::Info, 0u, 0u, false, ""}; }
    int idx = (int)_head - 1 - (int)i;
    while (idx < 0) idx += (int)CAP;
    const Entry result = _buf[idx];
    portEXIT_CRITICAL(&_lock);
    return result;
  }

  uint32_t version() const {
    portENTER_CRITICAL(&_lock); const uint32_t result = _version; portEXIT_CRITICAL(&_lock); return result;
  }

private:
  mutable portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
  Entry _buf[CAP]{};
  size_t _head = 0;
  size_t _count = 0;
  uint32_t _version = 0;
  uint32_t _seq = 0;
};

using LogBuffer = LogBufferT<app_constants::log_capacity, app_constants::log_message_bytes>;
using LogEntry  = LogBuffer::Entry;

namespace logs {
  extern LogBuffer* g_buf;
  extern bool g_serial;

  void bind(LogBuffer* buf, bool serial = true);

  void logI(const char* s);
  void logW(const char* s);
  void logE(const char* s);

  void logIf(const char* fmt, ...);
  void logWf(const char* fmt, ...);
  void logEf(const char* fmt, ...);
}
