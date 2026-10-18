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
    value: '', checked: false, style: {}, listeners: {},
    addEventListener(event, handler) { (this.listeners[event] ||= []).push(handler); }
  });
}
const node = id => nodes.get(id);
const calls = [];
let cfg;
const context = vm.createContext({
  ...Object.fromEntries(nodes), URLSearchParams, console,
  document: { getElementById: id => node(id) || null },
  window: { addEventListener() {}, close() {} },
  setTimeout() {},
  setInterval() { throw new Error('The settings page must not poll server thresholds'); },
  fetch: async (url, options) => {
    calls.push({ url, options });
    assert.ok(['/cfg', '/save', '/reboot', '/exit', '/factory_reset'].includes(url));
    return { ok: true, json: async () => cfg, text: async () => 'OK' };
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

(async () => {
  // Threshold controls, previews and manual refresh are completely absent.
  assert.equal(/id="(?:boundsCard|band|b[0-3]|nsConnectCard|connectNsBtn|nsConnectStatus|serverBoundsBtn)"/.test(html), false);
  assert.equal(/id="(?:soundAlarmEnabled|nsEnabled\d|nsMinutes\d|nsUseServer|testSoundBtn)"/.test(html), false);
  node('tzSel').options=[{value:'0'}];
  const local = { host: 'server.test', token: 'test-token', wifi: [], tzMin: 0, dst: false,
    batterySource: 0, alarmVolume: 31, soundAlarmSupported: true,
    expiryMinutes: 15, snoozeMinutes: [30,60,90,120] };
  cfg=local;
  // First setup saves all local settings without thresholds or a live Nightscout server.
  await run('loadCfg()');
  assert.equal(node('alarmCard').style.display, '');
  assert.equal(node('host').value, local.host);
  await run('saveOnly()');
  assert.deepEqual(calls.map(call=>call.url), ['/cfg','/save']);
  let form=calls.at(-1).options.body;
  assert.equal(form.get('alarmVolume'), '31');
  assert.equal(form.has('boundsOverride'), false);
  for (let i=0;i<4;++i) assert.equal(form.has('b'+i), false);

  // Local edits, credentials and Wi-Fi drafts are saved through the same form.
  node('alarmVolume').value='64'; await input('alarmVolume');
  node('tzSel').value='60'; node('dst').value='1'; node('batterySource').value='1';
  node('host').value='https://other.test/api/v1/'; node('token').value=' new-token ';
  run("slots=[{ssid:'local-wifi',pass:'local-pass',passDirty:true,hasPass:true}]");
  await run('saveOnly()');
  form=calls.at(-1).options.body;
  assert.equal(form.get('alarmVolume'), '64');
  assert.equal(node('alarmVolumeValue').textContent, '64%');
  assert.equal(form.get('tzOff'), '60');
  assert.equal(form.get('dst'), '1');
  assert.equal(form.get('batterySource'), '1');
  assert.equal(form.get('ssid0'), 'local-wifi');
  assert.equal(form.get('pass0'), 'local-pass');
  assert.equal(form.get('host'), 'other.test');
  assert.equal(form.get('token'), 'new-token');
  assert.equal(form.has('boundsOverride'), false);
  for (let i=0;i<4;++i) assert.equal(form.has('b'+i), false);

  node('alarmVolume').value='0'; await input('alarmVolume');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('alarmVolume'), '0');
  assert.equal(node('alarmVolumeValue').textContent, '0%');

  // Exit still restarts after saving, using only local endpoints.
  await click('saveExitBtn');
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(calls.at(-2).url, '/save');
  assert.equal(calls.at(-1).url, '/reboot');

  // Unsupported sound controls stay hidden without adding threshold controls.
  cfg={...local,soundAlarmSupported:false};
  await run('loadCfg()');
  assert.equal(node('alarmCard').style.display, 'none');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.has('alarmVolume'), false);
  console.log('Portal regression checks passed');
})().catch(error => { console.error(error); process.exitCode=1; });
