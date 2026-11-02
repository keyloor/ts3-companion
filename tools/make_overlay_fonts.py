"""Fuentes del overlay (overlay/fonts/*.ttf, en el repositorio; overlay/build.py las mete en la DLL).

Dear ImGui lee TTF, no WOFF2: convierte las fuentes de la app (app/fonts) y hace otra con los iconos de botones de la app
(app/pad-icons.js, Kenney, CC0), un glifo por icono desde U+E000. Lilita One se renombra: su licencia OFL reserva ese nombre
para la fuente sin modificar. Solo hace falta volver a ejecutarlo si cambian esos archivos.
Uso: python tools/make_overlay_fonts.py   (necesita fonttools y brotli)
"""
import json, os, re
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'overlay', 'fonts')
PAD_ORDER = ['a', 'b', 'x', 'y', 'lb', 'rb', 'lt', 'rt', 'cross', 'circle', 'square', 'triangle', 'l1', 'r1', 'l2', 'r2', 'xboxpad', 'pspad']


def convert(src, dst, rename=None):
    f = TTFont(os.path.join(ROOT, 'app', 'fonts', src))
    f.flavor = None
    if rename:
        for rec in f['name'].names:
            if rec.nameID in (1, 3, 4, 6, 16):
                rec.string = rename.replace(' ', '') if rec.nameID == 6 else rename
    f.save(os.path.join(OUT, dst))


def draw_path(pen, d):
    """Trazado SVG con solo M, L y Q absolutos (con coordenadas repetidas tras cada letra); y invertida."""
    tok = re.findall(r'[MLQ]|-?[\d.]+', d)
    i, cmd, open_ = 0, None, False
    def pt():
        nonlocal i
        x, y = float(tok[i]), float(tok[i + 1])
        i += 2
        return (round(x * 16), round((64 - y) * 16))
    while i < len(tok):
        if tok[i] in 'MLQ':
            cmd = tok[i]
            i += 1
        if cmd == 'M':
            if open_:
                pen.closePath()
            pen.moveTo(pt())
            open_, cmd = True, 'L'
        elif cmd == 'L':
            pen.lineTo(pt())
        else:
            c = pt()
            pen.qCurveTo(c, pt())
    if open_:
        pen.closePath()


def pad_font():
    s = open(os.path.join(ROOT, 'app', 'pad-icons.js'), encoding='utf-8').read()
    icons = json.loads(s[s.index('{'):s.rindex('}') + 1])
    path = lambda svg: re.search(r' d="([^"]+)"', svg).group(1)
    paths = {k: path(v) for group in icons.values() for k, v in group.items() if k != 'pad'}
    paths['xboxpad'], paths['pspad'] = path(icons['xbox']['pad']), path(icons['ps']['pad'])
    names = ['.notdef'] + PAD_ORDER
    glyphs = {'.notdef': TTGlyphPen(None).glyph()}
    for k in PAD_ORDER:
        pen = TTGlyphPen(None)
        draw_path(pen, paths[k])
        glyphs[k] = pen.glyph()
    fb = FontBuilder(1024, isTTF=True)
    fb.setupGlyphOrder(names)
    fb.setupCharacterMap({0xE000 + i: k for i, k in enumerate(PAD_ORDER)})
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics({n: (1024, 0) for n in names})
    fb.setupHorizontalHeader(ascent=1024, descent=0)
    fb.setupNameTable({'familyName': 'TS3C Pad', 'styleName': 'Regular'})
    fb.setupOS2(sTypoAscender=1024, sTypoDescender=0, usWinAscent=1024, usWinDescent=0)
    fb.setupPost()
    fb.save(os.path.join(OUT, 'pad.ttf'))


os.makedirs(OUT, exist_ok=True)
convert('nunito-latin-500-normal.woff2', 'nunito-500.ttf')
convert('nunito-latin-700-normal.woff2', 'nunito-700.ttf')
convert('nunito-latin-800-normal.woff2', 'nunito-800.ttf')
convert('lilita-one-latin-400-normal.woff2', 'display.ttf', 'TS3C Display')
pad_font()
for f in sorted(os.listdir(OUT)):
    t = TTFont(os.path.join(OUT, f))
    print(f, os.path.getsize(os.path.join(OUT, f)), 'upm', t['head'].unitsPerEm, 'hhea', t['hhea'].ascent, t['hhea'].descent, t['hhea'].lineGap, 'glyf' in t, len(t.getGlyphOrder()), 0x2192 in t.getBestCmap())
