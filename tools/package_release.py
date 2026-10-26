"""Empaqueta build/dist/TS3Companion.exe en build/release/TS3Companion-<versión>.zip (para GitHub Releases y Nexus Mods)."""
import os, re, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = re.search(r"VERSION = '([^']+)'", open(os.path.join(ROOT, 'desktop', 'companion.py'), encoding='utf-8').read()).group(1)
OUT = os.path.join(ROOT, 'build', 'release')
os.makedirs(OUT, exist_ok=True)
zip_path = os.path.join(OUT, 'TS3Companion-%s.zip' % VERSION)
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as z:
    z.write(os.path.join(ROOT, 'build', 'dist', 'TS3Companion.exe'), 'TS3Companion/TS3Companion.exe')
    z.write(os.path.join(ROOT, 'desktop', 'README.txt'), 'TS3Companion/README.txt')
    z.write(os.path.join(ROOT, 'LICENSE'), 'TS3Companion/LICENSE.txt')
    z.write(os.path.join(ROOT, 'app', 'CREDITS.md'), 'TS3Companion/CREDITS.md')
    z.write(os.path.join(ROOT, 'CHANGELOG.md'), 'TS3Companion/CHANGELOG.md')
print(zip_path, os.path.getsize(zip_path))
