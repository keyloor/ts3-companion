# PyInstaller: python -m PyInstaller desktop/TS3Companion.spec  (desde la raíz del proyecto)
import os
ROOT = os.path.abspath(os.path.join(SPECPATH, '..'))

a = Analysis([os.path.join(ROOT, 'desktop', 'companion.py')],
             pathex=[ROOT],
             datas=[(os.path.join(ROOT, 'app'), 'app')],
             excludes=['tkinter', 'unittest', 'pydoc', 'email.test'],
             noarchive=False)
pyz = PYZ(a.pure)
exe = EXE(pyz, a.scripts, a.binaries, a.datas, [],
          name='TS3Companion',
          console=False,
          icon=os.path.join(ROOT, 'desktop', 'icon.ico'),
          upx=False,
          version=os.path.join(ROOT, 'desktop', 'version.txt'))
