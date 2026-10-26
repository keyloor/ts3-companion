"""Combina los textos (es/en) y los datos de la partida en app/data/content.json y app/data/savemap.json.

Uso: python -I -X utf8 tools/build_content.py
La versión se toma de VERSION en desktop/companion.py (único sitio donde se cambia) y se copia a content.json
y al recurso de versión del exe (desktop/version.txt).
"""
import json, os, re, unicodedata

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C = os.path.join(ROOT, 'content')
OUT = os.path.join(ROOT, 'app', 'data')
load = lambda n: json.load(open(os.path.join(C, n), encoding='utf-8'))
VERSION = re.search(r"^VERSION = '([0-9.]+)'", open(os.path.join(ROOT, 'desktop', 'companion.py'), encoding='utf-8').read(), re.M).group(1)

es = load('es_tracker.json')
es_hints = load('es_hints.json')
en_story, en_ach, en_stars, en_picto, en_hints = (load(f'en_{n}.json') for n in ('story', 'achievements', 'stars', 'picto', 'hints'))
items, stars_rules = load('save_items.json'), load('save_stars.json')


def slug(s):
    s = unicodedata.normalize('NFD', s.lower())
    s = ''.join(ch for ch in s if unicodedata.category(ch) != 'Mn')
    return re.sub(r'^-|-$', '', re.sub(r'[^a-z0-9]+', '-', s))


def both(a, b):
    return {'es': a, 'en': b}


# ---------- Logros ----------
ach_groups = {'story': both('Modo historia', 'Story mode'), 'coll': both('Coleccionables de la historia', 'Story collectibles'),
              'box': both('Toy Box', 'Toy Box'), 'secret': both('Ocultos en Steam', 'Hidden on Steam')}
achievements = []
for en_key, es_name, pct, es_desc, es_tip, group, *opts in es['TROPHIES']:
    o = opts[0] if opts else {}
    n_en, d_en, t_en = en_ach[en_key]
    achievements.append({'id': 't-' + slug(en_key), 'group': group, 'pct': pct,
                         'name': both(es_name if es_name != en_key else n_en, n_en), 'desc': both(es_desc, d_en), 'tip': both(es_tip, t_en),
                         'link': {'historia': 'story', 'estrellas': 'stars', 'pictomatic': 'picto'}.get(o.get('link')),
                         'prog': o.get('prog')})
assert len(achievements) == 41

# ---------- Historia ----------
types = {k: {'es': {'name': v['name'], 'one': v['one']}} for k, v in es['TYPES'].items()}
EN_TYPES = {'CC': ('Cards', 'Card'), 'SP': ('Special cards', 'Special'), 'TP': ('Townspeople', 'Townsperson'),
            'AM': ("Andy's memorabilia", 'Memorabilia'), 'ZP': ('Zurg parts', 'Zurg part'), 'AL': ('Aliens', 'Alien')}
for k, (n, one) in EN_TYPES.items():
    types[k]['en'] = {'name': n, 'one': one}
levels = []
for li, lv in enumerate(es['LEVELS']):
    its = []
    for ii, it in enumerate(lv['items']):
        t, text = it[0], it[1]
        extra = it[2] if len(it) > 2 else {}
        sid = f's-{li + 1}-{ii + 1}'
        flags_en = en_story['flags'].get(sid, {})
        its.append({'id': sid, 'type': t, 'hint': both(text, en_story['hints'][sid]),
                    'flags': {'es': extra, 'en': flags_en}})
    levels.append({'n': li + 1, 'name': both(lv['es'], en_story['levels'][li]), 'sub': lv['en'], 'items': its})

# ---------- Cápsulas ----------
cats_en = en_picto['categories']
area_meta = {k: (t, i) for k, t, i in es['AREAS']}
areas = []
for key, title, intro in es['AREAS']:
    if key == 'candy':
        lst = [{'id': 'c-DUL%d' % (i + 1), 'code': '!', 'name': n, 'cat': both(cat, cats_en[cat]), 'note': note,
                'hint': both('Tienda de golosinas: sale al lanzar bolas al botón de arriba de la tienda.', en_picto['candy_hint'])}
               for i, (n, cat, note) in enumerate(es['CANDY'])]
    else:
        lst = [{'id': 'c-' + code, 'code': code, 'name': n, 'cat': both(cat, cats_en[cat]), 'note': note,
                'hint': both(es_hints[code], en_hints[code])}
               for code, n, cat, note, a in es['CAPS'] if a == key]
    t_en, i_en = en_picto['areas'][key]
    areas.append({'key': key, 'name': both(title, t_en), 'intro': both(intro, i_en), 'items': lst})
assert sum(len(a['items']) for a in areas) == 289

# ---------- Estrellas ----------
star_groups = []
for g in es['STAR_GROUPS']:
    t_en, i_en = en_stars['groups'][g['key']]
    its = []
    for row in g['items']:
        name, desc, tip = row[0], row[1], row[2]
        o = row[3] if len(row) > 3 else {}
        d_en, tp_en = en_stars['stars'][name]
        its.append({'id': 'g-' + slug(name), 'name': name, 'desc': both(desc, d_en), 'tip': both(tip, tp_en),
                    'link': {'capsulas': 'capsules', 'pictomatic': 'picto'}.get(o.get('link'))})
    star_groups.append({'key': g['key'], 'title': both(g['title'], t_en), 'intro': both(g['intro'], i_en), 'items': its})
assert sum(len(g['items']) for g in star_groups) == 103

# ---------- Pict-O-Matic ----------
discs = []
for di, d in enumerate(es['DISCS']):
    ms = [{'id': 'pm-' + slug(n), 'name': n, 'req': both(req, en_picto['missions'][n])} for n, req in d['missions']]
    discs.append({'n': di + 1, 'where': both(d['where'], en_picto['discs'][di]), 'missions': ms})
assert sum(len(d['missions']) for d in discs) == 40

content = {'version': VERSION, 'achievementGroups': ach_groups, 'achievements': achievements, 'storyTypes': types, 'story': levels,
           'capsules': areas, 'stars': star_groups, 'picto': {'intro': both(
               'El Pict-O-Matic lo consigues con la misión «Lost and Found». Selecciónalo con la cruceta y pulsa [□] para ver las escenas. Para vestir a alguien, cógelo y lánzalo a la sastrería, la sombrerería o la barbería. Los códigos de cápsula se ponen en verde cuando la tienes.',
               en_picto['intro']), 'discs': discs}}

# ---------- Lectura de la partida ----------
savemap = {'capsules': {}, 'discs': {}, 'stars': {}}
for r in items['capsulas'] + items['otros_objetos']:
    savemap['capsules']['c-' + r['cp']] = r['lock']
for r in items['tienda_golosinas']:
    savemap['capsules']['c-' + r['id']] = r['lock']
for n, lock in items['discos_pictomatic_locks'].items():
    savemap['discs']['pd-' + n] = lock
for s in stars_rules:
    savemap['stars'][s['id']] = s['condicion']
savemap['starTotalStat'] = 'GOLDSTAR_COLLECTED'
savemap['pictoDoneStat'] = 'PICTO_COMPLETED_CHALLENGE_COUNT'
savemap['story'] = load('save_story.json')   # generado por tools/make_story_map.py
savemap['counters'] = {'para': {'stat': 'PARATROOPERS_LANDED_UNIQUE', 'total': 30}}   # army men: objetivos de paracaidistas del Toy Box
assert len(savemap['capsules']) == 289 and len(savemap['stars']) == 103
assert len(savemap['story']['items']) + sum(len(c['ids']) for c in savemap['story']['cards']) == 88

os.makedirs(OUT, exist_ok=True)
json.dump(content, open(os.path.join(OUT, 'content.json'), 'w', encoding='utf-8'), ensure_ascii=False, separators=(',', ':'))

# Recurso de versión del exe (lo lee PyInstaller desde el .spec)
nums = ', '.join((VERSION.split('.') + ['0', '0', '0'])[:4])
open(os.path.join(ROOT, 'desktop', 'version.txt'), 'w', encoding='utf-8', newline='\n').write("""VSVersionInfo(
  ffi=FixedFileInfo(filevers=({n}), prodvers=({n}), mask=0x3f, flags=0x0, OS=0x40004, fileType=0x1, subtype=0x0, date=(0, 0)),
  kids=[
    StringFileInfo([StringTable('040904B0', [
      StringStruct('CompanyName', 'Fan-made (unofficial)'),
      StringStruct('FileDescription', 'Toy Story 3 Companion - unofficial 100% tracker'),
      StringStruct('FileVersion', '{v}'),
      StringStruct('InternalName', 'TS3Companion'),
      StringStruct('LegalCopyright', 'MIT License. Not affiliated with Disney/Pixar.'),
      StringStruct('OriginalFilename', 'TS3Companion.exe'),
      StringStruct('ProductName', 'Toy Story 3 Companion'),
      StringStruct('ProductVersion', '{v}')])]),
    VarFileInfo([VarStruct('Translation', [1033, 1200])])
  ]
)
""".format(n=nums, v=VERSION))
json.dump(savemap, open(os.path.join(OUT, 'savemap.json'), 'w', encoding='utf-8'), ensure_ascii=False, separators=(',', ':'))
print('content.json y savemap.json listos:', {k: len(v) if isinstance(v, list) else '' for k, v in content.items()})
