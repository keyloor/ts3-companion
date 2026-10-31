"""Genera content/save_story.json: qué lock de la partida corresponde a cada coleccionable de historia, y
content/story_icons.json: la imagen del juego (assets/ui/alscards) de cada coleccionable único.

Uso: python tools/make_story_map.py RUTA_playerdb.tupa "RUTA_DEL_JUEGO"
- playerdb.tupa sale de assets/data.zip; da el número de cada lock (SMC_*).
- Los niveles (assets/worlds/st_*.zip) dicen qué SMC_* hay en cada nivel.
Los objetos únicos de cada tipo en un nivel (carta especial, figura, recuerdo, alien) y las piezas de Zurg
(su pista dice cuál es) se asignan uno a uno. Las cartas normales no tienen orden en los datos del juego:
se guardan por nivel y se marcan juntas cuando el nivel está completo.
"""
import json, os, re, sys, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLDS = {1: ['st_train'], 2: ['st_andysroom', 'st_andysbasement'], 3: ['st_buzzgame', 'st_buzzgame_int'], 4: ['st_daycare'],
          5: ['st_bonnie_woody'], 6: ['st_prisonbreak'], 7: ['st_junkyard'], 8: ['st_hauntedbakery']}
ZURG = {'head': 'Head', 'gun': 'Gun', 'cape': 'Cape', 'body': 'Body'}


def kind(smc):
    if smc.startswith('SMC_Card_Special_'): return 'SP'
    if smc.startswith('SMC_Card_'): return 'CC'
    if smc.startswith('SMC_Blockhead_') and not smc.endswith('_Master'): return 'TP'
    if smc.startswith('SMC_Memorabilia_'): return 'AM'
    if smc.startswith('SMC_ZurgPiece_'): return 'ZP'
    if smc.startswith('SMC_Alien_') and not smc.endswith('_Master'): return 'AL'
    return None


def main(playerdb, game):
    db = open(playerdb, encoding='latin-1').read()
    lock = {i: int(n) for n, i in re.findall(r'Lock "(\d+)"\s*=\s*\{\s*id="([^"]+)"', db)}
    story = json.load(open(os.path.join(ROOT, 'app', 'data', 'content.json'), encoding='utf-8'))['story']
    hints = json.load(open(os.path.join(ROOT, 'content', 'en_story.json'), encoding='utf-8'))['hints']
    out = {'items': {}, 'cards': []}
    icons = {}
    for lv in story:
        found = set()
        for w in WORLDS[lv['n']]:
            with zipfile.ZipFile(os.path.join(game, 'assets', 'worlds', w + '.zip')) as zf:
                ap = zf.read(w + '.ap').decode('latin-1')
            # Las cartas usan Lock=, el resto de coleccionables pickup_ID=
            found |= set(re.findall(r'^(?:Lock|pickup_ID)=(SMC_\w+)', ap, re.M))
        by = {}
        for smc in found:
            if kind(smc):
                by.setdefault(kind(smc), []).append(smc)
        want = {}
        for it in lv['items']:
            want.setdefault(it['type'], []).append(it['id'])
        assert {k: len(v) for k, v in by.items()} == {k: len(v) for k, v in want.items()}, (lv['n'], by, want)
        for t, ids in want.items():
            if t == 'CC':
                out['cards'].append({'level': lv['n'], 'ids': ids, 'locks': sorted(lock[s] for s in by[t])})
            elif t == 'ZP':
                for i in ids:
                    part = next(v for k, v in ZURG.items() if re.search(r'\b%s\b' % k, hints[i], re.I))
                    out['items'][i] = lock['SMC_ZurgPiece_' + part]
                    icons[i] = 'smc_zurgpiece_' + part.lower()
            else:
                out['items'][ids[0]] = lock[by[t][0]]
                icons[ids[0]] = by[t][0].lower()
    assert len(set(out['items'].values())) == len(out['items'])
    n = len(out['items']) + sum(len(c['ids']) for c in out['cards'])
    assert n == 88, n
    json.dump(out, open(os.path.join(ROOT, 'content', 'save_story.json'), 'w', encoding='utf-8'), indent=1)
    json.dump(icons, open(os.path.join(ROOT, 'content', 'story_icons.json'), 'w', encoding='utf-8'), indent=1)
    print('content/save_story.json:', len(out['items']), 'objetos únicos +', n - len(out['items']), 'cartas en', len(out['cards']), 'niveles')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
