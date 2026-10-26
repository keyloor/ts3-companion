"""Toy Story 3 Companion (escritorio).

Sirve la interfaz web en http://127.0.0.1:47317 y la abre en una ventana propia (Edge o Chrome en modo app, sin
barras del navegador). Encuentra la partida del juego y la expone a la interfaz para sincronizar con un botón o
automáticamente al guardar en el juego. También lee los logros desbloqueados de la caché local de Steam.
Solo lee; nunca modifica la partida, el juego ni Steam. Se cierra al cerrar la ventana.
El progreso se guarda en %APPDATA%\\TS3Companion\\progress.json.

Uso: TS3Companion.exe [--save RUTA] [--port N] [--browser] [--no-window]
"""
import argparse, glob, http.server, json, os, socketserver, struct, subprocess, sys, threading, time, urllib.request, webbrowser
from urllib.parse import parse_qs, urlparse

APP = 'ts3-companion'
VERSION = '1.0.0'
BASE = getattr(sys, '_MEIPASS', os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WEB = os.path.join(BASE, 'app')
DATA_DIR = os.path.join(os.environ.get('APPDATA', os.path.expanduser('~')), 'TS3Companion')
CONFIG = os.path.join(DATA_DIR, 'config.json')
PROGRESS = os.path.join(DATA_DIR, 'progress.json')
PROFILE = os.path.join(DATA_DIR, 'window-profile')
MAX_BODY = 512 * 1024
DEFAULT_PORT = 47317
IDLE_LIMIT = 120          # segundos sin señal de la interfaz antes de cerrarse (modo navegador)
STEAM_APP = 4049180       # Toy Story 3 Complete Edition

# Sin consola (exe con ventana) no hay stdout: los mensajes van a un log
if sys.stdout is None or sys.stderr is None:
    os.makedirs(DATA_DIR, exist_ok=True)
    sys.stdout = sys.stderr = open(os.path.join(DATA_DIR, 'log.txt'), 'w', encoding='utf-8', buffering=1)


def load_config():
    try:
        return json.load(open(CONFIG, encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def save_config(cfg):
    os.makedirs(DATA_DIR, exist_ok=True)
    tmp = CONFIG + '.tmp'
    json.dump(cfg, open(tmp, 'w', encoding='utf-8'), indent=1)
    os.replace(tmp, CONFIG)


def is_save(path):
    """Las partidas de Toy Story 3 llevan la marca CMP1 en el byte 4."""
    try:
        with open(path, 'rb') as f:
            return f.read(8)[4:8] == b'CMP1'
    except OSError:
        return False


def pick_file(title):
    """Diálogo «Abrir» de Windows. Devuelve la ruta elegida o None si se cancela."""
    import ctypes
    from ctypes import wintypes

    class OPENFILENAMEW(ctypes.Structure):
        _fields_ = [('lStructSize', wintypes.DWORD), ('hwndOwner', wintypes.HWND), ('hInstance', wintypes.HINSTANCE),
                    ('lpstrFilter', wintypes.LPCWSTR), ('lpstrCustomFilter', wintypes.LPWSTR), ('nMaxCustFilter', wintypes.DWORD),
                    ('nFilterIndex', wintypes.DWORD), ('lpstrFile', wintypes.LPWSTR), ('nMaxFile', wintypes.DWORD),
                    ('lpstrFileTitle', wintypes.LPWSTR), ('nMaxFileTitle', wintypes.DWORD), ('lpstrInitialDir', wintypes.LPCWSTR),
                    ('lpstrTitle', wintypes.LPCWSTR), ('Flags', wintypes.DWORD), ('nFileOffset', wintypes.WORD),
                    ('nFileExtension', wintypes.WORD), ('lpstrDefExt', wintypes.LPCWSTR), ('lCustData', wintypes.LPARAM),
                    ('lpfnHook', ctypes.c_void_p), ('lpTemplateName', wintypes.LPCWSTR), ('pvReserved', ctypes.c_void_p),
                    ('dwReserved', wintypes.DWORD), ('FlagsEx', wintypes.DWORD)]

    buf = ctypes.create_unicode_buffer(1024)
    start = os.path.join(os.environ.get('APPDATA', ''), 'Disney')
    ofn = OPENFILENAMEW()
    ofn.lStructSize = ctypes.sizeof(OPENFILENAMEW)
    ofn.hwndOwner = ctypes.windll.user32.GetForegroundWindow()   # la ventana de la app: el diálogo queda encima
    ofn.lpstrFilter = 'Saved Game\0Saved Game*\0*.*\0*.*\0\0'
    ofn.lpstrFile = ctypes.cast(buf, wintypes.LPWSTR)
    ofn.nMaxFile = len(buf)
    ofn.lpstrInitialDir = start if os.path.isdir(start) else None
    ofn.lpstrTitle = title
    ofn.Flags = 0x00001000 | 0x00000800 | 0x00000008 | 0x00000004   # FILEMUSTEXIST, PATHMUSTEXIST, NOCHANGEDIR, HIDEREADONLY
    return buf.value if ctypes.windll.comdlg32.GetOpenFileNameW(ctypes.byref(ofn)) else None


def find_save(explicit=None):
    """Devuelve la ruta de la partida: la indicada, la de configuración o la ubicación estándar del juego."""
    if explicit and os.path.isfile(explicit):
        return explicit
    p = load_config().get('save')
    if p and os.path.isfile(p):
        return p
    appdata = os.environ.get('APPDATA', '')
    candidates = [os.path.join(appdata, 'Disney', 'Toy Story 3 Complete Edition', 'savedata', 'Saved Game')]
    candidates += glob.glob(os.path.join(appdata, 'Disney', '*', 'savedata', 'Saved Game'))
    candidates += glob.glob(os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Disney', '*', 'savedata', 'Saved Game'))
    return next((c for c in candidates if os.path.isfile(c)), None)


def read_kv(data, i=0):
    """KeyValues binario de Steam (appcache/stats): 0 sección, 1 texto, 2/4/6 entero, 3 float, 7 uint64, 8 fin."""
    out = {}
    while i < len(data):
        t = data[i]; i += 1
        if t == 8:
            return out, i
        j = data.index(b'\0', i); key = data[i:j].decode('utf-8', 'replace'); i = j + 1
        if t == 0:
            v, i = read_kv(data, i)
        elif t == 1:
            j = data.index(b'\0', i); v = data[i:j].decode('utf-8', 'replace'); i = j + 1
        elif t in (2, 4, 6):
            v = struct.unpack_from('<i', data, i)[0]; i += 4
        elif t == 3:
            v = struct.unpack_from('<f', data, i)[0]; i += 4
        elif t == 7:
            v = struct.unpack_from('<Q', data, i)[0]; i += 8
        else:
            raise ValueError('kv type %d' % t)
        out[key] = v
    return out, i


def steam_files():
    """Esquema y estadísticas de logros del usuario de Steam con sesión iniciada (o el más reciente)."""
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam') as k:
            steam = winreg.QueryValueEx(k, 'SteamPath')[0]
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam\ActiveProcess') as k:
                active = winreg.QueryValueEx(k, 'ActiveUser')[0]
        except OSError:
            active = 0
    except (ImportError, OSError):
        return None, None
    stats = os.path.join(steam, 'appcache', 'stats')
    schema = os.path.join(stats, 'UserGameStatsSchema_%d.bin' % STEAM_APP)
    mine = os.path.join(stats, 'UserGameStats_%d_%d.bin' % (active, STEAM_APP))
    users = glob.glob(os.path.join(stats, 'UserGameStats_*_%d.bin' % STEAM_APP))
    user = mine if os.path.isfile(mine) else max(users, key=os.path.getmtime, default=None)
    return (schema, user) if user and os.path.isfile(schema) else (None, None)


def steam_achievements():
    """Nombres en inglés de los logros desbloqueados según la caché local de Steam."""
    schema, user = steam_files()
    if not user:
        return None
    sch = next(iter(read_kv(open(schema, 'rb').read())[0].values()), {})
    usr = read_kv(open(user, 'rb').read())[0].get('cache', {})
    got = []
    for sid, block in sch.get('stats', {}).items():
        data = usr.get(sid, {}).get('data', 0) if isinstance(usr.get(sid), dict) else 0
        for bit, a in block.get('bits', {}).items():
            if data >> int(bit) & 1:
                got.append(a['display']['name']['english'])
    return got


def find_app_browser():
    """Edge (viene con Windows) o Chrome, para abrir la interfaz en modo app."""
    roots = [os.environ.get(k, '') for k in ('ProgramFiles(x86)', 'ProgramFiles', 'LOCALAPPDATA')]
    rels = [r'Microsoft\Edge\Application\msedge.exe', r'Google\Chrome\Application\chrome.exe']
    return next((os.path.join(r, p) for p in rels for r in roots if r and os.path.isfile(os.path.join(r, p))), None)


class Handler(http.server.SimpleHTTPRequestHandler):
    save_path = None
    explicit = None
    last_ping = None

    def __init__(self, *a, **kw):
        super().__init__(*a, directory=WEB, **kw)

    def log_message(self, *a):
        pass

    def send_json(self, obj, code=200):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def host_ok(self):
        """Solo acepta peticiones dirigidas a esta máquina (evita ataques de DNS rebinding desde otras webs)."""
        port = self.server.server_address[1]
        if self.headers.get('Host', '') in ('127.0.0.1:%d' % port, 'localhost:%d' % port):
            return True
        self.send_json({'error': 'bad-host'}, 403)
        return False

    def do_GET(self):
        if not self.host_ok():
            return
        path = urlparse(self.path).path
        if path.startswith('/api/'):
            Handler.last_ping = time.time()
        if path == '/api/status':
            Handler.save_path = find_save(Handler.explicit)
            p = Handler.save_path
            st = os.stat(p) if p else None
            steam = steam_files()[1]
            return self.send_json({'app': APP, 'version': VERSION, 'found': bool(p), 'path': p,
                                   'custom': bool(load_config().get('save')),
                                   'mtime': st.st_mtime if st else None, 'size': st.st_size if st else None,
                                   'steam': bool(steam), 'steamMtime': os.path.getmtime(steam) if steam else None})
        if path == '/api/steam':
            try:
                got = steam_achievements()
            except (OSError, ValueError, KeyError, struct.error):
                got = None
            return self.send_json({'found': got is not None, 'unlocked': got or []})
        if path == '/api/ping':
            return self.send_json({'ok': True})
        if path == '/api/open':
            # Abre un enlace de la interfaz en el navegador normal; solo la propia app y sitios conocidos
            u = (parse_qs(urlparse(self.path).query).get('u') or [''])[0]
            own = 'http://127.0.0.1:%d/' % self.server.server_address[1]
            if not u.startswith((own, 'https://github.com/', 'https://www.nexusmods.com/')):
                return self.send_json({'error': 'not-allowed'}, 403)
            webbrowser.open(u)
            return self.send_json({'ok': True})
        if path == '/api/save':
            p = Handler.save_path or find_save(Handler.explicit)
            if not p:
                return self.send_json({'error': 'save-not-found'}, 404)
            data = open(p, 'rb').read()
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if path == '/api/progress':
            try:
                return self.send_json(json.load(open(PROGRESS, encoding='utf-8')))
            except (OSError, ValueError):
                return self.send_json({'checked': []})
        return super().do_GET()

    def do_POST(self):
        # Solo la propia interfaz: una web ajena no puede abrir el diálogo (su Origin sería otro)
        port = self.server.server_address[1]
        if not self.host_ok():
            return
        if self.headers.get('Origin') not in ('http://127.0.0.1:%d' % port, 'http://localhost:%d' % port):
            return self.send_json({'error': 'bad-origin'}, 403)
        path = urlparse(self.path).path
        if path == '/api/pick-save':
            chosen = pick_file('Toy Story 3 - Saved Game')
            if not chosen:
                return self.send_json({'cancelled': True})
            if not is_save(chosen):
                return self.send_json({'error': 'not-a-save'}, 400)
            cfg = load_config(); cfg['save'] = chosen; save_config(cfg)
            return self.send_json({'ok': True, 'path': chosen})
        if path == '/api/reset-save':
            cfg = load_config(); cfg.pop('save', None); save_config(cfg)
            return self.send_json({'ok': True})
        return self.send_json({'error': 'not-found'}, 404)

    def do_PUT(self):
        if not self.host_ok():
            return
        if urlparse(self.path).path != '/api/progress':
            return self.send_json({'error': 'not-found'}, 404)
        Handler.last_ping = time.time()
        n = int(self.headers.get('Content-Length') or 0)
        if n <= 0 or n > MAX_BODY:
            return self.send_json({'error': 'bad-size'}, 400)
        try:
            body = json.loads(self.rfile.read(n))
            ids = [str(x) for x in body.get('checked', [])][:5000]
        except (ValueError, AttributeError):
            return self.send_json({'error': 'bad-json'}, 400)
        os.makedirs(DATA_DIR, exist_ok=True)
        tmp = PROGRESS + '.tmp'
        json.dump({'checked': ids, 'updated': body.get('updated')}, open(tmp, 'w', encoding='utf-8'))
        os.replace(tmp, PROGRESS)
        return self.send_json({'ok': True})


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = False


def running_instance(port):
    """True si ya hay un Companion escuchando en ese puerto."""
    try:
        with urllib.request.urlopen('http://127.0.0.1:%d/api/status' % port, timeout=1) as r:
            return json.load(r).get('app') == APP
    except Exception:
        return False


def open_window(url, use_browser):
    """Abre la interfaz en una ventana propia. Devuelve el proceso si se usó el modo app."""
    exe = None if use_browser else find_app_browser()
    if not exe:
        webbrowser.open(url)
        return None
    os.makedirs(PROFILE, exist_ok=True)
    args = [exe, '--app=' + url, '--user-data-dir=' + PROFILE, '--window-size=1180,880',
            '--no-first-run', '--no-default-browser-check', '--disable-features=Translate']
    flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
    return subprocess.Popen(args, creationflags=flags)


def watch_and_exit(server, proc):
    """Cierra el servidor cuando se cierra la ventana (o, en modo navegador, cuando la interfaz deja de dar señales)."""
    started = time.time()
    if proc is not None:
        proc.wait()
        # Si el proceso terminó al instante, Edge pasó la ventana a otra instancia: vigilar por señales
        if time.time() - started > 5:
            return server.shutdown()
    while True:
        time.sleep(5)
        last = Handler.last_ping
        if last and time.time() - last > IDLE_LIMIT:
            return server.shutdown()
        if not last and time.time() - started > 300:
            return server.shutdown()


def main():
    ap = argparse.ArgumentParser(description='Toy Story 3 Companion')
    ap.add_argument('--save', help='ruta de la partida (Saved Game)')
    ap.add_argument('--port', type=int, default=DEFAULT_PORT)
    ap.add_argument('--browser', action='store_true', help='abrir en el navegador normal en vez de en ventana propia')
    ap.add_argument('--no-window', '--no-browser', dest='no_window', action='store_true', help='solo servidor, sin abrir nada (y sin cierre automático)')
    args = ap.parse_args()
    Handler.explicit = args.save

    # Una sola instancia: si ya está abierto, solo se abre otra ventana
    if running_instance(args.port):
        if not args.no_window:
            open_window('http://127.0.0.1:%d/' % args.port, args.browser)
        return 0

    server = None
    for port in range(args.port, args.port + 10):
        try:
            server = Server(('127.0.0.1', port), Handler)
            break
        except OSError:
            continue
    if not server:
        print('No hay puertos libres / no free port')
        return 1
    url = 'http://127.0.0.1:%d/' % server.server_address[1]
    print('Toy Story 3 Companion', VERSION, url)
    print('Partida / Save:', find_save(args.save) or 'no encontrada / not found')
    if not args.no_window:
        proc = open_window(url, args.browser)
        threading.Thread(target=watch_and_exit, args=(server, proc), daemon=True).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == '__main__':
    sys.exit(main())
