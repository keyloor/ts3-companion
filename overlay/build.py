"""Compila el overlay: build/overlay/dxgi.dll + build/overlay/TS3Companion Overlay/ (datos del Companion).

Uso (desde companion/): python overlay/build.py [--test]
Necesita llvm-mingw (x86_64, UCRT): la carpeta en la variable TS3_LLVM_MINGW, o una llvm-mingw-* en ../overlay-deps.
--test compila también build/overlay/progress_test.exe (lee la partida y lo imprime, para compararlo con sync_cli.py).
Antes: python tools/build_content.py, para que los datos estén al día.
"""
import glob, os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, 'build', 'overlay')
IMGUI = os.path.join(HERE, 'third_party', 'imgui')


def compiler():
    base = os.environ.get('TS3_LLVM_MINGW') or next(iter(sorted(glob.glob(os.path.join(ROOT, '..', 'overlay-deps', 'llvm-mingw-*-x86_64')))), '')
    cxx = os.path.join(base, 'bin', 'x86_64-w64-mingw32-clang++.exe')
    if not os.path.isfile(cxx):
        sys.exit('llvm-mingw not found: set TS3_LLVM_MINGW')
    return cxx


def embed_fonts():
    """overlay/fonts/*.ttf como datos de C++, con la altura sobre y bajo la línea base (en em) de cada una."""
    import struct
    out = ['struct EmbeddedFont { const unsigned char *data; int size; float ascent, descent; };']
    table = []
    for i, name in enumerate(('nunito-500', 'nunito-700', 'nunito-800', 'display', 'pad')):
        d = open(os.path.join(HERE, 'fonts', name + '.ttf'), 'rb').read()
        tables = {d[12 + 16 * k:16 + 16 * k]: struct.unpack('>I', d[20 + 16 * k:24 + 16 * k])[0] for k in range(struct.unpack('>H', d[4:6])[0])}
        upm = struct.unpack('>H', d[tables[b'head'] + 18:tables[b'head'] + 20])[0]
        asc, desc = struct.unpack('>hh', d[tables[b'hhea'] + 4:tables[b'hhea'] + 8])
        out.append('static const unsigned char font%d[] = {%s};' % (i, ','.join(map(str, d))))
        table.append('{font%d, %d, %.4ff, %.4ff}' % (i, len(d), asc / upm, -desc / upm))
    out.append('extern const EmbeddedFont embeddedFonts[] = {%s};' % ', '.join(table))
    path = os.path.join(OUT, 'fonts_data.cpp')
    open(path, 'w').write('\n'.join(out) + '\n')
    return path


def main():
    cxx = compiler()
    os.makedirs(OUT, exist_ok=True)
    flags = ['-std=c++17', '-O2', '-DNDEBUG', '-DUNICODE', '-D_UNICODE', '-I', IMGUI, '-I', os.path.join(IMGUI, 'backends'),
             '-Wall', '-Wno-unused-function', '-static']
    src = [os.path.join(HERE, 'src', f) for f in ('main.cpp', 'd3d12.cpp', 'ui.cpp', 'progress.cpp', 'gamefiles.cpp')]
    src += [os.path.join(IMGUI, f) for f in ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp')]
    src += [os.path.join(IMGUI, 'backends', 'imgui_impl_dx12.cpp'), embed_fonts()]
    # Sin -ldxgi: la DLL es dxgi.dll y sus propias exportaciones llevan a la de Windows (src/main.cpp)
    dll = os.path.join(OUT, 'dxgi.dll')
    cmd = [cxx, *flags, '-shared', '-o', dll, *src, os.path.join(HERE, 'src', 'dxgi.def'),
           '-ld3d12', '-ld3dcompiler_47', '-ldxguid', '-luser32', '-lgdi32', '-ladvapi32', '-Wl,--kill-at']
    subprocess.run(cmd, check=True)
    print(dll, os.path.getsize(dll))
    old = os.path.join(OUT, 'xinput1_4.dll')   # versión anterior del overlay
    if os.path.isfile(old):
        os.remove(old)
    data = os.path.join(OUT, 'TS3Companion Overlay')
    os.makedirs(data, exist_ok=True)
    for f in ('content.json', 'savemap.json'):
        shutil.copyfile(os.path.join(ROOT, 'app', 'data', f), os.path.join(data, f))
    if '--test' in sys.argv:
        exe = os.path.join(OUT, 'progress_test.exe')
        subprocess.run([cxx, *flags, '-municode', '-o', exe, os.path.join(HERE, 'tests', 'progress_test.cpp'),
                        os.path.join(HERE, 'src', 'progress.cpp'), '-ladvapi32'], check=True)
        print(exe)


if __name__ == '__main__':
    main()
