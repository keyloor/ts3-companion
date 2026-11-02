"""Instala, quita y pone al día el overlay (overlay/, un dxgi.dll) en la carpeta del juego.

En el juego quedan `dxgi.dll` y la carpeta `TS3Companion Overlay` (content.json y savemap.json de esta versión de la
app, overlay.ini y overlay.log). Nunca se toca un dxgi.dll que no sea el nuestro (por ejemplo, el de ReShade). Con el juego
abierto Windows no deja cambiar la DLL: entonces se devuelve 'in-use'. Las primeras pruebas usaban xinput1_4.dll (chocaba
con el overlay de Steam): si queda uno nuestro, se borra.
"""
import os, shutil

DLL = 'dxgi.dll'
OLD_DLL = 'xinput1_4.dll'
DATA = 'TS3Companion Overlay'
MARK = b'TS3 Companion Overlay'          # texto que lleva nuestra DLL (overlay/src/main.cpp)
DATA_FILES = ('content.json', 'savemap.json')


def _read(path):
    try:
        return open(path, 'rb').read()
    except OSError:
        return None


def is_ours(path):
    data = _read(path)
    return data is not None and MARK in data


def status(game, dll, data_dir):
    """Estado para la interfaz. game: carpeta del juego o None; dll: la DLL que trae la app; data_dir: app/data."""
    out = {'available': bool(dll and os.path.isfile(dll)), 'game': bool(game), 'installed': False, 'foreign': False, 'current': False}
    if not game:
        return out
    target = os.path.join(game, DLL)
    if os.path.isfile(target):
        out['installed'] = is_ours(target)
        out['foreign'] = not out['installed']
    if out['installed'] and out['available']:
        out['current'] = _read(target) == _read(dll) and all(
            _read(os.path.join(game, DATA, f)) == _read(os.path.join(data_dir, f)) for f in DATA_FILES)
    return out


def _remove_old(game):
    old = os.path.join(game, OLD_DLL)
    if is_ours(old):
        try:
            os.remove(old)
        except OSError:
            pass


def _copy_data(game, data_dir):
    d = os.path.join(game, DATA)
    os.makedirs(d, exist_ok=True)
    for f in DATA_FILES:
        src, dst = os.path.join(data_dir, f), os.path.join(d, f)
        if _read(dst) != _read(src):
            shutil.copyfile(src, dst)
    return d


def install(game, dll, data_dir, lang):
    """'ok', 'no-game', 'unavailable', 'foreign' o 'in-use'."""
    if not game:
        return 'no-game'
    if not dll or not os.path.isfile(dll):
        return 'unavailable'
    target = os.path.join(game, DLL)
    if os.path.isfile(target) and not is_ours(target):
        return 'foreign'
    if _read(target) != _read(dll):
        tmp = target + '.new'
        shutil.copyfile(dll, tmp)
        try:
            os.replace(tmp, target)
        except PermissionError:
            os.remove(tmp)
            return 'in-use'
    _remove_old(game)
    d = _copy_data(game, data_dir)
    ini = os.path.join(d, 'overlay.ini')
    if not os.path.isfile(ini):
        # Mismo formato que crea la DLL, con el idioma de la app
        with open(ini, 'w', encoding='ascii') as f:
            f.write('; TS3 Companion Overlay\n'
                    '; Hotkey: F1..F12.  PadButton: DPadDown, DPadUp, DPadLeft, DPadRight, Back, Start, LeftThumb, RightThumb.\n'
                    '; Language: auto, es, en.  Theme: auto, light, dark.  Buttons: xbox, ps.  Scale: 1.0 = normal size.\n'
                    "; SavePath: empty = the Companion's save, or the standard location.\n"
                    '[Overlay]\nHotkey=F2\nPadButton=DPadDown\nLanguage=%s\nTheme=auto\nButtons=xbox\nScale=1.0\nSavePath=\n' % ('es' if lang == 'es' else 'en'))
    return 'ok'


def uninstall(game):
    """'ok', 'no-game', 'foreign' o 'in-use'."""
    if not game:
        return 'no-game'
    _remove_old(game)
    target = os.path.join(game, DLL)
    if os.path.isfile(target):
        if not is_ours(target):
            return 'foreign'
        try:
            os.remove(target)
        except PermissionError:
            return 'in-use'
    shutil.rmtree(os.path.join(game, DATA), ignore_errors=True)
    return 'ok'


def sync(game, dll, data_dir):
    """Al abrir la app: si el overlay está instalado, lo pone al día con esta versión (la DLL solo con el juego cerrado)."""
    if game:
        _remove_old(game)
    if not game or not is_ours(os.path.join(game, DLL)):
        return
    try:
        _copy_data(game, data_dir)
        if dll and os.path.isfile(dll) and _read(os.path.join(game, DLL)) != _read(dll):
            install(game, dll, data_dir, 'en')
    except OSError:
        pass
