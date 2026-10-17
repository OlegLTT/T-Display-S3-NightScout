// Точка доступа и веб-настройки: DNS-перенаправление и сохранение параметров.
#include "app_constants.h"
#include "captive_portal.h"
#include "board_config.h"

#include "esp_netif.h"
#include "lwip/inet.h"

#include "time_utils.h"
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Настройка</title>
<style>
  :root { --red:#d32f2f; --yellow:#ffff00; --green:#2e7d32; }

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

  /* Список сохранённых сетей Wi-Fi. */
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

  /* Настройки времени. */
  .row2{ display:grid; grid-template-columns:1fr 1fr; gap:10px; }
  @media(max-width:820px){ .row2{ grid-template-columns:1fr; } }

  /* Цветовая шкала порогов глюкозы. */
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
    width: 100%;
    box-sizing: border-box;
  }
  input.num {
    width: 80px;
    min-width: 0;
    flex: 1 1 80px;
    height: 56px;
    font-size: 22px;
    text-align: center;
    border-radius: 12px;
    border: 2px solid rgba(0,0,0,0.4);
    outline: none;
    background: rgba(255,255,255,0.95);
    padding:0;
  }
  @media(max-width:480px){ .boundsRow{ gap:8px; padding:16px 8px; } }

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


  /* Кнопки сохранения, выхода и сброса. */
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
  <div class="title">Настройки устройства</div>
  <div class="muted">Настройки сохраняются на этом устройстве. Из Nightscout обновляются только пороги глюкозы.</div>

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

    <div style="margin-top:12px">
      <div class="muted" style="margin-bottom:6px;">Токен доступа (Nightscout)</div>

      <div>
        <input
          id="token"
          placeholder="security token"
          autocomplete="off"
          style="width:100%;"
        >
        <div class="muted" style="margin-top:6px;">
          Если Nightscout защищён, сюда вставьте token. Если не нужен — оставьте пустым.
        </div>
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
    <label for="batterySource">Источник заряда батареи на экране</label>
    <select id="batterySource">
      <option value="0">Blueberry</option>
      <option value="1">Помпа</option>
    </select>
    <div class="muted">Показывается заряд выбранного устройства в процентах.
      Если данных нет, на экране будет --%. Напряжение помпы в проценты не пересчитывается.</div>
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

  <div class="card" id="nsConnectCard" style="display:none">
    <button class="btn" id="connectNsBtn" type="button">Обновить пороги из Nightscout</button>
    <div class="muted" id="nsConnectStatus" role="status">Укажите сервер, токен и домашний Wi-Fi выше.</div>
  </div>
  <div class="card" id="boundsCard">
    <div style="font-size:16px; font-weight:650;">Пороги глюкозы (ммоль/л)</div>

    <div class="band" id="band" style="margin-top:10px;">
      <div class="boundsRow">
        <input class="num" id="b0" type="text" inputmode="decimal" autocomplete="off" aria-label="Нижняя красная граница" placeholder="0.0">
        <input class="num" id="b1" type="text" inputmode="decimal" autocomplete="off" aria-label="Нижняя жёлтая граница" placeholder="0.0">
        <input class="num" id="b2" type="text" inputmode="decimal" autocomplete="off" aria-label="Верхняя жёлтая граница" placeholder="0.0">
        <input class="num" id="b3" type="text" inputmode="decimal" autocomplete="off" aria-label="Верхняя красная граница" placeholder="0.0">
      </div>
    </div>

    <div class="muted">Должно быть строго: b0 &lt; b1 &lt; b2 &lt; b3</div>
    <div class="muted" id="nsBoundsNotice" style="display:none"></div>
    <button class="linkBtn" id="serverBoundsBtn" type="button" style="display:none">Использовать пороги Nightscout</button>
  </div>

  <div class="card" id="alarmCard" style="display:none">
    <label for="alarmVolume">Громкость: <span id="alarmVolumeValue">—</span></label>
    <input type="range" id="alarmVolume" min="0" max="100" step="1" value="0">
    <div class="muted" style="margin-top:8px">
      0% отключает звук. Сигнал звучит только в красных зонах или после
      <span id="expiryMinutes"></span> минут без свежих данных Nightscout.
      Возраст отсчитывается от времени измерения. Snooze откладывает обе тревоги
      последовательно на <span id="snoozeMinutes"></span> минут; следующее нажатие отменяет отсрочку. Нажимайте с интервалом меньше 2 секунд; после 2 секунд цикл начинается заново. Пауза и отмена синхронизируются по локальной сети.
    </div>
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
  const alarmVolume = document.getElementById('alarmVolume');
  const alarmVolumeValue = document.getElementById('alarmVolumeValue');
  alarmVolume.addEventListener('input', () => {
    alarmVolumeValue.textContent = alarmVolume.value + '%';
  });
  let soundAlarmSupported = false;
  let nsBoundsDevice = false;
  let nsBoundsReady = false;
  let boundsDirty = false;
  let boundsOverridden = false;
  let serverBounds = [];
  let cfgLoaded = false;
  let localHost = '';
  let nsLoadEpoch = 0;

  function resetNsLoad() {
    ++nsLoadEpoch;
    nsBoundsReady=false;
    serverBounds=[];
    updateBoundsNotice();
  }

  function updateBoundsNotice() {
    document.getElementById('nsBoundsNotice').textContent = boundsOverridden
        ? 'Ручные пороги сохраняются только на этом устройстве и не меняют Nightscout или другие устройства.'
        : 'Пороги обновляются из Nightscout. Редактирование сохраняет ручные значения только на этом устройстве.';
    document.getElementById('serverBoundsBtn').style.display = nsBoundsDevice && boundsOverridden ? '' : 'none';
  }

  function showNsState(j) {
    nsBoundsDevice = !!j.nsBoundsDevice;
    const sameConnection = j.host === sanitizeHost(document.getElementById('host').value)
        && j.token === document.getElementById('token').value.trim();
    nsBoundsReady = sameConnection && !!j.nsBoundsReady && j.nsOrigin === j.host;
    serverBounds = nsBoundsReady ? j.serverBounds : [];
    document.getElementById('nsConnectCard').style.display = nsBoundsDevice ? '' : 'none';
    document.getElementById('alarmCard').style.display = soundAlarmSupported ? '' : 'none';
    document.getElementById('nsBoundsNotice').style.display = nsBoundsDevice ? '' : 'none';
    document.getElementById('nsConnectStatus').textContent = nsBoundsReady
        ? 'Пороги Nightscout получены.'
        : 'Локальные настройки доступны без сервера. Пороги обновятся при подключении к Nightscout.';
    if (sameConnection && (!boundsDirty || (!boundsOverridden && nsBoundsReady))
        && !boundsInputs.includes(document.activeElement)) {
      if (!boundsDirty) boundsOverridden=!!j.boundsOverride;
      const values=boundsDirty ? serverBounds : j.bounds;
      values.forEach((value,i)=>document.getElementById('b'+i).value=Number(value).toFixed(1));
      updateGradient();
    }
    updateBoundsNotice();
  }
  document.getElementById('serverBoundsBtn').addEventListener('click',()=>{
    boundsDirty=true;
    boundsOverridden=false;
    if (nsBoundsReady) serverBounds.forEach((value,i)=>document.getElementById('b'+i).value=Number(value).toFixed(1));
    updateBoundsNotice();
  });
  document.getElementById('connectNsBtn').addEventListener('click',async()=>{
    const status=document.getElementById('nsConnectStatus');
    resetNsLoad();
    const epoch=nsLoadEpoch;
    try {
      const form=new URLSearchParams({host:sanitizeHost(hostInp.value),token:tokenInp.value.trim()});
      slots.forEach((slot,i)=>{
        form.set('ssid'+i,(slot.ssid||'').trim());
        form.set('passChanged'+i,slot.passDirty?'1':'0');
        form.set('pass'+i,slot.passDirty?(slot.pass||''):'');
      });
      const response=await fetch('/connect_ns',{method:'POST',body:form});
      if (epoch!==nsLoadEpoch) return;
      if (!response.ok) throw new Error(await response.text());
      status.textContent='Подключение к серверу…';
    } catch(error) { if (epoch===nsLoadEpoch) status.textContent=error.message; }
  });
  setInterval(async()=>{
    if (!nsBoundsDevice || !cfgLoaded) return;
    const epoch=nsLoadEpoch;
    try {
      const response=await fetch('/cfg',{cache:'no-store'});
      if (epoch!==nsLoadEpoch) return;
      if (response.ok) {
        const state=await response.json();
        if (epoch===nsLoadEpoch) showNsState(state);
      }
    } catch(error) {
      if (epoch===nsLoadEpoch) document.getElementById('nsConnectStatus').textContent='Нет связи с устройством';
    }
  },2000);

  const band = document.getElementById('band');
  const boundsInputs = [b0,b1,b2,b3];

  const hostInp = document.getElementById('host');
  const tokenInp = document.getElementById('token');
  for (const input of [hostInp,tokenInp]) input.addEventListener('input',()=>{
    if (!nsBoundsDevice) return;
    resetNsLoad();
    if (input===hostInp && sanitizeHost(hostInp.value)!==localHost) {
      boundsOverridden=false;
      boundsDirty=true;
      updateBoundsNotice();
    }
    document.getElementById('nsConnectStatus').textContent='Настройки подключения изменены. Сохраните их или нажмите «Обновить пороги».';
  });


  // Сеть хранит SSID, пароль, признаки сохранённого и изменённого пароля, а также идентификатор.
  let slots = []; // Число строк приходит из настроек прошивки вместе с данными сетей.

  // Нормализация адреса Nightscout перед отправкой на устройство.
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

  // Отображение границ и цветовой шкалы глюкозы.
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
    i.addEventListener('input', ()=>{
      normalizeFloat1(i);
      boundsDirty=true;
      boundsOverridden=true;
      updateBoundsNotice();
    });
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

  // Выбор часового пояса и собственного смещения UTC.
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

  // Редактирование сохранённых сетей Wi-Fi.
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

    // Перетаскивание меняет порядок сетей.
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

  // Загрузка текущих параметров устройства в форму.
  async function loadCfg(){
    const epoch=nsLoadEpoch;
    const r = await fetch('/cfg', {cache:'no-store'});
    if (!r.ok) throw new Error('Не удалось загрузить настройки');
    const j = await r.json();
    if (epoch!==nsLoadEpoch) return;

    hostInp.value = sanitizeHost(j.host || "");
    localHost=hostInp.value;
    tokenInp.value = (j.token || "");

    slots = (j.wifi || []).map((w,idx)=>({
      ssid: w.ssid || "",
      pass: "",
      hasPass: !!w.hasPass,
      passDirty: false,
      id: idx
    }));
    renderAllSlots();

    dst.value = (j.dst ? '1':'0');
    soundAlarmSupported = !!j.soundAlarmSupported;
    document.getElementById('alarmCard').style.display = soundAlarmSupported ? '' : 'none';
    alarmVolume.value = j.alarmVolume;
    document.getElementById('batterySource').value = String(j.batterySource);
    alarmVolumeValue.textContent = alarmVolume.value + '%';
    const expiryLabel=document.getElementById('expiryMinutes');
    if (expiryLabel) expiryLabel.textContent = j.expiryMinutes;
    document.getElementById('snoozeMinutes').textContent = j.snoozeMinutes.join(', ');

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

    boundsOverridden=!!j.boundsOverride;
    boundsDirty=false;
    if(Array.isArray(j.bounds) && j.bounds.length===4){
      j.bounds.forEach((v,i)=>boundsInputs[i].value = Number(v).toFixed(1));
    }
    updateGradient();
    showNsState(j);
    cfgLoaded=true;
  }

  // Отправка настроек и выход из портала.
  async function saveOnly(){
    msgEl.textContent='';
    msgEl.className='msg';

    const nums = boundsInputs.map((el,idx)=>{
      const v = el.value.trim().replace(',', '.');
      if(!/^\d+(\.\d)?$/.test(v)) throw new Error(`Граница ${idx+1}: число с 1 десятичным знаком`);
      return parseFloat(v);
    });
    if(!validateBounds(nums)) throw new Error('Границы должны строго возрастать');

    const form = new URLSearchParams();
    for(let i=0;i<slots.length;i++){
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
    form.set('batterySource', document.getElementById('batterySource').value);
    if (soundAlarmSupported) form.set('alarmVolume', alarmVolume.value);
    if (nsBoundsDevice) {
      form.set('boundsOverride',boundsOverridden?'1':'0');
    }
    nums.forEach((v,i)=>form.set('b'+i, v.toFixed(1)));

    // Перед отправкой убираем из адреса лишние части URL.
    const hostClean = sanitizeHost(hostInp.value || '');
    hostInp.value = hostClean;
    form.set('host', hostClean);
    form.set('token', (tokenInp.value || '').trim());

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

  // Начальное заполнение формы после загрузки страницы.
  window.addEventListener('resize', updateGradient);
  window.addEventListener('scroll', updateGradient, {passive:true});

  window.addEventListener('load', async ()=>{
    try {
      buildTzList();
      setCustomEnabled(false);
      await loadCfg();
      requestAnimationFrame(()=>requestAnimationFrame(updateGradient));
    } catch (error) {
      msgEl.textContent = 'Ошибка загрузки настроек. Обновите страницу. ' + error.message;
      msgEl.className = 'msg err';
    }
  });
</script>
</body>
</html>)HTML";

// Принимается только целое число от 0 до 100, без знака и посторонних символов.
static bool parseVolume(const String& value, uint8_t& result) {
  if (value.isEmpty() || value.length() > 3) return false;
  unsigned parsed = 0;
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
    parsed = parsed * 10 + unsigned(value[i] - '0');
  }
  if (parsed > 100) return false;
  result = uint8_t(parsed);
  return true;
}

static String genApPass(uint8_t len = app_constants::portal_password_length) {

  if (len < 8) len = 8;

  static const char* alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const size_t n = strlen(alphabet);

  String out;
  out.reserve(len);
  for (uint8_t i = 0; i < len; i++) {
    uint32_t r = esp_random();
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

  Serial.println(s);
}
void CaptivePortal::logE(const String& s) {
  Serial.println(s);
  if (_logs) _logs->add(LogLevel::Error, s);
}

static bool parseBound(const String& s, float& result) {
  String v = s;
  v.trim();
  v.replace(',', '.');
  char* end = nullptr;
  result = strtof(v.c_str(), &end);
  return end != v.c_str() && *end == '\0' && std::isfinite(result) && result >= 0;
}

static void sanitizeHostInPlace(String& h) {
  h.trim();

  if (h.startsWith("http://"))  h.remove(0, 7);
  if (h.startsWith("https://")) h.remove(0, 8);

  int slash = h.indexOf('/');
  if (slash >= 0) h.remove(slash);

  while (h.endsWith("/")) h.remove(h.length() - 1);

  h.replace(" ", "");
}

// DHCP сообщает клиентам адрес устройства как DNS-сервер для обнаружения портала.
static bool forceApDnsToSelf(const IPAddress& apIP) {
  esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (!ap) return false;

  esp_err_t stopped = esp_netif_dhcps_stop(ap);
  if (stopped != ESP_OK && stopped != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) return false;

  esp_netif_dns_info_t dns{};
  dns.ip.type = IPADDR_TYPE_V4;
  dns.ip.u_addr.ip4.addr = ipaddr_addr(apIP.toString().c_str());
  const esp_err_t dnsResult = esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns);

  uint8_t offerDns = 0x02;
  const esp_err_t optionResult = esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
      ESP_NETIF_DOMAIN_NAME_SERVER, &offerDns, sizeof(offerDns));
  const esp_err_t startResult = esp_netif_dhcps_start(ap);
  return dnsResult == ESP_OK && optionResult == ESP_OK && startResult == ESP_OK;
}

// Запуск AP, DNS и HTTP; проверочные запросы ОС направляются на страницу настройки.
bool CaptivePortal::begin(LogBuffer* logs, SettingsStore* store, AppSettings* settings) {
  if (_active) return true;

  _logs = logs;
  _store = store;
  _settings = settings;

  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);

  WiFi.softAPConfig(_apIP, _apIP, IPAddress(255, 255, 255, 0));

  _apPass = genApPass();

  bool apOk = WiFi.softAP(_apSsid.c_str(), _apPass.c_str());
  if (!apOk) {
    logE("[AP] Failed to start access point");
    return false;
  }

  if (!forceApDnsToSelf(_apIP)) logE("[AP] Failed to configure DHCP DNS");

  logI(String("[AP] SSID: ") + _apSsid);
  logI(String("[AP] Password: ") + _apPass);
  logI(String("[AP] IP: ") + WiFi.softAPIP().toString());

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
        logI(String("[AP] Client connected: ") + mac);
      }

      if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                 info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                 info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
        logI(String("[AP] Client disconnected: ") + mac);
      }
    });
  }

  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  _dns.setTTL(0);
  if (!_dns.start(53, "*", _apIP)) {
    logE("[AP] Failed to start DNS server");
    WiFi.softAPdisconnect(true);
    return false;
  }

  auto noCache = [this]() {
    _srv.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    _srv.sendHeader("Pragma", "no-cache");
    _srv.sendHeader("Expires", "0");
  };

  auto logReq = [this]() {
    logI(String("[HTTP] Host=") + _srv.hostHeader() + " path=" + _srv.uri());
  };

  auto sendPortal = [this, noCache, logReq]() {
    logReq();
    noCache();

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

  _srv.on("/connect_ns", HTTP_POST, [this, noCache] {
    noCache();
    AppSettings draft = *_settings;
    if (!readConnectionForm(draft) || !draft.hostSite[0]) {
      _srv.send(400,"text/plain; charset=utf-8","Проверьте адрес сервера и данные Wi-Fi"); return;
    }
    bool haveNetwork=false;
    for (unsigned i=0;i<MAX_WIFI;++i) haveNetwork |= draft.wifi[i].ssid[0]!=0;
    if (!haveNetwork && WiFi.status()!=WL_CONNECTED) {
      _srv.send(400,"text/plain; charset=utf-8","Укажите домашнюю сеть Wi-Fi"); return;
    }
    _nsState=ns_alarm::Snapshot{};
    _connectionDraft=draft; _connectionPending=true;
    _srv.send(202,"text/plain","Connecting");
  });

  _srv.on("/exit", HTTP_POST, [this, logReq, noCache] {
    logReq();
    noCache();
    _srv.send(200, "text/plain; charset=utf-8", "OK");
    scheduleRestart();
  });

  _srv.on("/reboot", HTTP_POST, [this, logReq, noCache] {
    logReq();
    noCache();
    _srv.send(200, "text/plain; charset=utf-8", "OK");
    scheduleRestart();
  });

  _srv.on("/favicon.ico", HTTP_GET, [this, logReq, noCache] {
    logReq();
    noCache();
    _srv.send(204);
  });

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

  _srv.on("/connecttest.txt", HTTP_GET, redirectToPortal);
  _srv.on("/connecttest.txt", HTTP_HEAD, redirectToPortal);
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

    if (_store->factoryReset(*_settings)) {
      _srv.send(200, "text/plain; charset=utf-8", "OK");
      scheduleRestart();
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

// Обслуживание DNS и HTTP должно регулярно вызываться из основного цикла.
void CaptivePortal::tick() {
  if (_restartPending && uint32_t(millis()-_restartAt)>=150) ESP.restart();
  if (!_active) return;
  _dns.processNextRequest();
  _srv.handleClient();
}

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

    bool hasPass = (_settings->wifi[i].pass[0] != 0);
    j += "\"hasPass\":" + String(hasPass ? "true" : "false") + ",";
    j += "\"pass\":\"\"";
    j += "}";
  }
  j += "],";

  j += "\"host\":\"" + escJson(String(_settings->hostSite)) + "\",";
  j += "\"token\":\"" + escJson(String(_settings->nsToken)) + "\",";

  j += "\"tzMin\":" + String(_settings->tzMinutes) + ",";
  j += "\"dst\":" + String(_settings->dstEnable ? "true" : "false") + ",";
  j += "\"alarmVolume\":" + String(_settings->alarmVolume) + ",";
  j += "\"boundsOverride\":" + String(_settings->boundsOverridden ? "true" : "false") + ",";
#if defined(BOARD_TDISPLAY_S3) || defined(BOARD_M5CORE_S3)
  j += "\"nsBoundsDevice\":true,";
#else
  j += "\"nsBoundsDevice\":false,";
#endif
  const bool nsReady=ns_alarm::server_bounds_ready(_nsState,_settings->hostSite);
  j += "\"nsBoundsReady\":" + String(nsReady?"true":"false") + ",";
  j += "\"nsOrigin\":\"" + escJson(_nsState.origin) + "\",";
  j += "\"nsMessage\":\"" + escJson(_nsState.message) + "\",";
  j += "\"batterySource\":" + String(static_cast<uint8_t>(_settings->batterySource)) + ",";
  j += "\"expiryMinutes\":" + String(app_constants::data_expiry_seconds / 60) + ",";
  j += "\"snoozeMinutes\":[";
  for (unsigned step = 1; step <= app_constants::snooze_steps; ++step) {
    if (step > 1) j += ",";
    j += String(step * app_constants::snooze_step_minutes);
  }
  j += "],";
#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)
  j += "\"soundAlarmSupported\":true,";
#else
  j += "\"soundAlarmSupported\":false,";
#endif
  j += "\"bounds\":[";
  const float* bounds=_settings->sgvBounds;
  for (unsigned i=0;i<4;++i) { if (i) j+=','; j+=String(bounds[i],1); }
  j += ']';
  j += ",\"serverBounds\":[";
  if (nsReady) for (unsigned i=0;i<4;++i) { if (i) j+=','; j+=String(_nsState.bounds[i],1); }
  j += ']';
  j += "}";

  _srv.send(200, "application/json; charset=utf-8", j);
}

// Проверяем поля до записи и обновляем рабочие параметры только после успешного сохранения.
bool CaptivePortal::readConnectionForm(AppSettings& s) {
  for (int i=0;i<MAX_WIFI;++i) {
    String ssid=_srv.arg("ssid"+String(i)); ssid.trim();
    const String password=_srv.arg("pass"+String(i));
    if (ssid.length()>=sizeof(s.wifi[i].ssid) || password.length()>=sizeof(s.wifi[i].pass)) return false;
    s.wifi[i].pass[0]=0;
    strlcpy(s.wifi[i].ssid,ssid.c_str(),sizeof(s.wifi[i].ssid));
    if (_srv.arg("passChanged"+String(i))=="1") strlcpy(s.wifi[i].pass,password.c_str(),sizeof(s.wifi[i].pass));
    else for (int j=0;j<MAX_WIFI;++j) {
      if (ssid==_settings->wifi[j].ssid) { strlcpy(s.wifi[i].pass,_settings->wifi[j].pass,sizeof(s.wifi[i].pass)); break; }
    }
  }
  String host=_srv.arg("host"); sanitizeHostInPlace(host);
  String token=_srv.arg("token"); token.trim();
  if (host.length()>=sizeof(s.hostSite) || token.length()>=sizeof(s.nsToken)) return false;
  strlcpy(s.hostSite,host.c_str(),sizeof(s.hostSite));
  strlcpy(s.nsToken,token.c_str(),sizeof(s.nsToken));
  return true;
}

void CaptivePortal::handleSave() {
  if (!_settings || !_store) {
    logE("[HTTP] /save: settings storage unavailable");
    _srv.send(500, "text/plain; charset=utf-8", "no store/settings");
    return;
  }

  AppSettings s = *_settings;

  if (!readConnectionForm(s)) {
    _srv.send(400,"text/plain","Invalid connection settings"); return;
  }

  String tzStr = _srv.arg("tzOff");
  if (tzStr.length() == 0) tzStr = _srv.arg("tzMin");

  int tz = tzStr.toInt();
  if (tz < -14 * 60) tz = -14 * 60;
  if (tz > +14 * 60) tz = +14 * 60;

  s.tzMinutes = (int16_t)tz;
  s.dstEnable = (_srv.arg("dst") == "1");
  // Старая форма без этого поля сохраняет прежний выбор источника.
  if (_srv.hasArg("batterySource")) {
    const String source = _srv.arg("batterySource");
    if (source != "0" && source != "1") {
      _srv.send(400, "text/plain; charset=utf-8", "invalid batterySource");
      return;
    }
    s.batterySource = source == "1" ? BatterySource::Pump : BatterySource::Blueberry;
  }
#if defined(BOARD_M5CORE_S3) || defined(BOARD_TDISPLAY_S3)
  if (_srv.hasArg("alarmVolume") && !parseVolume(_srv.arg("alarmVolume"), s.alarmVolume)) {
    _srv.send(400, "text/plain", "alarmVolume must be 0..100");
    return;
  }
#endif

  if (strcmp(s.hostSite,_settings->hostSite)!=0) {
    s.boundsOverridden=false;
    s.boundsOrigin[0]=0;
  }

#if defined(BOARD_TDISPLAY_S3) || defined(BOARD_M5CORE_S3)
  const bool nsReady=ns_alarm::server_bounds_ready(_nsState,s.hostSite)
      && strcmp(s.nsToken,_settings->nsToken)==0;
#endif

  bool hasBounds=_srv.hasArg("boundsOverride");
  for (unsigned i=0;i<4;++i) hasBounds |= _srv.hasArg("b"+String(i));
  if (hasBounds) {
    for (unsigned i=0;i<4;++i) {
      if (!_srv.hasArg("b"+String(i)) || !parseBound(_srv.arg("b"+String(i)),s.sgvBounds[i])) {
        _srv.send(400,"text/plain; charset=utf-8","Укажите четыре числовые границы."); return;
      }
    }
    if (!s.isValidBounds()) {
      _srv.send(400,"text/plain; charset=utf-8","Границы должны строго возрастать."); return;
    }
    const String overrideValue=_srv.hasArg("boundsOverride") ? _srv.arg("boundsOverride") : "1";
    if (overrideValue!="0" && overrideValue!="1") {
      _srv.send(400,"text/plain","Invalid boundsOverride"); return;
    }
    s.boundsOverridden=overrideValue=="1";
#if defined(BOARD_TDISPLAY_S3) || defined(BOARD_M5CORE_S3)
    if (!s.boundsOverridden) {
      if (nsReady) memcpy(s.sgvBounds,_nsState.bounds,sizeof(s.sgvBounds));
    }
#endif
    strlcpy(s.boundsOrigin,s.hostSite,sizeof(s.boundsOrigin));
  }

  s.onboardingDone = true;

  if (_store->save(s)) {
    *_settings = s;

    applyTimeConfig(s.tzMinutes, s.dstEnable);

    _srv.send(200, "text/plain; charset=utf-8", "OK");
    return;
  }

  _srv.send(500, "text/plain; charset=utf-8", "save failed");
}
