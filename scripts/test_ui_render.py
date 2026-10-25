"""Compile and exercise the production UI with the installed LVGL on a host GCC."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--env', default='lilygo-t-display-s3')
    parser.add_argument('--cxx', default=os.environ.get('CXX'))
    args = parser.parse_args()
    os.chdir(ROOT)
    cxx = args.cxx or shutil.which('g++')
    if not cxx:
        local = ROOT / '.pio/render-tools/toolchain-gccmingw32/bin/g++.exe'
        if local.exists():
            cxx = str(local)
    if not cxx:
        parser.error('Pass --cxx with a host GCC C++ compiler.')
    # Older MinGW cannot launch cc1 from a compiler installation in a Unicode path.
    if os.name == 'nt' and not str(cxx).isascii():
        compiler_root = Path(cxx).parent.parent
        cached_compiler = Path(tempfile.gettempdir()) / 't-display-render-mingw'
        if not (cached_compiler / 'bin/g++.exe').exists():
            shutil.copytree(compiler_root, cached_compiler, dirs_exist_ok=True)
        cxx = str(cached_compiler / 'bin/g++.exe')
    os.environ['PATH'] = str(Path(cxx).parent) + os.pathsep + os.environ.get('PATH', '')
    gcc_name = Path(cxx).name.replace('g++', 'gcc')
    cc = str(Path(cxx).with_name(gcc_name))
    lvgl = Path('.pio/libdeps') / args.env / 'lvgl'
    if not lvgl.exists():
        parser.error('Build the selected PlatformIO environment to install LVGL first.')
    output = Path('.pio/render-check') / args.env
    output.mkdir(parents=True, exist_ok=True)
    config = (ROOT / 'include/lv_conf.h').read_text(encoding='utf-8')
    # Rendering settings are unchanged. Host assertions abort instead of hanging forever.
    config = config.replace('#define LV_USE_LOG 0', '#define LV_USE_LOG 1')
    config = config.replace('#define LV_LOG_PRINTF 0', '#define LV_LOG_PRINTF 1')
    config = config.replace('#define LV_ASSERT_HANDLER while(1);', '#define LV_ASSERT_HANDLER abort();')
    config_path = output / 'lv_conf.h'
    if not config_path.exists() or config_path.read_text(encoding='utf-8') != config:
        config_path.write_text(config, encoding='utf-8')
    flags = ['-O1', '-w', '-DLV_CONF_INCLUDE_SIMPLE', '-DBOARD_TDISPLAY_S3',
             '-DLV_FONT_MONTSERRAT_12=1', '-DLV_FONT_MONTSERRAT_14=1', '-DLV_FONT_MONTSERRAT_24=1',
             '-I' + str(output), '-Itest/render_support', '-Isrc', '-Iinclude', '-I' + str(lvgl),
             '-include', 'stdlib.h', '-finput-charset=UTF-8', '-fexec-charset=UTF-8']
    library_headers = list(lvgl.rglob('*.h')) + [config_path]
    c_stamp = max(path.stat().st_mtime for path in library_headers)
    cpp_stamp = max(c_stamp, *(path.stat().st_mtime for path in Path('src').glob('*.h')),
                    Path('test/render_support/Arduino.h').stat().st_mtime)
    sources = list((lvgl / 'src').rglob('*.c')) + list(Path('src/fonts').glob('*.c'))
    sources += [Path('src') / name for name in
                ('ui_main.cpp', 'ui_clock.cpp', 'ui_ns_view.cpp', 'ui_ns_cache.cpp', 'ui_navigation.cpp')]
    sources.append(Path('test/test_ui_render.cpp'))

    def compile_source(source):
        obj = output / (hashlib.sha256(str(source).encode()).hexdigest()[:16] + '.o')
        stamp = cpp_stamp if source.suffix == '.cpp' else c_stamp
        if not obj.exists() or obj.stat().st_mtime < max(stamp, source.stat().st_mtime):
            compiler = cxx if source.suffix == '.cpp' else cc
            standard = '-std=gnu++14' if source.suffix == '.cpp' else '-std=gnu11'
            test_flags = ['-DUI_RENDER_HOST'] if source.name == 'test_ui_render.cpp' else []
            subprocess.run([compiler, standard, *flags, *test_flags, '-c', str(source), '-o', str(obj)], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return obj.as_posix()

    print('Compiling production UI and LVGL:', len(sources), 'sources', flush=True)
    with ThreadPoolExecutor(max_workers=8) as pool:
        objects = list(pool.map(compile_source, sources))
    response = output / 'objects.txt'
    response.write_text('\n'.join(objects), encoding='ascii')
    executable = output / 'ui-render.exe'
    subprocess.run([cxx, '@' + str(response), '-static', '-o', str(executable)], check=True)
    print('Rendering real screens with production fonts and animations', flush=True)
    subprocess.run([str(executable), str(output)], check=True, timeout=180)
    print('Repeating with 64 KiB reserved for inactive UI', flush=True)
    subprocess.run([str(executable), str(output), str(64 * 1024)], check=True, timeout=180)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        print('Failed:', error.cmd, file=sys.stderr)
        if error.stdout:
            sys.stderr.write(error.stdout.decode('utf-8', errors='replace'))
        if error.stderr:
            sys.stderr.write(error.stderr.decode('utf-8', errors='replace'))
        raise SystemExit(error.returncode)
