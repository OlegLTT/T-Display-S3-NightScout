// Кольцевой буфер сообщений и вывод журнала в Serial и на экран.
#include "logs.h"

namespace logs {

LogBuffer* g_buf = nullptr;
bool g_serial = true;

static void fmt_ts(char* out, size_t out_len, uint32_t ts, bool ts_is_real) {
  if (!out || out_len == 0) return;

  if (ts_is_real) {
    time_t t = (time_t)ts;
    struct tm tm{};
    localtime_r(&t, &tm);
    snprintf(out, out_len, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else {
    uint32_t s = ts;
    uint32_t hh = s / 3600;
    uint32_t mm = (s / 60) % 60;
    uint32_t ss = s % 60;
    snprintf(out, out_len, "%02lu:%02lu:%02lu",
             (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
  }
}

static char lvl_char(LogLevel lvl) {
  switch (lvl) {
    case LogLevel::Warn:  return 'W';
    case LogLevel::Error: return 'E';
    default:              return 'I';
  }
}

static void add(LogLevel lvl, const char* s) {
  const char* msg = s ? s : "";

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

  char tbuf[16];
  fmt_ts(tbuf, sizeof(tbuf), ts, ts_is_real);

  char serial_line[LogBuffer::kMsgLen];
  snprintf(serial_line, sizeof(serial_line), "%s %c %s", tbuf, lvl_char(lvl), msg);

  char buf_line[LogBuffer::kMsgLen];
  snprintf(buf_line, sizeof(buf_line), "%s %s", tbuf, msg);

  if (g_serial) {
    Serial.println(serial_line);
  }

  if (g_buf) {
    g_buf->addWithTs(lvl, ts, ts_is_real, buf_line);
  }
}

static void addf(LogLevel lvl, const char* fmt, va_list ap) {
  char tmp[LogBuffer::kMsgLen];
  vsnprintf(tmp, sizeof(tmp), fmt ? fmt : "", ap);
  add(lvl, tmp);
}

void bind(LogBuffer* buf, bool serial) {
  g_buf = buf;
  g_serial = serial;
}

void logI(const char* s) { add(LogLevel::Info, s); }
void logW(const char* s) { add(LogLevel::Warn, s); }
void logE(const char* s) { add(LogLevel::Error, s); }

void logIf(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  addf(LogLevel::Info, fmt, ap);
  va_end(ap);
}

void logWf(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  addf(LogLevel::Warn, fmt, ap);
  va_end(ap);
}

void logEf(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  addf(LogLevel::Error, fmt, ap);
  va_end(ap);
}

}
