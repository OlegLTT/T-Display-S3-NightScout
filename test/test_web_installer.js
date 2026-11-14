// Run with: node --experimental-vm-modules test/test_web_installer.js
const assert = require('node:assert/strict');
const { createHash, webcrypto } = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = path.resolve(__dirname, '../docs');
const source = fs.readFileSync(path.join(root, 'installer.js'), 'utf8');
const html = fs.readFileSync(path.join(root, 'index.html'), 'utf8');
const expected = { tdisplay: 'ESP32-S3', cores3: 'ESP32-S3', m5stack: 'ESP32' };
const fixtures = {};

for (const [board, family] of Object.entries(expected)) {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'manifests', `${board}.json`), 'utf8'));
  assert.equal(manifest.board, board);
  assert.equal(manifest.builds.length, 1);
  assert.equal(manifest.builds[0].chipFamily, family);
  assert.equal(manifest.builds[0].parts.length, 1);
  assert.equal(manifest.builds[0].parts[0].offset, 0);
  assert.equal(manifest.new_install_prompt_erase, false);
  assert.equal(manifest.new_install_improv_wait_time, 0);
  const file = path.resolve(root, 'manifests', manifest.builds[0].parts[0].path);
  assert(file.startsWith(root + path.sep));
  const bytes = fs.readFileSync(file);
  assert.equal(bytes.length, manifest.size);
  assert.equal(createHash('sha256').update(bytes).digest('hex'), manifest.sha256);
  const bootOffset = family === 'ESP32' ? 0x1000 : 0;
  assert.equal(bytes[bootOffset], 0xE9);
  assert.equal(bytes.readUInt16LE(bootOffset + 12), family === 'ESP32' ? 0 : 9);
  assert.equal(bytes.readUInt16LE(0x10000 + 12), family === 'ESP32' ? 0 : 9);
  fixtures[board] = { manifest, bytes };
}
const cards = [...html.matchAll(/name="board" value="([^"]+)"/g)].map(match => match[1]).sort();
assert.deepEqual(cards, Object.keys(expected).sort());
for (const match of html.matchAll(/(?:src|href)="([^"#]+)"/g)) {
  const resource = match[1].split('?')[0];
  if (!resource.includes('://')) assert(fs.statSync(path.join(root, resource)).isFile(), resource);
}
assert.deepEqual(fs.readFileSync(path.join(root, 'images/m5stack-drawing.png')).subarray(0, 8),
  Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]));

async function harness({ editManifest, corruptFirmware = false, secure = true, serial = true, failModule = false } = {}) {
  const elements = {};
  for (const id of ['installer', 'install-button', 'reset-consent', 'install-status', 'browser-warning', 'retry-check']) {
    elements[id] = {
      hidden: false, disabled: false, checked: false, dataset: {}, attributes: {}, handlers: {},
      addEventListener(name, callback) { this.handlers[name] = callback; },
      setAttribute(name, value) { this.attributes[name] = value; },
    };
  }
  let selected;
  const inputs = Object.keys(expected).map(value => ({ value, handlers: {},
    addEventListener(name, callback) { this.handlers[name] = callback; },
  }));
  const context = vm.createContext({
    document: {
      querySelector(selector) {
        return selector === 'input[name="board"]:checked'
          ? inputs.find(input => input.value === selected) : elements[selector.slice(1)];
      },
      querySelectorAll() { return inputs; },
    },
    window: { isSecureContext: secure }, navigator: serial ? { serial: {} } : {},
    location: { href: 'https://installer.test/index.html', origin: 'https://installer.test' },
    crypto: webcrypto, AbortController, URL, Uint8Array,
    async fetch(url) {
      const pathname = new URL(url).pathname;
      if (pathname.startsWith('/manifests/')) {
        const board = path.basename(pathname, '.json');
        const manifest = JSON.parse(JSON.stringify(fixtures[board].manifest));
        editManifest?.(manifest);
        return { ok: true, async json() { return manifest; } };
      }
      const board = path.basename(pathname, '.bin');
      const bytes = Buffer.from(fixtures[board].bytes);
      if (corruptFirmware) bytes[bytes.length - 1] ^= 1;
      return { ok: true, async arrayBuffer() {
        return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      } };
    },
  });
  const module = new vm.SourceTextModule(source, {
    context,
    async importModuleDynamically() {
      if (failModule) throw new Error('Installer module unavailable');
      const mock = new vm.SyntheticModule([], () => {}, { context });
      await mock.link(() => {});
      await mock.evaluate();
      return mock;
    },
  });
  await module.link(() => {});
  await module.evaluate();
  await new Promise(resolve => setImmediate(resolve));
  return {
    elements,
    async select(board) {
      selected = board;
      await inputs.find(input => input.value === board).handlers.change();
    },
    consent() {
      elements['reset-consent'].checked = true;
      elements['reset-consent'].handlers.change();
    },
  };
}

async function main() {
  const ready = await harness();
  assert(ready.elements['install-button'].disabled, 'A board must be selected first');
  for (const board of Object.keys(expected)) {
    await ready.select(board);
    assert(ready.elements['install-button'].disabled, 'Consent is required after selecting each board');
    assert.equal(ready.elements['reset-consent'].checked, false);
    assert.equal(ready.elements['retry-check'].hidden, true);
    assert.equal(ready.elements.installer.attributes.manifest, `https://installer.test/manifests/${board}.json`);
    ready.consent();
    assert.equal(ready.elements['install-button'].disabled, false, `${board} must be installable`);
  }
  for (const board of Object.keys(expected)) {
    const wrongChip = await harness({ editManifest(manifest) {
      manifest.builds[0].chipFamily = expected[board] === 'ESP32' ? 'ESP32-S3' : 'ESP32';
    } });
    await wrongChip.select(board);
    wrongChip.consent();
    assert(wrongChip.elements['install-button'].disabled, 'Wrong chip must never be installable');
    assert.equal(wrongChip.elements['retry-check'].hidden, false);
  }
  for (const options of [
    { corruptFirmware: true },
    { editManifest(manifest) { manifest.board = 'cores3'; } },
    { editManifest(manifest) { manifest.version = '0.0.0'; } },
    { editManifest(manifest) { manifest.size += 1; } },
    { editManifest(manifest) { manifest.builds[0].parts[0].offset = 0x1000; } },
    { editManifest(manifest) { manifest.builds[0].parts[0].path = 'https://other.test/m5stack.bin'; } },
    { failModule: true },
  ]) {
    const invalid = await harness(options);
    await invalid.select('m5stack');
    invalid.consent();
    assert(invalid.elements['install-button'].disabled, 'Invalid or unavailable firmware must be blocked');
    assert.equal(invalid.elements['install-status'].dataset.error, 'true');
  }
  for (const options of [{ secure: false }, { serial: false }]) {
    const unsupported = await harness(options);
    await unsupported.select('m5stack');
    unsupported.consent();
    assert(unsupported.elements.installer.hidden);
    assert(unsupported.elements['install-button'].disabled);
    assert.equal(unsupported.elements['browser-warning'].hidden, false);
  }
  console.log('PASS: all three browser images, chip IDs, checksums, page assets and board selection');
  console.log('PASS: consent reset, wrong chip, corrupt firmware, invalid manifests and unsupported browsers');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
