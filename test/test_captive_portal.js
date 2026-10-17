// Execute the actual embedded portal script against a minimal DOM; no browser dependencies.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../src/captive_portal.cpp'), 'utf8');
const html = source.match(/R"HTML\(([\s\S]*?)\)HTML"/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const nodes = new Map();
for (const [, id] of html.matchAll(/\bid="([^"]+)"/g)) {
  nodes.set(id, {
    value: '', checked: false, style: {}, readOnly: false, listeners: {},
    addEventListener(event, handler) { (this.listeners[event] ||= []).push(handler); },
    getBoundingClientRect() { return { left: 0, width: 100 }; }
  });
}
const node = id => nodes.get(id);
const calls = [];
let cfg;
let poll;
let connectOk = true;
let nextCfgBody;
let cfgBodyStarted;
const context = vm.createContext({
  ...Object.fromEntries(nodes), URLSearchParams, console,
  document: { getElementById: id => node(id) || null },
  window: { addEventListener() {}, close() {} },
  setTimeout() {}, setInterval(fn) { poll = fn; },
  fetch: async (url, options) => {
    calls.push({ url, options });
    const pendingBody = url === '/cfg' ? nextCfgBody : null;
    if (pendingBody) nextCfgBody = null;
    return { ok: url === '/connect_ns' ? connectOk : true,
      json: async () => {
        if (pendingBody) { cfgBodyStarted(); return pendingBody; }
        return cfg;
      }, text: async () => 'Load failed' };
  }
});
vm.runInContext(script, context, { filename: 'embedded-captive-portal.js' });
const run = code => vm.runInContext(code, context);
const input = async id => {
  for (const handler of node(id).listeners.input || []) await handler({ target: node(id) });
};
const click = async id => {
  for (const handler of node(id).listeners.click || []) await handler({ currentTarget: node(id) });
};
const localFormVisible = () => {
  assert.notEqual(node('boundsCard').style.display, 'none');
  assert.equal(node('alarmCard').style.display, '');
};

(async () => {
  assert.match(html, /id="boundsCard">/);
  assert.equal(/<input[^>]+id="nsMinutes\d"/.test(html), false);
  assert.equal(/id="(?:soundAlarmEnabled|nsEnabled\d|nsUseServer|testSoundBtn)"/.test(html), false);
  for (let i=0;i<4;++i) {
    assert.match(html, new RegExp('id="b'+i+'" type="text" inputmode="decimal"'));
  }
  node('tzSel').options=[{value:'0'}];
  const local = { host: 'server.test', token: 'test-token', wifi: [], tzMin: 0, dst: false,
    batterySource: 0, alarmVolume: 31, soundAlarmSupported: true,
    expiryMinutes: 15, snoozeMinutes: [30,60,90,120],
    nsBoundsDevice: true, nsBoundsReady: false, nsOrigin: '', boundsOverride: false,
    bounds: [3.5,4,9,13], serverBounds: [] };
  cfg=local;
  await run('loadCfg()');
  localFormVisible();
  assert.equal(node('b0').value, '3.5');
  assert.equal(node('b0').readOnly, false);
  // All settings save without any server request or live threshold snapshot.
  await run('saveOnly()');
  assert.equal(calls.some(call=>call.url==='/connect_ns'), false);
  assert.equal(calls.at(-1).options.body.get('b0'), '3.5');
  assert.equal(calls.at(-1).options.body.get('boundsOverride'), '0');
  assert.equal(calls.at(-1).options.body.get('alarmVolume'), '31');
  assert.equal(calls.at(-1).options.body.has('soundAlarmEnabled'), false);

  // Only thresholds follow background updates. Local controls and Wi-Fi drafts stay intact.
  node('alarmVolume').value='64'; await input('alarmVolume');
  node('tzSel').value='60'; node('dst').value='1'; node('batterySource').value='1';
  run("slots=[{ssid:'local-wifi',pass:'local-pass',passDirty:true,hasPass:true}]");
  const server={...local,nsBoundsReady:true,nsOrigin:local.host,
    bounds:[3.6,4.2,9.2,13.2],serverBounds:[3.6,4.2,9.2,13.2],
    alarmVolume:99,tzMin:-300,dst:false,batterySource:0,wifi:[{ssid:'other-wifi'}]};
  cfg=server; await poll();
  localFormVisible();
  assert.equal(node('b0').value, '3.6');
  await run('saveOnly()');
  const form = calls.at(-1).options.body;
  assert.equal(form.has('nsEnabled0'), false);
  assert.equal(form.has('nsMinutes0'), false);
  assert.equal(form.get('b0'), '3.6');
  assert.equal(form.get('boundsOverride'), '0');
  assert.equal(form.get('alarmVolume'), '64');
  assert.equal(form.get('tzOff'), '60');
  assert.equal(form.get('dst'), '1');
  assert.equal(form.get('batterySource'), '1');
  assert.equal(form.get('ssid0'), 'local-wifi');
  assert.equal(form.get('pass0'), 'local-pass');

  // Polling must not move the caret while the user has focused an input.
  context.document.activeElement=node('b0');
  cfg={...server,bounds:[3.7,4.3,9.3,13.3],serverBounds:[3.7,4.3,9.3,13.3]};
  await poll();
  assert.equal(node('b0').value, '3.6');
  context.document.activeElement=null;

  // Decimal comma input stays editable, survives polling, and is saved locally.
  node('b0').value='3,2';
  await input('b0');
  assert.equal(node('b0').value, '3.2');
  cfg = { ...server, bounds: [3.6,4.2,9.2,13.2], serverBounds: [3.6,4.2,9.2,13.2] };
  await poll();
  assert.equal(node('b0').value, '3.2');
  assert.equal(node('b1').value, '4.2');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('boundsOverride'), '1');
  assert.equal(calls.at(-1).options.body.get('b0'), '3.2');
  node('b0').value='5'; await input('b0');
  await assert.rejects(run('saveOnly()'), /строго возрастать/);
  node('b0').value='3,2'; await input('b0');
  // Going offline never hides the form or blocks saving or returning to automatic sync.
  cfg={...local,boundsOverride:true,bounds:[3.2,4.2,9.2,13.2]}; await poll();
  localFormVisible();
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('boundsOverride'), '1');
  await click('serverBoundsBtn');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('boundsOverride'), '0');
  assert.equal(node('b0').value, '3.2'); // Cached fallback until the server returns.
  cfg=server; await poll();
  assert.equal(node('b0').value, '3.6');
  // A persisted manual value in /cfg must not undo the return to server bounds.
  cfg={...cfg, boundsOverride:true, bounds:[3.2,4,9,13]}; await poll();
  assert.equal(node('b0').value, '3.6');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('boundsOverride'), '0');
  node('alarmVolume').value='0'; await input('alarmVolume');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('alarmVolume'), '0');
  assert.equal(node('alarmVolumeValue').textContent, '0%');

  // A new page/load shows the saved manual thresholds, not the server defaults.
  cfg={...local,boundsOverride:true,bounds:[3.2,4,9,13]}; await run('loadCfg()');
  assert.equal(node('b0').value, '3.2');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('boundsOverride'), '1');

  // Old snapshots must not overwrite a draft for another server; the local form remains visible.
  node('host').value = 'other.test'; await input('host');
  cfg = server; await poll();
  localFormVisible();
  assert.equal(node('host').value, 'other.test');
  assert.equal(node('b0').value, '3.2');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('host'), 'other.test');
  assert.equal(calls.at(-1).options.body.get('boundsOverride'), '0');
  node('host').value = 'server.test'; node('token').value = 'new-token';
  await input('token'); await poll();
  localFormVisible();
  assert.equal(node('token').value, 'new-token');
  node('token').value = 'test-token'; await input('token');

  // An optional threshold refresh can fail without blocking local settings.
  connectOk=false; await click('connectNsBtn'); localFormVisible();
  await run('saveOnly()');
  connectOk=true;
  cfg=local; await run('loadCfg()');

  // A JSON body arriving after a new refresh request cannot restore the old server state.
  let release;
  nextCfgBody=new Promise(resolve => { release=resolve; });
  const bodyStarted=new Promise(resolve => { cfgBodyStarted=resolve; });
  const stale=poll(); await bodyStarted;
  await click('connectNsBtn'); localFormVisible();
  release(server); await stale;
  assert.equal(node('b0').value, '3.5');
  cfg=local; await poll(); localFormVisible();
  cfg=server; await poll();
  assert.equal(node('alarmCard').style.display, '');

  // Boards without threshold sync retain the same local editable form.
  cfg={...local,nsBoundsDevice:false}; await run('loadCfg()');
  localFormVisible();
  assert.equal(node('b0').readOnly, false);
  [3,4,10,14].forEach((value,i) => node('b'+i).value = String(value));
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('b0'), '3.0');
  console.log('Portal regression checks passed');
})().catch(error => { console.error(error); process.exitCode=1; });
