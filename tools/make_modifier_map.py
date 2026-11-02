"""Genera content/save_modifiers.json: los locks de cada tema (modificador) de la base de objetos del juego.

Uso: python tools/make_modifier_map.py RUTA_playerdb.tupa
- playerdb.tupa sale de assets/data.zip (la versión de texto, no playerdb.tup).
- Cada objeto lleva `ModifierType` (HAUNTED, ENCHANTED, SPACE o vacío). Las estrellas doradas «Customizer» piden
  el 100 % de los objetos de un tema (EvalFetch_Locks con Modifier); save.js y sync_cli.py las calculan con esto.
  «Western Collector» pide el tema Western, que ningún objeto lleva: esa estrella sigue siendo manual.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main(playerdb):
    db = open(playerdb, encoding='latin-1').read()
    out = {}
    for n, body in re.findall(r'Lock "(\d+)"\s*=\s*\{\s*id="[^"]+"(.*?)\n\}\n', db, re.S):
        m = re.search(r'cpID="ModifierType"\s*val="([^"]+)"', body)
        if m:
            out.setdefault(m.group(1).upper(), []).append(int(n))
    assert out, 'no ModifierType found: is this the text playerdb.tupa?'
    json.dump(out, open(os.path.join(ROOT, 'content', 'save_modifiers.json'), 'w', encoding='utf-8'), indent=1)
    print('content/save_modifiers.json:', {k: len(v) for k, v in out.items()})


if __name__ == '__main__':
    main(sys.argv[1])
