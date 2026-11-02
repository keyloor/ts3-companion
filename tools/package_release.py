"""Empaqueta las dos descargas (para GitHub Releases y Nexus Mods) en build/release, o en la carpeta que se pase:

TS3Companion-<versión>.zip         la app de escritorio (build/dist/TS3Companion.exe); va donde quieras
TS3Companion-InGame-<versión>.zip  solo el panel dentro del juego (build/overlay, de overlay/build.py); se descomprime
                                   en la carpeta del juego y no necesita la app
"""
import os, re, sys, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = re.search(r"VERSION = '([^']+)'", open(os.path.join(ROOT, 'desktop', 'companion.py'), encoding='utf-8').read()).group(1)
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'release')
os.makedirs(OUT, exist_ok=True)

zip_path = os.path.join(OUT, 'TS3Companion-%s.zip' % VERSION)
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as z:
    z.write(os.path.join(ROOT, 'build', 'dist', 'TS3Companion.exe'), 'TS3Companion/TS3Companion.exe')
    z.write(os.path.join(ROOT, 'desktop', 'README.txt'), 'TS3Companion/README.txt')
    z.write(os.path.join(ROOT, 'LICENSE'), 'TS3Companion/LICENSE.txt')
    z.write(os.path.join(ROOT, 'app', 'CREDITS.md'), 'TS3Companion/CREDITS.md')
    z.write(os.path.join(ROOT, 'CHANGELOG.md'), 'TS3Companion/CHANGELOG.md')
print(zip_path, os.path.getsize(zip_path))

# Sin carpeta raíz y con los textos dentro de la del overlay: en el juego solo quedan dxgi.dll y esa carpeta.
# Los archivos se nombran uno a uno: build/overlay también guarda el overlay.ini y el registro de las pruebas.
DATA = 'TS3Companion Overlay'
zip_path = os.path.join(OUT, 'TS3Companion-InGame-%s.zip' % VERSION)
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as z:
    z.write(os.path.join(ROOT, 'build', 'overlay', 'dxgi.dll'), 'dxgi.dll')
    z.write(os.path.join(ROOT, 'app', 'data', 'content.json'), DATA + '/content.json')
    z.write(os.path.join(ROOT, 'app', 'data', 'savemap.json'), DATA + '/savemap.json')
    z.write(os.path.join(ROOT, 'overlay', 'README.txt'), DATA + '/README.txt')
    z.write(os.path.join(ROOT, 'LICENSE'), DATA + '/LICENSE.txt')
    z.write(os.path.join(ROOT, 'app', 'CREDITS.md'), DATA + '/CREDITS.md')
    z.write(os.path.join(ROOT, 'CHANGELOG.md'), DATA + '/CHANGELOG.md')
    # Las fuentes de la app van dentro de la DLL: su licencia va con ellas
    for f in ('OFL-LilitaOne.txt', 'OFL-Nunito.txt'):
        z.write(os.path.join(ROOT, 'app', 'fonts', f), DATA + '/' + f)
print(zip_path, os.path.getsize(zip_path))
