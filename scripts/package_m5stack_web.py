"""Package the verified M5Stack build for the browser installer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='1.0.3')
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        parser.error('Version must have the form X.Y.Z.')
    os.chdir(ROOT)
    if subprocess.check_output(['git', 'status', '--porcelain', '--', 'src', 'include', 'platformio.ini']):
        parser.error('Build and commit firmware source changes before packaging.')
    source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    build = ROOT / '.pio/build/m5stack'
    info = json.loads((build / 'idedata.json').read_text(encoding='utf-8'))
    assert info['env_name'] == 'm5stack'
    assert info['extra']['application_offset'] == '0x10000'
    assert [int(item['offset'], 0) for item in info['extra']['flash_images']] == [0x1000, 0x8000, 0xE000]
    pio = Path(os.environ.get('PLATFORMIO_CORE_DIR', str(Path.home() / '.platformio')))
    python = pio / ('penv/Scripts/python.exe' if os.name == 'nt' else 'penv/bin/python')
    esptool = pio / 'packages/tool-esptoolpy/esptool.py'
    boot_app = pio / 'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    parts = [(0x1000, build / 'bootloader.bin'), (0x8000, build / 'partitions.bin'),
             (0xE000, boot_app), (0x10000, build / 'firmware.bin')]
    app = parts[-1][1].read_bytes()
    boot = parts[0][1].read_bytes()
    for image in (boot, app):
        assert image[0] == 0xE9 and int.from_bytes(image[12:14], 'little') == 0, 'Expected ESP32 image'
        assert image[2:4] == b'\x02\x2f', 'Expected DIO, 80 MHz, 4 MB flash'
    assert app[0xB0:0xD0] == hashlib.sha256((build / 'firmware.elf').read_bytes()).digest()
    app_partition = None
    table = parts[1][1].read_bytes()
    for offset in range(0, len(table), 32):
        entry = table[offset:offset + 32]
        if entry[:2] != b'\xaa\x50':
            break
        _, kind, subtype, address, length, _, _ = struct.unpack('<HBBII16sI', entry)
        if kind == 0 and subtype == 0x10:
            app_partition = (address, length)
    assert app_partition == (0x10000, 0x300000)
    assert len(app) <= app_partition[1]
    target = ROOT / f'docs/firmware/v{args.version}/m5stack.bin'
    target.parent.mkdir(parents=True, exist_ok=True)
    command = [str(python), str(esptool), '--chip', 'esp32', 'merge_bin',
               '--flash_mode', 'dio', '--flash_freq', '80m', '--flash_size', '4MB', '-o', str(target)]
    for address, path in parts:
        command.extend([hex(address), str(path)])
    subprocess.run(command, check=True)
    merged = target.read_bytes()
    for address, path in parts:
        assert merged[address:address + path.stat().st_size] == path.read_bytes()
    assert merged[:0x1000] == b'\xff' * 0x1000
    assert merged[0x9000:0xE000] == b'\xff' * 0x5000
    assert len(merged) == 0x10000 + len(app) <= 4 * 1024 * 1024
    manifest = {
        'name': 'Nightscout Monitor - M5Stack Core Basic / Gray',
        'version': args.version,
        'board': 'm5stack',
        'new_install_prompt_erase': False,
        'new_install_improv_wait_time': 0,
        'builds': [{'chipFamily': 'ESP32', 'parts': [{'path': f'../firmware/v{args.version}/m5stack.bin', 'offset': 0}]}],
        'sha256': hashlib.sha256(merged).hexdigest(),
        'size': len(merged),
        'source_commit': source,
    }
    (ROOT / 'docs/manifests/m5stack.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'M5Stack browser image verified: {len(merged)} bytes, SHA-256 {manifest["sha256"]}')


if __name__ == '__main__':
    main()
