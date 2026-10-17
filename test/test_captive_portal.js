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
const show = state => { context.state = state; run('showNsState(state,true)'); };
const input = async id => {
  for (const handler of node(id).listeners.input || []) await handler({ target: node(id) });
};
const click = async id => {
  for (const handler of node(id).listeners.click || []) await handler({ currentTarget: node(id) });
};
const hidden = () => {
  assert.equal(node('boundsCard').style.display, 'none');
  assert.equal(node('alarmCard').style.display, 'none');
};

(async () => {
  assert.match(html, /id="boundsCard" style="display:none"/);
  assert.match(html, /id="alarmCard" style="display:none"/);
  assert.equal(/<input[^>]+id="nsMinutes\d"/.test(html), false);
  node('host').value = 'server.test'; node('token').value = 'test-token';
  node('tzSel').value = '0'; node('batterySource').value = '0';
  const server = { nsAlarmDevice: true, nsAlarmReady: true, nsOrigin: 'server.test',
    token: 'test-token', nsOverride: false, nsEnabled: [true,true,true,true],
    bounds: [3.5,4,9,13] };
  run('soundAlarmSupported = true');
  show(server); // Background loading cannot reveal either section before the button.
  hidden();
  cfg=server;
  await poll();
  hidden();
  // Saving Wi-Fi/time/etc before server loading must not require hidden bounds.
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.has('b0'), false);

  await click('connectNsBtn');
  hidden(); // An accepted connection request is not a successful settings load.
  cfg={ ...server, nsAlarmReady: false, bounds: [] };
  await poll(); hidden();
  cfg=server; await poll();
  assert.equal(node('boundsCard').style.display, '');
  assert.equal(node('alarmCard').style.display, '');
  assert.equal(node('b0').value, '3.5');
  assert.equal(node('b0').readOnly, true);
  await run('saveOnly()');
  const form = calls.at(-1).options.body;
  assert.equal(form.get('nsEnabled0'), '1');
  assert.equal(form.has('nsMinutes0'), false);
  assert.equal(form.has('b0'), false);

  // Both supported boards use this server-backed menu. Local alarm flags must
  // survive polling while fresh thresholds continue to follow Nightscout.
  node('nsEnabled0').checked = false;
  await input('nsEnabled0');
  assert.equal(node('nsUseServer').checked, false);
  cfg = { ...server, bounds: [3.6,4.2,9.2,13.2] };
  await poll();
  assert.equal(node('nsEnabled0').checked, false);
  assert.equal(node('b0').value, '3.6');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('nsOverride'), '1');
  assert.equal(calls.at(-1).options.body.get('nsEnabled0'), '0');
  assert.equal(calls.at(-1).options.body.has('b0'), false);
  node('nsUseServer').checked = true;
  await input('nsUseServer');
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('nsOverride'), '0');

  // An old /cfg snapshot must not reveal bounds after editing connection fields.
  node('host').value = 'other.test'; await input('host');
  cfg = server; await poll();
  hidden();
  node('host').value = 'server.test'; node('token').value = 'new-token';
  await input('token'); await poll();
  hidden();
  node('token').value = 'test-token'; show(server);
  hidden(); // Restoring the original token still requires a new explicit load.
  await click('connectNsBtn'); cfg=server; await poll();
  cfg = { ...server, nsAlarmReady: false, bounds: [] }; await poll();
  hidden();

  // Failed requests must not reveal settings cached by the background session.
  connectOk=false; await click('connectNsBtn'); show(server); hidden();
  connectOk=true; await click('connectNsBtn'); cfg=server; await poll();

  // An old JSON body arriving after another load cannot reveal the old settings.
  let release;
  nextCfgBody=new Promise(resolve => { release=resolve; });
  const bodyStarted=new Promise(resolve => { cfgBodyStarted=resolve; });
  const stale=poll(); await bodyStarted;
  await click('connectNsBtn'); hidden();
  release(server); await stale; hidden();
  cfg={ ...server, nsAlarmReady: false, bounds: [] }; await poll(); hidden();
  cfg=server; await poll();
  assert.equal(node('alarmCard').style.display, '');

  // Boards without Nightscout alarm support retain the manual threshold form.
  show({ nsAlarmDevice: false, nsAlarmReady: false });
  assert.equal(node('boundsCard').style.display, '');
  assert.equal(node('b0').readOnly, false);
  [3,4,10,14].forEach((value,i) => node('b'+i).value = String(value));
  await run('saveOnly()');
  assert.equal(calls.at(-1).options.body.get('b0'), '3.0');
  console.log('Portal regression checks passed');
})().catch(error => { console.error(error); process.exitCode=1; });
