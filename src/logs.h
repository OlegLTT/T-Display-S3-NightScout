// ===== logs.h =====
#pragma once
#include <Arduino.h>
#include <stdarg.h>
#include <time.h>

enum class LogLevel : uint8_t { Info, Warn, Error };

template <size_t CAP, size_t MSG_LEN>
class LogBufferT {
public:
  static constexpr size_t kCap    = CAP;
  static constexpr size_t kMsgLen = MSG_LEN;

  struct Entry {
    LogLevel level;
    uint32_t seq;

    // seconds: epoch if ts_is_real, else uptime seconds
    uint32_t ts;
    // true=real time (epoch), false=uptime
    bool     ts_is_real;

    char msg[MSG_LEN];
  };

  // Добавление записи с уже посчитанным временем (чтобы не считать/форматировать 2 раза)
  void addWithTs(LogLevel lvl, uint32_t ts, bool ts_is_real, const char* s) {
    Entry& e = _buf[_head];
    e.level = lvl;
    e.seq   = _seq++;
    e.ts    = ts;
    e.ts_is_real = ts_is_real;

    strlcpy(e.msg, s ? s : "", MSG_LEN);

    _head = (_head + 1) % CAP;
    if (_count < CAP) _count++;
    _version++;
  }

  // Старый API оставляем: сам считает время и кладёт в буфер (можно использовать где-то ещё)
  void add(LogLevel lvl, const char* s) {
    time_t now = time(nullptr);
    uint32_t ts;
    bool ts_is_real;

    if (now > 100000) {
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

  size_t count() const { return _count; }

  Entry getNewestFirst(size_t i) const {
    if (i >= _count) return Entry{LogLevel::Info, 0u, 0u, false, ""};
    int idx = (int)_head - 1 - (int)i;
    while (idx < 0) idx += (int)CAP;
    return _buf[idx];
  }

  uint32_t version() const { return _version; }

private:
  Entry _buf[CAP]{};
  size_t _head = 0;
  size_t _count = 0;
  uint32_t _version = 0;
  uint32_t _seq = 0;
};

using LogBuffer = LogBufferT<100, 150>;
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
