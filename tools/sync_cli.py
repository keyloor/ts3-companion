"""Lee la partida de Toy Story 3 y muestra lo conseguido según app/data/savemap.json (misma lógica que app/save.js).
Los logros de Steam no se leen aquí: los lee la app de escritorio.

Uso: python -I -X utf8 tools/sync_cli.py [--save RUTA] [--json]
"""
import argparse, json, os, re, struct, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = os.path.join(os.environ.get('APPDATA', ''), 'Disney', 'Toy Story 3 Complete Edition', 'savedata', 'Saved Game')
OPS = {'MOREOREQUAL': lambda a, b: a >= b, 'MORE': lambda a, b: a > b, 'EQUAL': lambda a, b: a == b,
       'LESSOREQUAL': lambda a, b: a <= b, 'LESS': lambda a, b: a < b, 'EQUALNONZERO': lambda a, b: a == b and a != 0}


def jshash(s):
    h = 0x4E67C6A7
    for c in s.encode('latin1'):
        c = c - 0x100 if c >= 0x80 else c
        h = (h ^ (((h << 5) + (h >> 2) + c) & 0xffffffff)) & 0xffffffff
    return h


def decompress(data):
    outer = zlib.decompress(data[0x18:0x18 + struct.unpack_from('<I', data, 12)[0]])
    return zlib.decompress(outer[0x14:0x14 + struct.unpack_from('<I', outer, 8)[0]])


class Save:
    def __init__(self, raw):
        self.f = {}
        for m in re.finditer(rb'(?s)(.{4})\x04\x00\x00\x00(.{4})', raw):
            self.f.setdefault(struct.unpack('<I', m.group(1))[0], m.group(2))

    def lock(self, n):
        v = self.f.get(jshash('LOCK%03d' % (n - 1)))
        return struct.unpack('<i', v)[0] if v else 0

    def stat(self, name):
        v = self.f.get(jshash('Stat_' + name))
        if v is None:
            return None
        i = struct.unpack('<i', v)[0]
        return struct.unpack('<f', v)[0] if abs(i) > 10_000_000 else i

    def evaluate(self, ev):
        t, d = ev['Type'], ev.get('Data', {})
        if t == 'Eval_Stat':
            v = self.stat(d['Stat'])
            return None if v is None else OPS[d['Operator']](v, d['Value'])
        if t == 'Eval_LogicAND':
            rs = [self.evaluate(c) for c in d.get('Children', [])]
            return False if False in rs else (None if None in rs else True)
        if t == 'Eval_NumericCompare':
            vals = [self.stat(c['Data']['Stat']) if c['Type'] == 'EvalFetch_Stat' else c['Data'].get('Value') if c['Type'] == 'EvalFetch_Number' else None for c in d['Children']]
            return None if None in vals else OPS[d['Operator']](*vals)
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--save', default=DEFAULT)
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    sm = json.load(open(os.path.join(ROOT, 'app', 'data', 'savemap.json'), encoding='utf-8'))
    s = Save(decompress(open(a.save, 'rb').read()))
    caps = [i for i, l in sm['capsules'].items() if s.lock(l) >= 5]
    discs = [i for i, l in sm['discs'].items() if s.lock(l) >= 5]
    stars = [i for i, c in sm['stars'].items() if s.evaluate(c)]
    # Historia: objetos únicos uno a uno; las cartas de un nivel, todas juntas cuando están completas
    story = [i for i, l in sm['story']['items'].items() if s.lock(l) >= 5]
    partial = []
    for lv in sm['story']['cards']:
        have = sum(s.lock(l) >= 5 for l in lv['locks'])
        if have == len(lv['locks']):
            story += lv['ids']
        elif have:
            partial.append({'level': lv['level'], 'have': have, 'total': len(lv['locks'])})
    counters = {k: s.stat(c['stat']) for k, c in sm.get('counters', {}).items()}
    out = {'capsules': caps, 'discs': discs, 'stars': stars, 'story': story, 'storyCardsPartial': partial, 'counters': counters,
           'starsGame': s.stat(sm['starTotalStat']), 'pictoDone': s.stat(sm['pictoDoneStat'])}
    if a.json:
        print(json.dumps(out))
    else:
        print('Capsules %d/%d · discs %d/6 · gold stars %d (game: %s) · story %d/88 · Pict-O scenes %s/40 · army men %s/%d' % (
            len(caps), len(sm['capsules']), len(discs), len(stars), out['starsGame'], len(story), out['pictoDone'],
            counters.get('para'), sm['counters']['para']['total']))
        for p in partial:
            print('  level %d: %d/%d cards' % (p['level'], p['have'], p['total']))


if __name__ == '__main__':
    sys.exit(main())
