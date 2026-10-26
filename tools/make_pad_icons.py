"""Genera app/pad-icons.js con los iconos de botones de Kenney Input Prompts (CC0, www.kenney.nl).

Uso: python tools/make_pad_icons.py RUTA_A_kenney_input-prompts (carpeta descomprimida)
Los gatillos son blancos en el original; se pasan a currentColor para que se vean en tema claro y oscuro.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONS = {
    'xbox': {'a': 'xbox_button_color_a', 'b': 'xbox_button_color_b', 'x': 'xbox_button_color_x', 'y': 'xbox_button_color_y',
             'lb': 'xbox_lb', 'rb': 'xbox_rb', 'lt': 'xbox_lt', 'rt': 'xbox_rt', 'pad': 'controller_xboxseries'},
    'ps': {'cross': 'playstation_button_color_cross', 'circle': 'playstation_button_color_circle',
           'square': 'playstation_button_color_square', 'triangle': 'playstation_button_color_triangle',
           'l1': 'playstation_trigger_l1', 'r1': 'playstation_trigger_r1', 'l2': 'playstation_trigger_l2', 'r2': 'playstation_trigger_r2',
           'pad': 'controller_playstation5'},
}
FOLDERS = {'xbox': 'Xbox Series', 'ps': 'PlayStation Series'}


def load(src, pad, name):
    svg = open(os.path.join(src, FOLDERS[pad], 'Vector', name + '.svg'), encoding='utf-8').read()
    w, h = re.search(r'<svg width="(\d+)" height="(\d+)"', svg).groups()
    inner = re.search(r'<g>(.*)</g>', svg, re.S).group(1)
    inner = re.sub(r'\s+', ' ', inner).replace('fill="#FFFFFF"', 'fill="currentColor"').replace(' stroke="none"', '').strip()
    return '<svg viewBox="0 0 %s %s" aria-hidden="true">%s</svg>' % (w, h, inner)


def main(src):
    out = {pad: {k: load(src, pad, f) for k, f in icons.items()} for pad, icons in ICONS.items()}
    js = ('// Iconos de botones: Kenney Input Prompts 1.5 (CC0 1.0, www.kenney.nl). Generado por tools/make_pad_icons.py\n'
          'window.PAD_ICONS = ' + json.dumps(out, ensure_ascii=False, indent=1) + ';\n')
    open(os.path.join(ROOT, 'app', 'pad-icons.js'), 'w', encoding='utf-8', newline='\n').write(js)
    print('app/pad-icons.js', len(js), 'bytes')


if __name__ == '__main__':
    main(sys.argv[1])
