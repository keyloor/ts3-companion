"""Iconos del juego para la app de escritorio, leídos de la copia de Toy Story 3 instalada en este PC.

El Companion no reparte ninguna imagen del juego: las saca de assets/ui/<archivo>/<archivo>.zip del juego del
usuario. Cada icono es un .oct con una textura DDS RGBA de 32 bits sin comprimir, así que pasarlo a PNG es reordenar
bytes. Se convierte la primera vez que la interfaz lo pide y se guarda en %APPDATA%\\TS3Companion\\icons\\<sello>\\;
el sello cambia si cambian los .zip del juego, y entonces se vuelven a sacar.
"""
import glob, hashlib, os, re, shutil, struct, threading, zipfile, zlib

ARCHIVES = ('icons', 'alscards')     # assets/ui/<n>/<n>.zip
MAX_SIZE = 128                       # las cartas grandes (512) se sirven desde su mipmap de 128
NAME = re.compile(r'^[a-z0-9_]{1,64}$')


def find_game(steam, app):
    """Carpeta del juego en cualquier biblioteca de Steam, o None."""
    libs = [steam]
    try:
        vdf = open(os.path.join(steam, 'steamapps', 'libraryfolders.vdf'), encoding='utf-8', errors='replace').read()
        libs += [p.replace('\\\\', '\\') for p in re.findall(r'"path"\s+"([^"]+)"', vdf)]
    except OSError:
        pass
    for lib in libs:
        try:
            acf = open(os.path.join(lib, 'steamapps', 'appmanifest_%d.acf' % app), encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        m = re.search(r'"installdir"\s+"([^"]+)"', acf)
        game = m and os.path.join(lib, 'steamapps', 'common', m.group(1))
        if game and os.path.isdir(os.path.join(game, 'assets', 'ui')):
            return game
    return None


def dds_to_png(data):
    """PNG del mipmap más grande que quepa en MAX_SIZE. Solo DDS BGRA de 32 bits sin comprimir; si no, None."""
    i = data.find(b'DDS |\0\0\0')
    if i < 0 or len(data) < i + 128:
        return None
    h, w = struct.unpack_from('<2I', data, i + 12)
    mips = struct.unpack_from('<I', data, i + 28)[0]
    pf_flags, fourcc, bits, rm, gm, bm, am = struct.unpack_from('<7I', data, i + 80)
    if not pf_flags & 0x40 or bits != 32 or (rm, gm, bm, am) != (0xff0000, 0xff00, 0xff, 0xff000000):
        return None
    off = i + 128
    while max(w, h) > MAX_SIZE and mips > 1:
        off += w * h * 4; w, h, mips = max(w // 2, 1), max(h // 2, 1), mips - 1
    px = data[off:off + w * h * 4]
    if len(px) != w * h * 4:
        return None
    rgba = bytearray(len(px))
    rgba[0::4], rgba[1::4], rgba[2::4], rgba[3::4] = px[2::4], px[1::4], px[0::4], px[3::4]
    raw = b''.join(b'\0' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    chunk = lambda tag, body: struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', w, h, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


class GameIcons:
    def __init__(self, game, cache_root):
        self.game = game
        self.archives = [p for p in (os.path.join(game, 'assets', 'ui', n, n + '.zip') for n in ARCHIVES) if os.path.isfile(p)]
        stamp = hashlib.md5(repr([(p, os.path.getsize(p), int(os.path.getmtime(p))) for p in self.archives]).encode()).hexdigest()[:12]
        self.cache = os.path.join(cache_root, stamp)
        self.cache_root = cache_root
        self.lock = threading.Lock()
        self.index = None

    def _members(self):
        """{nombre: (zip, archivo dentro del zip)}, leído una vez."""
        if self.index is None:
            self.index = {}
            for p in self.archives:
                with zipfile.ZipFile(p) as z:
                    for m in z.namelist():
                        if m.lower().endswith('.oct'):
                            self.index.setdefault(os.path.basename(m)[:-4].lower(), (p, m))
        return self.index

    def png(self, name):
        """Bytes PNG del icono, o None si el juego no lo tiene."""
        if not NAME.match(name):
            return None
        path = os.path.join(self.cache, name + '.png')
        try:
            return open(path, 'rb').read()
        except OSError:
            pass
        with self.lock:
            where = self._members().get(name)
            if not where:
                return None
            with zipfile.ZipFile(where[0]) as z:
                png = dds_to_png(z.read(where[1]))
            if not png:
                return None
            if not os.path.isdir(self.cache):
                for old in glob.glob(os.path.join(self.cache_root, '*')):   # iconos de una versión anterior del juego
                    shutil.rmtree(old, ignore_errors=True)
                os.makedirs(self.cache, exist_ok=True)
            tmp = path + '.tmp'
            open(tmp, 'wb').write(png)
            os.replace(tmp, path)
            return png
