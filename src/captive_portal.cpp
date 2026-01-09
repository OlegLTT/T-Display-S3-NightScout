// ===== captive_portal.cpp =====
#include "captive_portal.h"

#include "esp_netif.h"
#include "lwip/inet.h"

#include "time_utils.h" // нужен applyTimeConfig()
#include <pgmspace.h>

// ==================== Статический HTML в PROGMEM ====================

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Настройка</title>
<style>
  :root { --red:#d32f2f; --yellow:#fbc02d; --green:#2e7d32; }

  body{
    margin:0;
    font-family:system-ui,Arial,sans-serif;
    background:#f0f0f0;
    display:flex;
    flex-direction:column;
    align-items:center;
    gap:14px;
    padding:16px 0 20px;
  }

  .wrap{ width:min(920px,94vw); display:flex; flex-direction:column; gap:14px; }
  .title{ font-size:18px; font-weight:650; margin:2px 0 0; padding:0 2px; }

  .card{
    background:#fff;
    border-radius:14px;
    padding:14px 14px;
    box-shadow:0 6px 18px rgba(0,0,0,.08);
  }

  .muted{ color:#666; font-size:13px; }

  /* ---- WiFi slots ---- */
  .slots{ display:flex; flex-direction:column; gap:10px; margin-top:10px; }

  .slot{
    border:1px solid rgba(0,0,0,.18);
    border-radius:14px;
    background:#fafafa;
    padding:10px 10px;
    display:flex;
    flex-direction:column;
    gap:10px;
  }

  .slotTop{
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:10px;
  }
  .slotTitle{
    font-size:14px;
    font-weight:650;
    display:flex;
    align-items:center;
    gap:10px;
    min-width:0;
  }
  .dragHandle{
    width:26px; height:26px;
    border-radius:8px;
    display:flex; align-items:center; justify-content:center;
    cursor:grab;
    user-select:none;
    background:rgba(0,0,0,.06);
  }
  .prioBtns{ display:flex; gap:6px; }
  .prioBtns button{
    height:32px; min-width:36px;
    border-radius:10px;
    border:none; cursor:pointer;
    background:#e0e0e0;
  }
  .prioBtns button:hover{ background:#d6d6d6; }

  .row{
    display:grid;
    grid-template-columns: 90px 1fr;
    gap:10px;
    align-items:center;
  }
  .row label{
    font-size:13px;
    color:#333;
    margin:0;
    white-space:nowrap;
  }

  input, select{
    width:100%;
    height:44px;
    border-radius:10px;
    border:1px solid rgba(0,0,0,.25);
    padding:0 12px;
    font-size:15px;
    outline:none;
    box-sizing:border-box;
    background:#fff;
  }

  .passWrap{
    position:relative;
    display:flex;
    align-items:center;
  }
  .passWrap input{ padding-right:44px; }
  .eye{
    position:absolute;
    right:8px;
    width:34px;
    height:34px;
    border-radius:10px;
    border:none;
    background:rgba(0,0,0,.06);
    cursor:pointer;
  }

  /* ---- Time card ---- */
  .row2{ display:grid; grid-template-columns:1fr 1fr; gap:10px; }
  @media(max-width:820px){ .row2{ grid-template-columns:1fr; } }

  /* ---- SGV band ---- */
  .band {
    width: 100%;
    padding: 40px 0;
    display: flex;
    justify-content: center;
    background: var(--red);
    transition: background 0.15s linear;
    border-radius:14px;
    overflow:hidden;
  }
  .boundsRow {
    display: flex;
    gap: 14px;
    padding: 16px 20px;
    border-radius: 14px;
    background: rgba(255,255,255,0.25);
    backdrop-filter: blur(6px);
  }
  input.num {
    width: 80px;
    height: 56px;
    font-size: 22px;
    text-align: center;
    border-radius: 12px;
    border: 2px solid rgba(0,0,0,0.4);
    outline: none;
    background: rgba(255,255,255,0.95);
    padding:0;
  }

  .linkBtn{
    border:none;
    background:none;
    padding:0;
    margin:0;
    color:#666;
    font-size:12px;
    text-decoration:underline;
    cursor:pointer;
  }
  .linkBtn:hover{
    color:#333;
  }


  /* ---- Actions ---- */
  .actions{ display:flex; gap:10px; flex-wrap:wrap; align-items:center; }
  .btn{
    padding:12px 16px;
    font-size:15px;
    border-radius:10px;
    border:none;
    cursor:pointer;
    background:#1976d2;
    color:#fff;
  }
  .btn:hover{ background:#1565c0; }
  .btnDark{ background:#424242; }
  .btnDark:hover{ background:#2f2f2f; }

  .msg{ min-height:20px; font-size:13px; }
  .ok{ color:#1b5e20 } .err{ color:#b71c1c }
</style>
</head>
<body>
<div class="wrap">
  <div class="title">Настройка</div>

  <div class="card">
    <div style="font-size:16px; font-weight:650; margin-bottom:6px;">
      Адрес сервера Nightscout
    </div>

    <div>
      <input
        id="host"
        placeholder="например: j12345678.nightscout-jino.ru или 10.0.0.5:1337"
        autocomplete="off"
        style="width:100%;"
      >
      <div class="muted" style="margin-top:6px;">
        Указывайте только домен (и порт при необходимости), без http:// и без /api/...
      </div>
    </div>
  </div>

  <div class="card">
    <div style="display:flex; align-items:flex-end; justify-content:space-between; gap:10px;">
      <div>
        <div style="font-size:16px; font-weight:650;">Порядок подключения к Wi-Fi (приоритет сверху вниз)</div>
        <div class="muted">SSID вводится вручную. Пустой SSID очищает слот.</div>
      </div>
    </div>

    <div class="slots" id="slots"></div>
  </div>

  <div class="card">
    <div style="font-size:16px; font-weight:650;">Часовой пояс</div>

    <div class="row2" style="margin-top:10px%;">
      <div>
        <div class="muted" style="margin-bottom:6px;">Часовой пояс</div>
        <select id="tzSel"></select>
        <div class="muted" style="margin-top:6px">Выберите часовой пояс (UTC±HH:MM) или “Свой”.</div>
      </div>

      <div>
        <div class="muted" style="margin-bottom:6px;">Свой (часы и минуты)</div>
        <div class="row2">
          <input id="tzH" inputmode="numeric" placeholder="+5" disabled>
          <input id="tzM" inputmode="numeric" placeholder="0" disabled>
        </div>
        <div class="muted" style="margin-top:6px">Пример: +5 30 (UTC+05:30)</div>
      </div>
    </div>

    <div style="margin-top:12px">
      <div class="muted" style="margin-bottom:6px;">Летнее время</div>
      <select id="dst">
        <option value="0">выкл</option>
        <option value="1">вкл (+1 час)</option>
      </select>
    </div>
  </div>

  <div class="card">
    <div style="font-size:16px; font-weight:650;">Границы подсветки SGV (mmol/L)</div>

    <div class="band" id="band" style="margin-top:10px;">
      <div class="boundsRow">
        <input class="num" id="b0" inputmode="decimal" placeholder="0.0">
        <input class="num" id="b1" inputmode="decimal" placeholder="0.0">
        <input class="num" id="b2" inputmode="decimal" placeholder="0.0">
        <input class="num" id="b3" inputmode="decimal" placeholder="0.0">
      </div>
    </div>

    <div class="muted">Должно быть строго: b0 &lt; b1 &lt; b2 &lt; b3</div>
  </div>

  <div class="card">
    <div class="actions">
      <button class="btn" id="saveExitBtn" type="button">Сохранить и выйти</button>
      <button class="btn btnDark" id="exitNoSaveBtn" type="button">Выйти без сохранения</button>
      <span id="msg" class="msg"></span>
    </div>
    <div class="muted" style="margin-top:8px">
      Оба варианта перезагружают устройство. Если вкладка не закрылась сама — просто закройте вручную.
    </div>
  </div>

  <div class="card" style="padding:8px 14px;">
    <div class="muted" style="font-size:12px; text-align:center;">
      <button id="factoryBtn" class="linkBtn" type="button">
        Сброс устройства…
      </button>
    </div>
  </div>

</div>

<script>
  const slotsEl = document.getElementById('slots');
  const msgEl = document.getElementById('msg');

  const tzSel = document.getElementById('tzSel');
  const tzH = document.getElementById('tzH');
  const tzM = document.getElementById('tzM');
  const dst = document.getElementById('dst');

  const band = document.getElementById('band');
  const boundsInputs = [b0,b1,b2,b3];

  const hostInp = document.getElementById('host');

  // slots: {ssid, pass, hasPass, passDirty, id}
  let slots = Array.from({length:5}, (_,i)=>({ssid:'', pass:'', hasPass:false, passDirty:false, id:i}));

  // ----- HOST sanitize (front-end) -----
  function sanitizeHost(s){
    s = String(s || '').trim();
    s = s.replace(/^https?:\/\//i, '');
    const slash = s.indexOf('/');
    if (slash >= 0) s = s.slice(0, slash);
    s = s.replace(/\/+$/,'');
    s = s.replace(/\s+/g,'');
    return s;
  }
  hostInp.addEventListener('blur', ()=>{ hostInp.value = sanitizeHost(hostInp.value); });
  hostInp.addEventListener('paste', ()=>{ setTimeout(()=>{ hostInp.value = sanitizeHost(hostInp.value); }, 0); });

  // ----- SGV -----
  function normalizeFloat1(el) {
    let v = el.value.replace(',', '.').replace(/[^0-9.]/g,'');
    const parts = v.split('.');
    if (parts.length > 2) v = parts[0] + '.' + parts.slice(1).join('');
    if (v.includes('.')) {
      const [a,b] = v.split('.');
      v = a + '.' + b.slice(0,1);
    }
    el.value = v;
  }
  boundsInputs.forEach(i=>{
    i.addEventListener('input', ()=>normalizeFloat1(i));
    i.addEventListener('paste', ()=>setTimeout(()=>normalizeFloat1(i),0));
  });

  function updateGradient(){
    const bandRect = band.getBoundingClientRect();
    const bandLeft = bandRect.left;
    const bandW    = bandRect.width;

    const centers = boundsInputs.map(el=>{
      const r = el.getBoundingClientRect();
      return r.left + r.width/2;
    });

    const p = centers.map(x => ((x - bandLeft) / bandW * 100).toFixed(2));

    band.style.background = `
      linear-gradient(90deg,
        var(--red) 0%,
        var(--red) ${p[0]}%,
        var(--yellow) ${p[0]}%,
        var(--yellow) ${p[1]}%,
        var(--green) ${p[1]}%,
        var(--green) ${p[2]}%,
        var(--yellow) ${p[2]}%,
        var(--yellow) ${p[3]}%,
        var(--red) ${p[3]}%,
        var(--red) 100%
      )`;
  }

  function validateBounds(nums){
    for(let i=1;i<nums.length;i++){
      if(!(nums[i]>nums[i-1])) return false;
    }
    return true;
  }

  // ----- TZ -----
  function fmtTzLabel(mins){
    const sign = mins >= 0 ? "+" : "-";
    const a = Math.abs(mins);
    const hh = String(Math.floor(a/60)).padStart(2,'0');
    const mm = String(a%60).padStart(2,'0');
    return `UTC${sign}${hh}:${mm}`;
  }

  function buildTzList(){
    tzSel.innerHTML = "";
    const arr = [];
    for(let m=-12*60; m<=14*60; m+=30) arr.push(m);

    const extra = [+5*60+45, +8*60+45, -3*60-30, +9*60+30, +10*60+30];
    for (const x of extra) if (!arr.includes(x)) arr.push(x);
    arr.sort((a,b)=>a-b);

    for(const mins of arr){
      const opt = document.createElement("option");
      opt.value = String(mins);
      opt.textContent = fmtTzLabel(mins);
      tzSel.appendChild(opt);
    }

    const custom = document.createElement("option");
    custom.value = "CUSTOM";
    custom.textContent = "Свой (ввести часы/минуты)";
    tzSel.appendChild(custom);
  }

  function setCustomEnabled(en){
    tzH.disabled = !en;
    tzM.disabled = !en;
  }

  tzSel.addEventListener("change", ()=>{
    const isCustom = (tzSel.value === "CUSTOM");
    setCustomEnabled(isCustom);
    if(!isCustom){ tzH.value=""; tzM.value=""; }
  });

  function clampInt(v, lo, hi){
    v = parseInt(v,10);
    if(Number.isNaN(v)) return null;
    if(v < lo) v = lo;
    if(v > hi) v = hi;
    return v;
  }

  function calcCustomOffsetMinutes(){
    const h = clampInt(tzH.value.trim(), -14, 14);
    if(h === null) throw new Error("Часовой пояс: часы -14..+14");
    const m = clampInt(tzM.value.trim(), 0, 59);
    if(m === null) throw new Error("Часовой пояс: минуты 0..59");

    const sign = (h < 0) ? -1 : 1;
    const total = h*60 + sign*m;
    if(total < -14*60 || total > 14*60) throw new Error("Часовой пояс вне диапазона UTC-14..UTC+14");
    return total;
  }

  // ----- WiFi slots UI -----
  function escapeHtml(s){
    return String(s)
      .replaceAll("&","&amp;")
      .replaceAll("<","&lt;")
      .replaceAll(">","&gt;")
      .replaceAll('"',"&quot;")
      .replaceAll("'","&#39;");
  }

  function renderSlot(slotIndex){
    const slot = slots[slotIndex];
    const MASK = "••••••";

    const el = document.createElement('div');
    el.className = 'slot';
    el.draggable = true;

    const passPlaceholder = "пароль (может быть пустым)";

    const passValue =
      (slot.hasPass && !slot.passDirty)
        ? MASK
        : escapeHtml(slot.pass);

    el.innerHTML = `
      <div class="slotTop">
        <div class="slotTitle">
          <div class="dragHandle" title="Перетащить">≡</div>
          <div>Точка WiFi #${slotIndex+1}</div>
        </div>
        <div class="prioBtns">
          <button type="button" data-act="up" title="Вверх">↑</button>
          <button type="button" data-act="down" title="Вниз">↓</button>
        </div>
      </div>

      <div class="row">
        <label>SSID</label>
        <div>
          <input class="ssidInp" placeholder="имя сети" value="${escapeHtml(slot.ssid)}" autocomplete="off">
        </div>
      </div>

      <div class="row">
        <label>Пароль</label>
        <div class="passWrap">
          <input class="passInp" type="password"
                 placeholder="${passPlaceholder}"
                 value="${passValue}">
          <button class="eye" type="button" title="Показать/скрыть">👁</button>
        </div>
      </div>
    `;

    el.querySelector('[data-act="up"]').addEventListener('click', ()=>{
      if(slotIndex<=0) return;
      const t = slots[slotIndex-1];
      slots[slotIndex-1] = slots[slotIndex];
      slots[slotIndex] = t;
      renderAllSlots();
    });
    el.querySelector('[data-act="down"]').addEventListener('click', ()=>{
      if(slotIndex>=slots.length-1) return;
      const t = slots[slotIndex+1];
      slots[slotIndex+1] = slots[slotIndex];
      slots[slotIndex] = t;
      renderAllSlots();
    });

    const ssidInp = el.querySelector('.ssidInp');
    const passInp = el.querySelector('.passInp');
    const eyeBtn = el.querySelector('.eye');

    ssidInp.addEventListener('input', ()=>{
      slots[slotIndex].ssid = ssidInp.value;
    });

    passInp.addEventListener('input', ()=>{
      const slot = slots[slotIndex];
      slot.passDirty = true;
      slot.pass = passInp.value;
      slot.hasPass = slot.pass.length > 0;
    });

    eyeBtn.addEventListener('click', ()=>{
      passInp.type = (passInp.type === 'password') ? 'text' : 'password';
    });

    // drag & drop reorder
    el.addEventListener('dragstart', (e)=>{
      e.dataTransfer.setData('text/plain', String(slotIndex));
      e.dataTransfer.effectAllowed = 'move';
    });
    el.addEventListener('dragover', (e)=>{
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
    });
    el.addEventListener('drop', (e)=>{
      e.preventDefault();
      const from = parseInt(e.dataTransfer.getData('text/plain'),10);
      const to = slotIndex;
      if(Number.isNaN(from) || from===to) return;
      const item = slots.splice(from,1)[0];
      slots.splice(to,0,item);
      renderAllSlots();
    });

    return el;
  }

  function renderAllSlots(){
    slotsEl.innerHTML = "";
    slots.forEach((_,i)=> slotsEl.appendChild(renderSlot(i)));
    updateGradient();
  }

  // ----- Load cfg -----
  async function loadCfg(){
    const r = await fetch('/cfg', {cache:'no-store'});
    const j = await r.json();

    hostInp.value = sanitizeHost(j.host || "");

    slots = (j.wifi || []).slice(0,5).map((w,idx)=>({
      ssid: w.ssid || "",
      pass: "",
      hasPass: !!w.hasPass,
      passDirty: false,
      id: idx
    }));
    while(slots.length < 5) slots.push({ssid:'', pass:'', hasPass:false, passDirty:false, id:slots.length});
    renderAllSlots();

    dst.value = (j.dst ? '1':'0');

    const cur = Number(j.tzMin ?? 0);
    const found = Array.from(tzSel.options).find(o => o.value === String(cur));
    if(found){
      tzSel.value = String(cur);
      setCustomEnabled(false);
      tzH.value = ""; tzM.value = "";
    } else {
      tzSel.value = "CUSTOM";
      setCustomEnabled(true);
      const sign = cur < 0 ? -1 : 1;
      const a = Math.abs(cur);
      tzH.value = String(sign * Math.floor(a/60));
      tzM.value = String(a%60);
    }

    if(Array.isArray(j.bounds) && j.bounds.length===4){
      j.bounds.forEach((v,i)=>boundsInputs[i].value = Number(v).toFixed(1));
    }
    updateGradient();
  }

  // ----- Save/Exit -----
  async function saveOnly(){
    msgEl.textContent='';
    msgEl.className='msg';

    const nums = boundsInputs.map((el,idx)=>{
      const v = el.value.trim();
      if(!/^\d+(\.\d)?$/.test(v)) throw new Error(`Граница ${idx+1}: число с 1 десятичным знаком`);
      return parseFloat(v);
    });
    if(!validateBounds(nums)) throw new Error('Границы должны строго возрастать');

    const form = new URLSearchParams();
    for(let i=0;i<5;i++){
      const slot = slots[i] || {ssid:'', pass:'', hasPass:false, passDirty:false};
      form.set('ssid'+i, (slot.ssid || '').trim());

      // passChangedN: 1 — менять (берём из passN), 0 — оставить как есть
      if (slot.passDirty) {
        form.set('passChanged'+i, '1');
        form.set('pass'+i, (slot.pass || '').trim());
      } else {
        form.set('passChanged'+i, '0');
        form.set('pass'+i, ''); // игнорируется на бэке
      }
    }

    let tzOffMin = 0;
    if (tzSel.value === "CUSTOM") tzOffMin = calcCustomOffsetMinutes();
    else {
      tzOffMin = parseInt(tzSel.value,10);
      if(Number.isNaN(tzOffMin)) throw new Error("Часовой пояс: выберите значение из списка");
    }

    form.set('tzOff', String(tzOffMin));
    form.set('dst', dst.value === '1' ? '1' : '0');
    nums.forEach((v,i)=>form.set('b'+i, v.toFixed(1)));

    // sanitize host before sending
    const hostClean = sanitizeHost(hostInp.value || '');
    hostInp.value = hostClean;
    form.set('host', hostClean);

    const r = await fetch('/save', {
      method:'POST',
      body: form,
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      cache:'no-store'
    });
    const t = await r.text();
    if(!r.ok) throw new Error(t || 'ошибка сохранения');
  }

  async function postSimple(path){
    const r = await fetch(path, {method:'POST', cache:'no-store'});
    const t = await r.text();
    if(!r.ok) throw new Error(t || 'ошибка');
  }

  function tryClosePage(){ window.close(); }

  const factoryBtn = document.getElementById('factoryBtn');
    if (factoryBtn) {
      factoryBtn.addEventListener('click', () => {
        const ok = confirm(
          'Сбросить ВСЕ настройки устройства (Wi-Fi, Nightscout, границы, часовой пояс)?\n\n' +
          'Устройство выполнит фабричный сброс и перезагрузится.'
        );
        if (!ok) return;

        postSimple('/factory_reset')
          .then(() => {
            msgEl.textContent = 'Фабричный сброс выполнен. Перезагрузка…';
            msgEl.className = 'msg ok';
            setTimeout(tryClosePage, 300);
          })
          .catch(e => {
            msgEl.textContent = e.message;
            msgEl.className = 'msg err';
          });
      });
    }


  document.getElementById('saveExitBtn').addEventListener('click', ()=>{
    saveOnly()
      .then(()=>postSimple('/reboot'))
      .then(()=>{
        msgEl.textContent='Сохранено. Перезагрузка…';
        msgEl.className='msg ok';
        setTimeout(tryClosePage, 300);
      })
      .catch(e=>{
        msgEl.textContent = e.message;
        msgEl.className='msg err';
      });
  });

  document.getElementById('exitNoSaveBtn').addEventListener('click', ()=>{
    postSimple('/exit')
      .then(()=>{
        msgEl.textContent='Выход без сохранения. Перезагрузка…';
        msgEl.className='msg ok';
        setTimeout(tryClosePage, 300);
      })
      .catch(e=>{
        msgEl.textContent = e.message;
        msgEl.className='msg err';
      });
  });

  // ----- Boot -----
  window.addEventListener('resize', updateGradient);
  window.addEventListener('scroll', updateGradient, {passive:true});

  window.addEventListener('load', async ()=>{
    buildTzList();
    setCustomEnabled(false);
    await loadCfg();
    requestAnimationFrame(()=>requestAnimationFrame(updateGradient));
  });
</script>
</body>
</html>)HTML";


// ==================== Локальные утилиты ====================

static String genApPass(uint8_t len = 10) {
  // len должен быть >= 8 (ограничение WPA2)
  if (len < 8) len = 8;

  // без похожих символов (0/O, 1/l/I)
  static const char* alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const size_t n = strlen(alphabet);

  String out;
  out.reserve(len);
  for (uint8_t i = 0; i < len; i++) {
    uint32_t r = esp_random();          // крипто-рандом ESP32
    out += alphabet[r % n];
  }
  return out;
}

static String escJson(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '\\': o += "\\\\"; break;
      case '"':  o += "\\\""; break;
      case '\b': o += "\\b";  break;
      case '\f': o += "\\f";  break;
      case '\n': o += "\\n";  break;
      case '\r': o += "\\r";  break;
      case '\t': o += "\\t";  break;
      default:
        if ((uint8_t)c < 32) { o += ' '; }
        else o += c;
    }
  }
  return o;
}

void CaptivePortal::logI(const String& s) {
  // Информационные сообщения только в Serial, без попадания в экран логов
  Serial.println(s);
}
void CaptivePortal::logE(const String& s) {
  Serial.println(s);
  if (_logs) _logs->add(LogLevel::Error, s);
}

float CaptivePortal::toF(const String& s) {
  String v = s;
  v.replace(',', '.');
  return v.toFloat();
}

// --- IMPORTANT: sanitize Nightscout HOST from user input ---
static void sanitizeHostInPlace(String& h) {
  h.trim();

  // если пользователь вставил http/https — обрежем
  if (h.startsWith("http://"))  h.remove(0, 7);
  if (h.startsWith("https://")) h.remove(0, 8);

  // если вставлен полный URL — отрезать путь/квери
  int slash = h.indexOf('/');
  if (slash >= 0) h.remove(slash);

  // remove trailing slashes just in case
  while (h.endsWith("/")) h.remove(h.length() - 1);

  // remove spaces inside
  h.replace(" ", "");
}

static void forceApDnsToSelf(const IPAddress& apIP) {
  esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (!ap) return;

  esp_netif_dns_info_t dns{};
  dns.ip.type = IPADDR_TYPE_V4;
  dns.ip.u_addr.ip4.addr = ipaddr_addr(apIP.toString().c_str());
  esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns);
}

// ==================== Portal begin/stop/tick ====================

bool CaptivePortal::begin(LogBuffer* logs, SettingsStore* store, AppSettings* settings) {
  if (_active) return true;

  _logs = logs;
  _store = store;
  _settings = settings;

  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(200);

  // Только AP (сканирования больше нет)
  WiFi.mode(WIFI_AP);

  WiFi.softAPConfig(_apIP, _apIP, IPAddress(255, 255, 255, 0));

  // _apSsid = "TDisplay-Setup";
  _apPass = genApPass(8); // 8..12 норм

  bool apOk = WiFi.softAP(_apSsid.c_str(), _apPass.c_str());
  if (!apOk) {
    logE("[AP] softAP failed");
    return false;
  }

  delay(150);
  forceApDnsToSelf(_apIP);

  logI(String("[AP] SSID: ") + _apSsid);
  logI(String("[AP] PASS: ") + _apPass);
  logI(String("[AP] IP  : ") + WiFi.softAPIP().toString());

  if (!_evtInstalled) {
    _evtInstalled = true;

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (!_active) return;

      if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                 info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                 info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
        logI(String("[AP] STA connected: ") + mac);
      }

      if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                 info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                 info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
        logI(String("[AP] STA disconnected: ") + mac);
      }
    });
  }

  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  _dns.start(53, "*", _apIP);

  auto noCache = [this]() {
    _srv.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    _srv.sendHeader("Pragma", "no-cache");
    _srv.sendHeader("Expires", "0");
  };

  auto logReq = [this]() {
    logI(String("[HTTP] host=") + _srv.hostHeader() + " uri=" + _srv.uri());
  };

  auto sendPortal = [this, noCache, logReq]() {
    logReq();
    noCache();
    // HTML сразу из PROGMEM
    _srv.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  };

  auto redirectToPortal = [this, noCache, logReq]() {
    logReq();
    noCache();
    _srv.sendHeader("Location", String("http://") + _apIP.toString() + "/", true);
    _srv.send(302, "text/plain; charset=utf-8", "Redirect");
  };

  _srv.on("/", HTTP_GET, sendPortal);

  _srv.on("/cfg", HTTP_GET, [this, logReq, noCache] {
    logReq();
    noCache();
    handleCfg();
  });

  _srv.on("/save", HTTP_POST, [this, logReq, noCache] {
    logReq();
    noCache();
    handleSave();
  });

  // --- actions ---
  _srv.on("/exit", HTTP_POST, [this, logReq, noCache] {
    logReq();
    noCache();
    _srv.send(200, "text/plain; charset=utf-8", "OK");
    delay(150);
    ESP.restart();
  });

  _srv.on("/reboot", HTTP_POST, [this, logReq, noCache] {
    logReq();
    noCache();
    _srv.send(200, "text/plain; charset=utf-8", "OK");
    delay(150);
    ESP.restart();
  });

  _srv.on("/favicon.ico", HTTP_GET, [this, logReq, noCache] {
    logReq();
    noCache();
    _srv.send(204);
  });

  // Captive probe endpoints
  _srv.on("/success.txt", HTTP_GET, redirectToPortal);
  _srv.on("/success.txt", HTTP_HEAD, redirectToPortal);

  _srv.on("/canonical.html", HTTP_GET, redirectToPortal);
  _srv.on("/canonical.html", HTTP_HEAD, redirectToPortal);

  _srv.on("/generate_204", HTTP_GET, redirectToPortal);
  _srv.on("/gen_204", HTTP_GET, redirectToPortal);
  _srv.on("/generate_204", HTTP_HEAD, redirectToPortal);

  _srv.on("/hotspot-detect.html", HTTP_GET, sendPortal);
  _srv.on("/hotspot-detect.html", HTTP_HEAD, sendPortal);

  _srv.on("/library/test/success.html", HTTP_GET, sendPortal);
  _srv.on("/library/test/success.html", HTTP_HEAD, sendPortal);

  // Windows
  _srv.on("/connecttest.txt", HTTP_GET, [this, logReq, noCache] {
    logReq();
    noCache();
    _srv.send(200, "text/plain; charset=utf-8", "captive");
  });
  _srv.on("/ncsi.txt", HTTP_GET, redirectToPortal);
  _srv.on("/fwlink", HTTP_GET, redirectToPortal);
  _srv.on("/redirect", HTTP_GET, redirectToPortal);

  _srv.onNotFound([this, noCache, logReq]() {
    logReq();
    noCache();
    _srv.sendHeader("Location", String("http://") + _apIP.toString() + "/", true);
    _srv.send(302, "text/plain; charset=utf-8", "Redirect");
  });

  _srv.on("/factory_reset", HTTP_POST, [this, logReq, noCache] {
    logReq();
    noCache();

    if (!_store || !_settings) {
      _srv.send(500, "text/plain; charset=utf-8", "no store/settings");
      return;
    }

    // Полный сброс настроек
    if (_store->factoryReset(*_settings)) {
      _srv.send(200, "text/plain; charset=utf-8", "OK");
      delay(150);
      ESP.restart();
    } else {
      _srv.send(500, "text/plain; charset=utf-8", "factory reset failed");
    }
  });


  _srv.begin();
  _active = true;

  logI("[AP] Captive portal started");
  return true;
}

void CaptivePortal::stop() {
  if (!_active) return;

  _srv.stop();
  _dns.stop();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  _active = false;
  logI("[AP] Captive portal stopped");
}

void CaptivePortal::tick() {
  if (!_active) return;
  _dns.processNextRequest();
  _srv.handleClient();
}

// ==================== cfg/save ====================

void CaptivePortal::handleCfg() {
  if (!_settings) {
    _srv.send(500, "text/plain; charset=utf-8", "no settings");
    return;
  }

  String j = "{";
  j += "\"wifi\":[";
  for (int i = 0; i < MAX_WIFI; i++) {
    if (i) j += ",";
    j += "{";
    j += "\"ssid\":\"" + escJson(_settings->wifi[i].ssid) + "\",";
    // ВАЖНО: пароль не отдаём, только флаг hasPass
    bool hasPass = (_settings->wifi[i].pass[0] != 0);
    j += "\"hasPass\":" + String(hasPass ? "true" : "false") + ",";
    j += "\"pass\":\"\"";
    j += "}";
  }
  j += "],";

  j += "\"host\":\"" + escJson(String(_settings->hostSite)) + "\",";

  j += "\"tzMin\":" + String(_settings->tzMinutes) + ",";
  j += "\"dst\":" + String(_settings->dstEnable ? "true" : "false") + ",";
  j += "\"bounds\":[" +
       String(_settings->sgvBounds[0], 1) + "," +
       String(_settings->sgvBounds[1], 1) + "," +
       String(_settings->sgvBounds[2], 1) + "," +
       String(_settings->sgvBounds[3], 1) + "]";
  j += "}";

  _srv.send(200, "application/json; charset=utf-8", j);
}

void CaptivePortal::handleSave() {
  if (!_settings || !_store) {
    logE("[HTTP] /save: no store/settings");
    _srv.send(500, "text/plain; charset=utf-8", "no store/settings");
    return;
  }

  AppSettings s = *_settings;

  // Wi-Fi slots (порядок задаёт фронт: ssid0..ssid4, pass0..pass4, passChanged0..4)
  for (int i = 0; i < MAX_WIFI; i++) {
    String ss = _srv.arg("ssid" + String(i));
    String pw = _srv.arg("pass" + String(i));
    String ch = _srv.arg("passChanged" + String(i)); // "1" если поле меняли

    ss.trim();
    pw.trim();
    ch.trim();

    if (ss.length() == 0) {
      // пустой SSID — слот очищаем полностью, в т.ч. пароль
      s.wifi[i].ssid[0] = 0;
      s.wifi[i].pass[0] = 0;
    } else {
      strlcpy(s.wifi[i].ssid, ss.c_str(), sizeof(s.wifi[i].ssid));

      if (ch == "1") {
        // пароль реально меняли: можно установить новый (включая пустой, чтобы очистить)
        strlcpy(s.wifi[i].pass, pw.c_str(), sizeof(s.wifi[i].pass));
      } else {
        // пароль не трогали — оставляем как был (_settings уже скопирован в s)
      }
    }
  }

  // host site (sanitize! store only host[:port] without scheme/path)
  String host = _srv.arg("host");
  sanitizeHostInPlace(host);
  strlcpy(s.hostSite, host.c_str(), sizeof(s.hostSite));

  // часовой пояс
  String tzStr = _srv.arg("tzOff");
  if (tzStr.length() == 0) tzStr = _srv.arg("tzMin");

  int tz = tzStr.toInt();
  if (tz < -14 * 60) tz = -14 * 60;
  if (tz > +14 * 60) tz = +14 * 60;

  s.tzMinutes = (int16_t)tz;
  s.dstEnable = (_srv.arg("dst") == "1");

  // bounds
  for (int i = 0; i < 4; i++) {
    s.sgvBounds[i] = toF(_srv.arg("b" + String(i)));
  }
  if (!s.isValidBounds()) {
    _srv.send(400, "text/plain; charset=utf-8", "bounds must be strictly increasing");
    return;
  }

  // пользователь явно сохранил настройки из портала — считаем онбординг пройденным
  s.onboardingDone = true;

  if (_store->save(s)) {
    *_settings = s;

    // Apply TZ/DST instantly for BOTH your legacy time_utils and system localtime()
    applyTimeConfig(s.tzMinutes, s.dstEnable);

    _srv.send(200, "text/plain; charset=utf-8", "OK");
    return;
  }

  _srv.send(500, "text/plain; charset=utf-8", "save failed");
}
