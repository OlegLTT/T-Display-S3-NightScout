"""Generate dependency-free SVG guides to the firmware UI (schematic examples).

Run from any directory: python scripts/generate_readme_diagrams.py
Layout references: src/ui_main.cpp, ui_clock.cpp, ui_settings.cpp, ui_logs.cpp,
ui_navigation.cpp, ui_layout.h and captive_portal.cpp. These are drawings,
not screenshots; no firmware source or user settings are changed.
"""
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/images/screens"
INK, MUTED, BLUE = "#172b43", "#52677e", "#176bce"
GREEN, YELLOW, RED, CYAN, GREY = "#00ff00", "#ffff00", "#ff0000", "#71ffff", "#c8c8c8"


def text(x, y, value, size=18, color=INK, weight=400, anchor="start", family="Arial, sans-serif"):
    return (f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}" '
            f'font-weight="{weight}" text-anchor="{anchor}" font-family="{family}">{escape(str(value))}</text>')


def rect(x, y, w, h, fill, radius=0, stroke="none", sw=1):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'


def path(d, color, width=2, fill="none"):
    return f'<path d="{d}" fill="{fill}" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>'


def dot(x, y, color, r=3):
    return f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>'


def badge(n, x, y):
    return dot(x, y, BLUE, 13) + text(x, y + 5, n, 15, "white", 700, "middle")


def pin(n, x, y, tx, ty):
    return path(f'M{x} {y} L{tx} {ty}', BLUE, 1.5) + dot(tx, ty, BLUE) + badge(n, x, y)


def group(body, x=0, y=0, scale=1):
    return f'<g transform="translate({x} {y}) scale({scale})">{body}</g>'


def save(name, title, subtitle, body, height=760):
    OUT.mkdir(parents=True, exist_ok=True)
    doc = (f'<svg xmlns="http://www.w3.org/2000/svg" width="1160" height="{height}" viewBox="0 0 1160 {height}" role="img" aria-labelledby="title desc">'
           f'<title id="title">{escape(title)}</title><desc id="desc">{escape(subtitle)}</desc>'
           + rect(0, 0, 1160, height, "#f4f7fb", 20)
           + text(36, 44, title, 28, weight=700)
           + text(36, 75, subtitle, 17, MUTED)
           + body + text(36, height - 20, "Nightscout Monitor · Схема интерфейса · Данные для примера", 14, MUTED)
           + '</svg>\n')
    (OUT / name).write_text(doc, encoding="utf-8")


def speaker(x, y, color, muted=False):
    body = path('M0 10 H6 L14 3 V27 L6 20 H0 Z', color, 2, color)
    if muted:
        body += path('M-2 28 L27 0', color, 2.5)
    else:
        body += path('M19 8 Q28 15 19 22 M23 3 Q37 15 23 27', color, 2)
    return group(body, x, y)


def arrow(x, y, color=CYAN, angle=-45, scale=1):
    return (f'<g transform="translate({x} {y}) rotate({angle}) scale({scale})">'
            + path('M-25 0 H25 M12 -13 L25 0 L12 13', color, 7) + '</g>')


def alarm(core, height, state="normal"):
    states = {
        "normal": (GREEN, "Звук включён"),
        "critical": (RED, "Тревога!"),
        "snooze": (RED, "Отсрочка 30 мин | 29:42"),
        "old": ("#ffd166", "Нет свежих данных"),
        "missing": (RED, "Нет данных 15+ мин"),
        "error": (RED, "Ошибка звука"),
    }
    color, label = states[state]
    if core:
        return rect(0, height - 44, 320, 44, "#101010") + text(160, height - 17, label, 18, color, anchor="middle")
    color = "#999999" if state == "old" else "#ffd166" if state == "missing" else color
    if state == "error":
        return text(22, height / 2 + 8, "!", 30, RED, 700, "middle")
    body = speaker(9, height / 2 - 15, color, state == "snooze")
    if state == "snooze":
        body += text(24, height / 2 + 35, "29:42", 13, RED, anchor="middle")
    return body


def main_screen(core=False, glucose="5.8", color=GREEN, state="normal", age="2 мин", delta="+0.1"):
    height = 240 if core else 170
    content = height - 44 if core else height
    body = rect(0, 0, 320, height, "#000")
    body += rect(2, 4, 23, 11, "none", 2, GREEN, 1.6) + rect(25, 7, 3, 5, GREEN)
    body += rect(4, 6, 16, 7, GREEN) + text(31, 17, "78%", 17, GREEN)
    body += text(160, 17, "12:45", 17, GREY, anchor="middle")
    body += text(318, 17, age, 17, GREEN, anchor="end")
    body += text(130 if core else 140, content / 2 + 47, glucose, 128 if core else 116, color, 600, "middle", "Arial Narrow, Arial, sans-serif")
    body += text(281, content - 76, delta, 32, GREEN if delta.startswith('+') else RED if delta != "--" else "#787878", anchor="middle")
    if glucose != "--":
        body += arrow(280, content - 38, CYAN, 45 if delta.startswith('-') else -45)
    body += text(1, content - 3, "I: --" if glucose == "--" else "I: 1.25", 17, GREY)
    body += text(120, content - 3, "C: --" if glucose == "--" else "C: 20", 17, GREY)
    return body + alarm(core, height, state)


def clock_screen(core=False):
    height, content = (240, 196) if core else (170, 170)
    body = rect(0, 0, 320, height, "#000")
    body += text(6, 21, "5.8", 18, GREEN) + text(119, 21, "+0.1", 18, GREEN)
    body += arrow(195, 14, GREEN, -45, .36)
    body += text(314, 21, "2 мин", 18, GREEN, anchor="end")
    body += text(160 if core else 184, content / 2 + 31, "12:45", 90 if core else 82, GREEN, 700, "middle", "Arial Narrow, Arial, sans-serif")
    body += text(5, content - 4, "29 октября 2026", 16, GREY)
    body += text(319, content - 4, "четверг", 16, GREY, anchor="end")
    return body + alarm(core, height)


def qr(x, y, size):
    """Illustrative QR pattern, intentionally not a connection credential."""
    body = rect(0, 0, 29, 29, "#fff")
    modules = []
    for row in range(2, 27):
        for col in range(2, 27):
            if ((col < 10 and row < 10) or (col > 18 and row < 10) or (col < 10 and row > 18)):
                continue
            if ((col * 13 + row * 7 + col * row) % 11) < 5:
                modules.append(f'M{col} {row}h1v1h-1z')
    body += f'<path d="{" ".join(modules)}" fill="#000"/>'
    for col, row in [(2, 2), (20, 2), (2, 20)]:
        body += rect(col, row, 7, 7, "#000") + rect(col + 1, row + 1, 5, 5, "#fff") + rect(col + 2, row + 2, 3, 3, "#000")
    return group(body, x, y, size / 29)


def settings_screen(core=False):
    height = 240 if core else 170
    body = rect(0, 0, 320, height, "#000")
    body += text(160, 18, "ИНСТРУКЦИЯ ПО НАСТРОЙКЕ" if core else "НАСТРОЙКИ", 16 if core else 18, RED, anchor="middle")
    body += qr(6, 24, 104)
    body += text(116, 39, "1) Подключитесь к Wi-Fi:", 14, "#ccff33")
    body += text(116, 57, "SSID:", 14, "#ff9900") + text(166, 57, "NS-Monitior-Setup", 13, "#66ccff")
    body += text(116, 75, "PASS:", 14, "#ff9900") + text(166, 75, "DEMO1234", 14, "#66ccff")
    body += qr(250, height - 70, 64)
    if core:
        body += text(6, 178, "2) Если портал не открылся,", 14, "#ccff33")
        body += text(6, 196, "перейдите на", 14, "#ccff33")
        body += text(6, 214, "http://192.168.4.1/", 14, "#66ccff")
    else:
        body += text(238, 141, "2) Откройте в браузере:", 14, "#ccff33", anchor="end")
        body += text(238, 159, "http://192.168.4.1/", 14, "#66ccff", anchor="end")
    return body


def logs_screen(core=False):
    height = 240 if core else 170
    body = rect(0, 0, 320, height, "#000")
    lines = [
        (GREEN, "12:44:01 [BOOT] Starting"),
        (GREEN, "12:44:02 [WiFi] Connecting..."),
        (GREEN, "12:44:04 [WiFi] Connected"),
        (GREEN, "12:44:05 [NTP] Time synced"),
        (GREEN, "12:44:06 Nightscout status OK"),
        (YELLOW, "12:44:07 [NS] Request timeout"),
        (RED, "12:44:08 [NS] HTTP error"),
        (GREEN, "12:45:01 [NS] Glucose received"),
        (GREEN, "12:45:03 [SNOOZE] 30 min"),
    ]
    if core:
        lines += [(GREEN, "12:45:04 [SNOOZE LAN] Received"), (GREEN, "12:45:05 [WiFi] Connected")]
    for index, (color, line) in enumerate(lines):
        body += text(0, 12 + index * 14, line, 11.5, color, family="Consolas, monospace")
    body += rect(316, 0, 4, height, "#222") + rect(316, height * .3, 4, height * .45, "#999", 2)
    return body


def device(core, body):
    sx, sy, scale = (692, 190, 1.2) if core else (110, 190, 1.2)
    if core:
        frame = rect(666, 158, 436, 352, "#233142", 22) + rect(676, 166, 416, 334, "#121a25", 16)
        frame += dot(748, 177, "#53687c", 4) + dot(764, 177, "#53687c", 2)
        frame += text(884, 498, "M5STACK", 11, "#91a1b3", anchor="middle")
    else:
        frame = rect(46, 164, 482, 256, "#233142", 20) + rect(55, 173, 464, 238, "#121a25", 13)
        frame += dot(80, 221, "#9caabb", 14) + dot(80, 359, "#9caabb", 14)
        frame += rect(53, 267, 6, 42, "#657a8f", 2)
    return frame + group(body, sx, sy, scale)


def legend(items, y=578):
    body = ""
    for i, (n, label) in enumerate(items):
        x = 52 if i % 2 == 0 else 610
        line_y = y + (i // 2) * 32
        body += badge(n, x, line_y - 5) + text(x + 23, line_y, label, 18)
    return body


def pair(name, title, subtitle, render, pins, items, note=""):
    body = text(52, 117, "LilyGO T-Display S3 · 320 × 170", 19, weight=700)
    body += text(650, 117, "M5Stack CoreS3 / SE · 320 × 240", 19, weight=700)
    for core in (False, True):
        body += device(core, render(core))
        sx = 692 if core else 110
        for n, px, py, tx, ty in pins(core):
            body += pin(n, px, py, sx + tx * 1.2, 190 + ty * 1.2)
    body += legend(items)
    if note:
        body += text(36, 714, note, 16, MUTED)
    save(name, title, subtitle, body)


def main():
    def main_pins(core):
        if core:
            return [(1, 645, 340, 100, 104), (2, 1126, 390, 281, 157), (3, 1126, 304, 282, 105),
                    (4, 1060, 147, 288, 14), (5, 884, 147, 160, 14), (6, 728, 147, 35, 14),
                    (7, 728, 539, 38, 190), (8, 885, 539, 147, 190), (9, 1060, 539, 234, 218)]
        return [(1, 286, 462, 130, 104), (2, 558, 386, 280, 133), (3, 558, 285, 280, 80),
                (4, 472, 147, 290, 14), (5, 302, 147, 160, 14), (6, 148, 147, 35, 14),
                (7, 136, 462, 37, 164), (8, 425, 462, 146, 164), (9, 31, 285, 22, 85)]

    pair("main.svg", "Основной экран · что означают элементы", "Одинаковые номера обозначают одинаковые показания на обеих платах.",
         main_screen, main_pins, [(1, "Глюкоза, ммоль/л; цвет зависит от зоны"), (2, "Стрелка направления и скорости изменения"),
                                 (3, "Дельта к предыдущему измерению"), (4, "Возраст последнего измерения"),
                                 (5, "Текущее местное время"), (6, "Заряд выбранного источника, %"),
                                 (7, "I: IOB — активный инсулин"), (8, "C: COB — активные углеводы"),
                                 (9, "Состояние звука / паузы тревог")])

    def clock_pins(core):
        sx = 692 if core else 110
        bottom = 539 if core else 462
        return [(1, sx + 40, 147, 25, 15), (2, sx + 190, 147, 155, 15),
                (3, sx + 350, 147, 291, 15), (4, sx + 170, bottom, 177, 91),
                (5, sx + 18, bottom, 68, 190 if core else 163),
                (6, sx + 360, bottom, 277, 190 if core else 163),
                (7, 1126 if core else 31, 350 if core else 285, 281 if core else 22, 218 if core else 85)]

    pair("clock.svg", "Часы · время и краткие показания", "Цвет времени, глюкозы, дельты и стрелок соответствует зоне глюкозы.",
         clock_screen, clock_pins, [(1, "Глюкоза, ммоль/л"), (2, "Дельта и стрелка тренда"),
                                   (3, "Возраст измерения"), (4, "Текущее время крупными цифрами"),
                                   (5, "Дата"), (6, "День недели"), (7, "Индикатор звука / кнопка Snooze")],
         "С 6 полных минут цвет показаний приглушается; дата и день недели остаются светло-серыми.")

    def setup_pins(core):
        sx = 692 if core else 110
        return [(1, sx - 38, 327, 58, 76), (2, sx + 365, 147, 220, 56),
                (3, sx + 365, 539 if core else 462, 282, 205 if core else 135),
                (4, sx + 110, 539 if core else 462, 117, 214 if core else 156)]

    pair("setup.svg", "Окно настройки · подключение телефона", "Здесь показана инструкция. Параметры вводятся в браузере телефона или компьютера.",
         settings_screen, setup_pins, [(1, "Большой QR: подключение к Wi-Fi устройства"), (2, "SSID и пароль точки доступа"),
                                     (3, "Малый QR: адрес веб-настроек"), (4, "Адрес для ручного ввода в браузере")],
         "QR-коды и пароль на рисунке условные. Используйте коды и пароль с экрана своего устройства.")

    def logs_pins(core):
        sx = 692 if core else 110
        return [(1, sx + 34, 147, 34, 8), (2, sx - 34, 307, 149, 78),
                (3, sx + 350, 147, 190, 36), (4, sx + 350, 539 if core else 462, 318, 125)]

    pair("logs.svg", "Журнал · запуск, сеть и сообщения об ошибках", "Журнал занимает всю высоту экрана. Индикатор звука и кнопка Snooze скрыты.",
         logs_screen, logs_pins, [(1, "Время записи события"), (2, "Уровень сообщения обозначен цветом"),
                                 (3, "Источник и текст сообщения"), (4, "Полоса прокрутки справа")],
         "Зелёный — информация; жёлтый — предупреждение; красный — ошибка. Тексты сообщений — примеры.")
    states()
    web_settings()
    print(f"Generated 6 SVG diagrams in {OUT.relative_to(ROOT)}")


def states():
    examples = [
        ("Целевая зона", "5.8", GREEN, "normal", "2 мин", "+0.1", "Свежие данные, звук готов к тревоге"),
        ("Жёлтая зона", "8.2", YELLOW, "normal", "2 мин", "+0.1", "Цвет предупреждает; звуковой тревоги нет"),
        ("Красная зона", "3.2", RED, "critical", "2 мин", "-0.2", "Свежее измерение, звуковая тревога"),
        ("Snooze · пауза тревог", "3.2", RED, "snooze", "2 мин", "-0.2", "Тревожный цвет остаётся; звук на паузе"),
        ("Нет свежих данных", "5.8", GREEN, "old", "11 мин", "+0.1", "Последнее показание ещё видно"),
        ("Данные устарели", "--", "#787878", "missing", "16 мин", "--", "Показания скрыты; тревога потери данных"),
    ]
    body = ""
    for i, (label, glucose, color, state, age, delta, caption) in enumerate(examples):
        x, y = 30 + (i % 2) * 565, 103 + (i // 2) * 312
        body += rect(x, y, 545, 296, "#fff", 14, "#dae3ee")
        body += badge(i + 1, x + 27, y + 28) + text(x + 50, y + 34, label, 21, weight=700)
        body += group(main_screen(False, glucose, color, state, age, delta), x + 110, y + 48, .95)
        # CoreS3's same state, shown as its distinctive bottom touch bar.
        body += text(x + 17, y + 235, "CoreS3:", 14, MUTED)
        body += group(alarm(True, 44, state), x + 110, y + 212, .95)
        body += text(x + 17, y + 280, caption, 16, MUTED)
    body += text(36, 1066, "Пример границ: 3.5 / 4.5 / 7.5 / 9.5 ммоль/л. Ваши границы загружаются из Nightscout.", 17, MUTED)
    save("states.svg", "Режимы индикации · цвет, тревога и пауза", "В каждом примере: экран T-Display и соответствующая нижняя кнопка CoreS3.", body, 1110)


def web_settings():
    body = rect(58, 108, 670, 925, "#fff", 16, "#dae3ee")
    body += rect(58, 108, 670, 40, "#e5ebf2", 16) + text(90, 134, "192.168.4.1", 16, MUTED)
    body += text(84, 184, "Настройки устройства", 26, weight=700)
    body += text(84, 210, "Параметры сохраняются на этом устройстве", 15, MUTED)

    def field(y, label, value):
        return text(100, y, label, 16) + rect(100, y + 10, 585, 36, "#fff", 7, "#b9c6d6") + text(112, y + 34, value, 17)

    body += rect(82, 229, 621, 174, "#f4f6f8", 10)
    body += field(254, "Адрес сервера Nightscout", "ns.example.org")
    body += field(332, "Токен доступа (если требуется)", "••••••••••••••••")
    body += rect(82, 416, 621, 149, "#f4f6f8", 10)
    body += text(100, 445, "Wi-Fi · приоритет сверху вниз", 18, weight=700)
    body += field(478, "Сеть 1 · SSID и пароль", "Home Wi-Fi      ••••••••          ↑  ↓")
    body += text(100, 551, "Ещё 4 слота ниже · показан один для примера", 14, MUTED)
    body += rect(82, 578, 621, 91, "#f4f6f8", 10)
    body += field(605, "Источник заряда батареи на экране", "Blueberry                                      ▾")
    body += rect(82, 682, 621, 127, "#f4f6f8", 10)
    body += field(710, "Часовой пояс", "UTC+05:00                                    ▾")
    body += text(100, 792, "Летнее время: выкл", 16)
    body += rect(82, 822, 621, 92, "#f4f6f8", 10)
    body += text(100, 850, "Громкость: 31%", 17)
    body += path('M108 881 H673', "#c5ced8", 5) + path('M108 881 H283', BLUE, 5) + dot(283, 881, BLUE, 9)
    body += rect(84, 929, 270, 42, "#1976d2", 8) + text(219, 957, "Сохранить и выйти", 17, "#fff", anchor="middle")
    body += rect(366, 929, 313, 42, "#333", 8) + text(522, 957, "Выйти без сохранения", 17, "#fff", anchor="middle")
    body += text(84, 1007, "Сброс устройства…", 16, "#b71c1c")
    notes = [(1, 292, "Сервер и токен", "Источник данных Nightscout"),
             (2, 491, "Сети Wi-Fi", "До 5 сетей и порядок подключения"),
             (3, 625, "Источник батареи", "Blueberry или помпа"),
             (4, 739, "Местное время", "Часовой пояс и летнее время"),
             (5, 867, "Громкость", "0% полностью отключает звук"),
             (6, 955, "Завершение настройки", "Обе кнопки перезагружают дисплей"),
             (7, 1002, "Сброс параметров", "Очищает сохранённые настройки")]
    for n, y, label, caption in notes:
        body += pin(n, 758, y, 688, y)
        body += text(782, y - 3, label, 20, weight=700) + text(782, y + 22, caption, 16, MUTED)
    body += text(36, 1070, "Схема формы сокращена. Пороги глюкозы задаются в Nightscout, на этой странице их полей нет.", 17, MUTED)
    save("web-settings.svg", "Веб-настройки · окно в браузере", "Общая форма для обеих плат. Домен, Wi-Fi и значения показаны для примера.", body, 1110)


if __name__ == "__main__":
    main()
