(function () {
  'use strict';

  // ---------- textos de la interfaz ----------
  const UI = {
    es: {
      eyebrow: 'Herramienta no oficial · Steam / PC', title: 'Toy Story 3 Companion',
      tabs: { route: 'Ruta', achievements: 'Logros', story: 'Historia', capsules: 'Cápsulas', stars: 'Estrellas', picto: 'Pict-O-Matic' },
      search: 'Buscar…', hideGot: 'Ocultar lo conseguido', markAll: 'Marcar todo', unmarkAll: 'Desmarcar todo',
      allHere: 'Ya tienes todo lo de aquí.', allFilter: 'Ya tienes todo lo de aquí con este filtro.', allCats: 'Todas las categorías',
      reset: 'Reiniciar pestaña', resetAsk: '¿Borrar lo marcado en esta pestaña?', yes: 'Sí, borrar', no: 'Cancelar',
      syncTitle: 'Sincronizar con tu partida', syncNow: 'Sincronizar ahora', loadSave: 'Cargar partida…', auto: 'Al guardar en el juego',
      syncDesktop: 'Partida encontrada', syncNoSave: 'No encuentro la partida', syncWeb: 'Elige el archivo «Saved Game»',
      syncBusy: 'Leyendo la partida…', syncErr: 'No se pudo leer la partida', synced: 'Sincronizado',
      savePathHint: 'Está en %APPDATA%\\Disney\\Toy Story 3 Complete Edition\\savedata\\Saved Game',
      pickSave: 'Buscar partida…', changeSave: 'Cambiar…', autoSave: 'Ubicación automática', chosenByHand: 'Elegida a mano',
      notASave: 'Ese archivo no es una partida de Toy Story 3.', saveChosen: 'Partida guardada: la recordaré la próxima vez.',
      backup: 'Copia de seguridad', exportBtn: 'Exportar', importBtn: 'Importar', imported: 'Progreso importado',
      buttons: 'Botones', added: (n) => `Añadidos ${n} elementos nuevos.`, nothingNew: 'Nada nuevo desde la última sincronización.',
      starsOk: (n) => `Estrellas: ${n}, igual que el juego.`, starsDiff: (a, b) => `Estrellas: ${a} calculadas, el juego dice ${b}.`,
      pictoDone: (n) => `Escenas Pict-O-Matic hechas: ${n}/40 (márcalas a mano).`,
      storyCards: (lv, a, b) => `${lv}: ${a}/${b} cartas. Se marcan solas al completar el nivel; antes, márcalas a mano.`,
      steamOk: (n) => `Logros de Steam: ${n}/41 desbloqueados.`, steamNone: 'No encuentro los logros de Steam en este PC.',
      steamRow: 'Logros de Steam', steamFound: 'Se leen de la caché local de Steam al sincronizar.', steamMissing: 'No encontrados. Abre Steam con tu cuenta y vuelve a sincronizar.',
      steamWeb: 'Solo en la app de escritorio. En la web, márcalos a mano.',
      progress: (a, b, l) => `Llevas ${a}/${b} ${l}`, goTo: (s) => `Ir a ${s} →`,
      src: { START: ['de inicio', 'Lo tienes desde el principio'], SHOP: ['tienda', 'Se compra en la tienda de golosinas'] },
      found: 'Encontrado', disc: 'Disco', discFound: 'Disco encontrado', scenes: (n) => `${n} escenas`,
      rarity: { G: 'Raro', S: 'Poco común', B: 'Común', P: 'Oculto en Steam' },
      progLabel: { story: 'coleccionables', stars: 'estrellas', discs: 'discos', picto: 'escenas' },
      overall: 'total',
      routeSteps: [
        ['Modo historia', 'Los 8 niveles con todos sus coleccionables.', ['story', 'achievements']],
        ['Toy Box', 'Misiones, cápsulas y estrellas doradas. Guarda a menudo: el juego no guarda solo.', ['capsules', 'stars']],
        ['Pict-O-Matic', 'Mejor cuando tengas casi todas las cápsulas: así sabes qué piezas usar.', ['picto']],
        ['Limpieza', 'Repasa lo que falte.', ['achievements']],
      ],
      notice: 'Herramienta hecha por fans, sin relación con Disney, Pixar ni Avalanche Software. Toy Story es una marca de Disney/Pixar. Las pistas son textos propios; para más detalle, consulta las guías de Scrapdaddy en PlayStationTrophies.org y GameFAQs.',
      sources: 'Fuentes y créditos',
      footer: 'Hecho por fans · sin relación con Disney, Pixar ni Avalanche Software',
      settings: 'Opciones', close: 'Cerrar',
      appearance: 'Apariencia', language: 'Idioma', theme: 'Tema', themes: { system: 'Sistema', light: 'Claro', dark: 'Oscuro' },
      buttonsDesc: 'Cómo se muestran los botones en las pistas',
      syncSec: 'Sincronización', autoLong: 'Sincronizar al guardar en el juego', autoDesc: 'Revisa la partida cada pocos segundos y marca lo nuevo.',
      autoWeb: 'Solo en la app de escritorio. En la web, carga la partida con el botón de arriba.',
      saveFile: 'Archivo de partida', lastSync: 'Última sincronización', never: 'Todavía no',
      progressSec: 'Tu progreso', storedAt: 'Dónde se guarda', storedWeb: 'En este navegador. Exporta una copia para no perderlo.',
      backupDesc: 'Guarda o recupera tu progreso en un archivo .json.',
      resetAll: 'Borrar todo el progreso', resetAllDesc: 'Desmarca todo en todas las pestañas.', resetAllAsk: '¿Seguro? No se puede deshacer.', cleared: 'Progreso borrado',
      about: 'Acerca de', version: 'Versión', mode: 'Modo', modeDesktop: 'App de escritorio', modeWeb: 'Versión web',
    },
    en: {
      eyebrow: 'Unofficial tool · Steam / PC', title: 'Toy Story 3 Companion',
      tabs: { route: 'Guide', achievements: 'Achievements', story: 'Story', capsules: 'Capsules', stars: 'Gold Stars', picto: 'Pict-O-Matic' },
      search: 'Search…', hideGot: 'Hide completed', markAll: 'Mark all', unmarkAll: 'Unmark all',
      allHere: 'You have everything here.', allFilter: 'You have everything here with this filter.', allCats: 'All categories',
      reset: 'Reset tab', resetAsk: 'Clear everything marked in this tab?', yes: 'Yes, clear', no: 'Cancel',
      syncTitle: 'Sync with your save', syncNow: 'Sync now', loadSave: 'Load save…', auto: 'When the game saves',
      syncDesktop: 'Save found', syncNoSave: 'Save not found', syncWeb: 'Pick the «Saved Game» file',
      syncBusy: 'Reading save…', syncErr: 'Could not read the save', synced: 'Synced',
      savePathHint: 'It is at %APPDATA%\\Disney\\Toy Story 3 Complete Edition\\savedata\\Saved Game',
      pickSave: 'Find save…', changeSave: 'Change…', autoSave: 'Default location', chosenByHand: 'Picked by hand',
      notASave: 'That file is not a Toy Story 3 save.', saveChosen: 'Save set: it will be remembered next time.',
      backup: 'Backup', exportBtn: 'Export', importBtn: 'Import', imported: 'Progress imported',
      buttons: 'Buttons', added: (n) => `Added ${n} new items.`, nothingNew: 'Nothing new since the last sync.',
      starsOk: (n) => `Gold stars: ${n}, same as the game.`, starsDiff: (a, b) => `Gold stars: ${a} computed, the game says ${b}.`,
      pictoDone: (n) => `Pict-O-Matic scenes done: ${n}/40 (tick them by hand).`,
      storyCards: (lv, a, b) => `${lv}: ${a}/${b} cards. They tick themselves once the level is complete; until then, tick them by hand.`,
      steamOk: (n) => `Steam achievements: ${n}/41 unlocked.`, steamNone: 'Could not find Steam achievements on this PC.',
      steamRow: 'Steam achievements', steamFound: 'Read from Steam\'s local cache when you sync.', steamMissing: 'Not found. Open Steam with your account and sync again.',
      steamWeb: 'Desktop app only. On the web, tick them by hand.',
      progress: (a, b, l) => `${a}/${b} ${l}`, goTo: (s) => `Go to ${s} →`,
      src: { START: ['starter', 'You have it from the start'], SHOP: ['candy shop', 'Bought at the candy shop'] },
      found: 'Found', disc: 'Disc', discFound: 'Disc found', scenes: (n) => `${n} scenes`,
      rarity: { G: 'Rare', S: 'Uncommon', B: 'Common', P: 'Hidden on Steam' },
      progLabel: { story: 'collectibles', stars: 'stars', discs: 'discs', picto: 'scenes' },
      overall: 'overall',
      routeSteps: [
        ['Story mode', 'All 8 levels with every collectible.', ['story', 'achievements']],
        ['Toy Box', 'Missions, capsules and gold stars. Save often: the game does not autosave.', ['capsules', 'stars']],
        ['Pict-O-Matic', 'Best once you own almost every capsule, so you know which pieces to use.', ['picto']],
        ['Clean-up', 'Check what is left.', ['achievements']],
      ],
      notice: 'Fan-made tool, not affiliated with Disney, Pixar or Avalanche Software. Toy Story is a trademark of Disney/Pixar. Hints are original text; for full detail see Scrapdaddy\'s guides on PlayStationTrophies.org and GameFAQs.',
      sources: 'Sources and credits',
      footer: 'Fan-made · not affiliated with Disney, Pixar or Avalanche Software',
      settings: 'Settings', close: 'Close',
      appearance: 'Appearance', language: 'Language', theme: 'Theme', themes: { system: 'System', light: 'Light', dark: 'Dark' },
      buttonsDesc: 'How buttons are shown in the hints',
      syncSec: 'Sync', autoLong: 'Sync when the game saves', autoDesc: 'Checks the save every few seconds and ticks anything new.',
      autoWeb: 'Desktop app only. On the web, load your save with the button at the top.',
      saveFile: 'Save file', lastSync: 'Last sync', never: 'Not yet',
      progressSec: 'Your progress', storedAt: 'Stored in', storedWeb: 'In this browser. Export a copy to keep it safe.',
      backupDesc: 'Save or restore your progress as a .json file.',
      resetAll: 'Clear all progress', resetAllDesc: 'Unticks everything in every tab.', resetAllAsk: 'Sure? This cannot be undone.', cleared: 'Progress cleared',
      about: 'About', version: 'Version', mode: 'Mode', modeDesktop: 'Desktop app', modeWeb: 'Web version',
    },
  };

  const SVG = inner => `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true">${inner}</svg>`;
  const TYPE_ICONS = {
    CC: SVG('<rect x="3.5" y="1.5" width="9" height="13" rx="1.8" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M8 4.9l.95 1.95 2.1.3-1.52 1.48.36 2.1L8 9.74l-1.89.99.36-2.1L4.95 7.15l2.1-.3z"/>'),
    SP: SVG('<rect x="3" y="1.5" width="10" height="13" rx="1.8" opacity=".35"/><rect x="3" y="1.5" width="10" height="13" rx="1.8" fill="none" stroke="currentColor" stroke-width="1.4"/><path d="M8 3.6c.4 2.6 1.6 3.8 4 4.4-2.4.6-3.6 1.8-4 4.4-.4-2.6-1.6-3.8-4-4.4 2.4-.6 3.6-1.8 4-4.4z"/>'),
    TP: SVG('<circle cx="8" cy="4.3" r="2.7"/><path d="M3 14.6c0-3.3 2.2-5.6 5-5.6s5 2.3 5 5.6z"/>'),
    AM: SVG('<path d="M4.9 9.1l.8-4.9c.2-1.1 1.3-1.5 2.3-.9 1-.6 2.1-.2 2.3.9l.8 4.9z"/><path d="M1.2 8.9c1.7 2.6 4.1 3.6 6.8 3.6s5.1-1 6.8-3.6c-1-.3-1.9.1-2.9.6-1.2.6-2.5.8-3.9.8s-2.7-.2-3.9-.8c-1-.5-1.9-.9-2.9-.6z"/>'),
    ZP: SVG('<path d="M2.5 5h2.8a1.85 1.85 0 1 1 3.5 0h2.9v2.9a1.85 1.85 0 1 1 0 3.5V14.3H2.5z"/>'),
    AL: SVG('<path d="M7.3 1h1.4v2.7H7.3z"/><circle cx="8" cy="1.3" r="1.2"/><path fill-rule="evenodd" d="M2.4 9.4C2.4 5.7 4.9 3.4 8 3.4s5.6 2.3 5.6 6c0 3-2.5 5.2-5.6 5.2S2.4 12.4 2.4 9.4zM4.6 8.9a1 1 0 1 0 2 0 1 1 0 1 0-2 0zM7 7.6a1 1 0 1 0 2 0 1 1 0 1 0-2 0zM9.4 8.9a1 1 0 1 0 2 0 1 1 0 1 0-2 0z"/>'),
  };
  const TAB_ICONS = {
    route: SVG('<path d="M5.5 1.5 1.5 3v11.5l4-1.5 5 1.5 4-1.5V1.5l-4 1.5zM6 3l4 1.2v8.8L6 11.8z" fill-rule="evenodd"/>'),
    achievements: SVG('<path d="M4.5 1.5h7v1.3h2.6v2.1a3 3 0 0 1-2.8 3 3.8 3.8 0 0 1-2.4 1.9v1.7h2v2h-5.8v-2h2v-1.7a3.8 3.8 0 0 1-2.4-1.9 3 3 0 0 1-2.8-3V2.8h2.6zM3.4 4.3v.6c0 .6.4 1.1.9 1.3V4.3zm8.3 0v1.9c.5-.2.9-.7.9-1.3v-.6z"/>'),
    story: SVG('<path d="M8 1.2l2 4.1 4.5.6-3.3 3.1.8 4.5L8 11.4l-4 2.1.8-4.5L1.5 5.9 6 5.3z"/>'),
    capsules: SVG('<path d="M8 1.5a5.2 5.2 0 0 0-5.2 5.2h10.4A5.2 5.2 0 0 0 8 1.5z" opacity=".55"/><path d="M2.8 8.1h10.4a5.2 5.2 0 0 1-10.4 0z"/><rect x="2" y="6.4" width="12" height="2" rx="1"/>'),
    stars: SVG('<circle cx="8" cy="8" r="6.6" fill="none" stroke="currentColor" stroke-width="1.5"/><path d="M8 3.9l1.2 2.5 2.7.4-2 1.9.5 2.7L8 10.1l-2.4 1.3.5-2.7-2-1.9 2.7-.4z"/>'),
    picto: SVG('<rect x="3.2" y="1" width="9.6" height="14" rx="1.8"/><rect x="4.7" y="2.6" width="6.6" height="5" rx=".6" style="fill:var(--paper)"/><circle cx="10.4" cy="10.4" r=".95" style="fill:var(--paper)"/><circle cx="9" cy="12" r=".95" style="fill:var(--paper)"/>'),
  };
  const MEDAL = '<svg class="medal" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M7 2h10v2h3.5v3.2A4.5 4.5 0 0 1 16.6 11.6 5.6 5.6 0 0 1 13 14.4V17h3v3H8v-3h3v-2.6a5.6 5.6 0 0 1-3.6-2.8A4.5 4.5 0 0 1 3.5 7.2V4H7V2zm-1.5 4v1.2c0 1 .6 1.9 1.5 2.3V6H5.5zm11.5 0v3.5c.9-.4 1.5-1.3 1.5-2.3V6H17z"/></svg>';
  const CHECK = '<svg viewBox="0 0 16 16" fill="none" aria-hidden="true"><path d="M3 8.5l3.2 3L13 4.5" stroke="currentColor" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/></svg>';
  const CHEV = '<svg class="chev" viewBox="0 0 20 20" fill="none" aria-hidden="true"><path d="M5 7.5l5 5 5-5" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>';

  // Los textos usan botones de PlayStation; se muestran según la preferencia: [icono, nombre accesible]
  const PADS = {
    xbox: { 'X': ['a', 'A'], '□': ['x', 'X'], '△': ['y', 'Y'], '○': ['b', 'B'], 'R1': ['rb', 'RB'], 'R2': ['rt', 'RT'], 'L1': ['lb', 'LB'], 'L2': ['lt', 'LT'] },
    ps: { 'X': ['cross', '✕'], '□': ['square', '□'], '△': ['triangle', '△'], '○': ['circle', '○'], 'R1': ['r1', 'R1'], 'R2': ['r2', 'R2'], 'L1': ['l1', 'L1'], 'L2': ['l2', 'L2'] },
  };
  const padIcon = (pad, key) => { const [icon, name] = PADS[pad][key]; return `<span class="pad" role="img" aria-label="${name}" title="${name}">${PAD_ICONS[pad][icon]}</span>`; };

  // ---------- estado ----------
  const LS = { get(k, d) { try { const v = localStorage.getItem(k); return v ? JSON.parse(v) : d; } catch { return d; } }, set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch {} } };
  const prefs = Object.assign({ lang: navigator.language && navigator.language.startsWith('es') ? 'es' : 'en', pad: 'xbox', tab: 'route', hide: false, types: [], cat: '', closed: [], auto: true, theme: 'system' }, LS.get('ts3c-prefs', {}));
  const savePrefs = () => LS.set('ts3c-prefs', prefs);
  let checked = new Set();
  let C, SAVEMAP, SECTIONS = [], SEC = {}, INDEX = {};
  let current = null;
  const query = {};
  const VERSION = '1.0.0';
  const desktop = { on: false, mtime: null, found: null, path: null, steam: null, steamMtime: null };
  let lastSync = null;
  const t = () => UI[prefs.lang];
  const L = (o) => (o && typeof o === 'object' && 'es' in o) ? o[prefs.lang] : o;
  const esc = s => String(s == null ? '' : s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const fmt = s => esc(s)
    .replace(/\[(X|□|△|○|R1|R2|L1|L2)\]/g, (_, k) => padIcon(prefs.pad, k))
    .replace(/\{((?:CP|RC|ZB|KN)\d+)\}/g, (_, id) => `<span class="cap" data-cap="${id}">${id}</span>`)
    .replace(/ \{(START|SHOP)\}/g, (_, k) => ` <span class="src ${k.toLowerCase()}" title="${esc(t().src[k][1])}">${esc(t().src[k][0])}</span>`);

  // ---------- almacenamiento: servidor local (ejecutable) o navegador ----------
  const store = {
    async load() {
      if (desktop.on) {
        try { const r = await fetch('/api/progress'); if (r.ok) return (await r.json()).checked || []; } catch {}
      }
      return LS.get('ts3c-progress', { checked: [] }).checked || [];
    },
    timer: null,
    save() {
      const body = { checked: [...checked].sort(), updated: new Date().toISOString() };
      LS.set('ts3c-progress', body);
      if (!desktop.on) return;
      clearTimeout(this.timer);
      this.timer = setTimeout(() => fetch('/api/progress', { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).catch(() => {}), 400);
    },
  };

  // ---------- secciones a partir de content.json ----------
  function build() {
    SECTIONS = []; INDEX = {};
    const add = s => { SECTIONS.push(s); s.groups.forEach(g => g.items.forEach(it => INDEX[it.id] = it)); };
    add({ key: 'route', groups: [] });
    const rarity = p => p == null ? 'P' : p < 10 ? 'G' : p < 30 ? 'S' : 'B';
    add({
      key: 'achievements',
      groups: Object.keys(C.achievementGroups).map(gk => ({
        key: gk, title: C.achievementGroups[gk], icon: 'achievements',
        items: C.achievements.filter(a => a.group === gk).map(a => ({
          id: a.id, lead: MEDAL.replace('class="medal"', `class="medal ${rarity(a.pct)}"`), title: a.name, sub: a.name.es !== a.name.en ? a.name : null,
          flags: [{ text: () => t().rarity[rarity(a.pct)] + (a.pct != null ? ' · ' + a.pct.toLocaleString(prefs.lang) + ' %' : '') }],
          text: a.desc, tip: a.tip, prog: a.prog, link: a.link,
        })),
      })),
    });
    add({
      key: 'story', typeFilter: true,
      groups: C.story.map(lv => ({
        key: 'l' + lv.n, num: lv.n, title: lv.name, sub: { es: lv.sub, en: lv.sub },
        items: lv.items.map((it, i) => ({ id: it.id, num: i + 1, type: it.type, text: it.hint, storyFlags: it.flags })),
      })),
    });
    add({
      key: 'capsules', catFilter: true,
      groups: C.capsules.map((a, ai) => ({
        key: a.key, num: ai + 1, title: a.name, intro: a.intro,
        items: a.items.map(c => ({ id: c.id, code: c.code, cat: c.cat, title: { es: c.name, en: c.name }, catBadge: c.cat, text: c.hint, note: c.note })),
      })),
    });
    add({
      key: 'stars',
      groups: C.stars.map(g => ({ key: g.key, title: g.title, intro: g.intro, icon: 'stars', items: g.items.map(s => ({ id: s.id, title: { es: s.name, en: s.name }, text: s.desc, tip: s.tip, link: s.link })) })),
    });
    add({
      key: 'picto', intro: C.picto.intro,
      groups: C.picto.discs.map(d => ({
        key: 'd' + d.n, num: d.n, title: { es: 'Disco ' + d.n, en: 'Disc ' + d.n }, subFn: () => t().scenes(d.missions.length),
        items: [{ id: 'pd-' + d.n, disc: true, lead: TAB_ICONS.picto.replace('<svg', '<svg class="medal" style="color:var(--accent-ink)"'), titleFn: () => t().discFound, text: d.where }]
          .concat(d.missions.map(m => ({ id: m.id, title: { es: m.name, en: m.name }, text: m.req }))),
      })),
    });
    SEC = Object.fromEntries(SECTIONS.map(s => [s.key, s]));
  }

  const items = key => SEC[key].groups.flatMap(g => g.items);
  const count = list => [list.filter(i => checked.has(i.id)).length, list.length];
  function progFor(p) {
    if (p === 'story') return count(items('story'));
    if (p && p.startsWith('story:')) return count(items('story').filter(i => i.type === p.slice(6)));
    if (p === 'stars') return count(items('stars'));
    if (p === 'discs') return count(items('picto').filter(i => i.disc));
    if (p === 'picto') return count(items('picto').filter(i => !i.disc));
    return [0, 0];
  }

  // ---------- render ----------
  const $ = id => document.getElementById(id);
  function renderChrome() {
    document.documentElement.lang = prefs.lang;
    $('eyebrow').textContent = t().eyebrow;
    $('title').innerHTML = esc(t().title) + ' <span class="star">★</span>';
    $('sync-title').textContent = t().syncTitle;
    $('sync-now').textContent = t().syncNow;
    $('load-save').textContent = t().loadSave;
    $('pick-save').textContent = t().pickSave;
    syncButtons();
    $('footer').textContent = t().footer;
    for (const [id, k] of [['settings-open', 'settings'], ['settings-close', 'close']]) { $(id).title = t()[k]; $(id).setAttribute('aria-label', t()[k]); }
    renderSettings();
    if (lastStatus) $('sync-status').textContent = lastStatus[1]();
    $('tabs').innerHTML = SECTIONS.map(s => `<button type="button" class="tab" role="tab" data-tab="${s.key}" aria-selected="${s.key === current}">${TAB_ICONS[s.key]}<span class="tl">${t().tabs[s.key]}</span><span class="tc" data-tc="${s.key}"></span></button>`).join('');
  }

  // En escritorio, si no se encuentra la partida, el botón principal pasa a ser «Buscar partida…»
  function syncButtons() {
    const lost = desktop.on && desktop.found === false;
    $('sync-now').hidden = !desktop.on || lost;
    $('pick-save').hidden = !lost;
    $('load-save').hidden = desktop.on;
  }

  // ---------- opciones ----------
  const seg = (pref, opts) => `<span class="seg" role="group">${opts.map(([v, label, icon]) => `<button type="button" data-pref="${pref}" data-v="${v}" aria-pressed="${prefs[pref] === v}">${icon || ''}${esc(label)}</button>`).join('')}</span>`;
  const row = (lbl, desc, ctl, cls = '') => `<div class="set-row"><div><span class="lbl">${esc(lbl)}</span>${desc ? `<span class="desc ${cls}">${desc}</span>` : ''}</div>${ctl || ''}</div>`;
  function renderSettings() {
    const u = t(), body = $('settings-body');
    $('settings-title').textContent = u.settings;
    const a = document.activeElement, focus = a && body.contains(a) ? (a.dataset.v ? `[data-v="${a.dataset.v}"]` : a.id ? '#' + a.id : null) : null;
    const savePath = desktop.on ? esc(desktop.path || u.syncNoSave) + (desktop.custom ? ` · ${esc(u.chosenByHand)}` : '') : esc(u.savePathHint);
    const saveActs = desktop.on ? `<span class="acts"><button type="button" class="btn" id="change-save">${esc(desktop.found ? u.changeSave : u.pickSave)}</button>${desktop.custom ? `<button type="button" class="btn" id="reset-save">${esc(u.autoSave)}</button>` : ''}</span>` : '';
    body.innerHTML = `
      <section class="set-group"><h3>${esc(u.appearance)}</h3><div class="card set-list">
        ${row(u.language, '', seg('lang', [['es', 'Español'], ['en', 'English']]))}
        ${row(u.theme, '', seg('theme', Object.entries(u.themes)))}
        ${row(u.buttons, esc(u.buttonsDesc), seg('pad', [['xbox', 'Xbox', `<span class="pad">${PAD_ICONS.xbox.pad}</span>`], ['ps', 'PlayStation', `<span class="pad">${PAD_ICONS.ps.pad}</span>`]]))}
      </div></section>
      <section class="set-group"><h3>${esc(u.syncSec)}</h3><div class="card set-list">
        ${row(u.autoLong, esc(desktop.on ? u.autoDesc : u.autoWeb), `<input type="checkbox" role="switch" class="switch" id="auto" aria-label="${esc(u.autoLong)}"${prefs.auto && desktop.on ? ' checked' : ''}${desktop.on ? '' : ' disabled'}>`)}
        ${row(u.saveFile, savePath, saveActs, 'path')}
        ${row(u.steamRow, esc(!desktop.on ? u.steamWeb : desktop.steam ? u.steamFound : u.steamMissing))}
        ${row(u.lastSync, esc(lastSync ? lastSync.toLocaleString(prefs.lang) : u.never))}
      </div></section>
      <section class="set-group"><h3>${esc(u.progressSec)}</h3><div class="card set-list">
        ${row(u.storedAt, desktop.on ? '%APPDATA%\\TS3Companion\\progress.json' : esc(u.storedWeb), '', desktop.on ? 'path' : '')}
        ${row(u.backup, esc(u.backupDesc), `<span class="acts"><button type="button" class="btn" id="export">${esc(u.exportBtn)}</button><button type="button" class="btn" id="import">${esc(u.importBtn)}</button></span>`)}
        ${row(u.resetAll, esc(u.resetAllDesc), `<span class="acts" id="reset-all-slot"><button type="button" class="btn danger" id="reset-all">${esc(u.resetAll)}</button></span>`)}
      </div></section>
      <section class="set-group"><h3>${esc(u.about)}</h3><div class="card set-list">
        ${row(u.version, esc(VERSION))}
        ${row(u.mode, esc(desktop.on ? u.modeDesktop : u.modeWeb))}
        <div class="set-row about-note">${esc(u.notice)}</div>
        <div class="set-row"><span class="acts"><a href="CREDITS.md" target="_blank" rel="noopener">${esc(u.sources)}</a> · <a href="https://github.com/keyloor/ts3-companion" target="_blank" rel="noopener">GitHub</a></span></div>
      </div></section>`;
    const el = focus && body.querySelector(focus);
    if (el) el.focus();
  }
  // Tema: un círculo que crece desde el botón pulsado. Idioma: fundido suave. Sin animación si el sistema la desactiva.
  function animate(kind, update, from) {
    if (!document.startViewTransition || matchMedia('(prefers-reduced-motion: reduce)').matches) return update();
    const root = document.documentElement;
    root.dataset.vt = kind;
    let ran = false;
    const vt = document.startViewTransition(() => { ran = true; update(); });
    // Si la ventana no está pintando (minimizada, en segundo plano) la transición no arranca: se aplica sin animación
    setTimeout(() => { if (!ran) vt.skipTransition(); }, 400);
    if (kind === 'theme' && from) {
      const b = from.getBoundingClientRect(), x = b.left + b.width / 2, y = b.top + b.height / 2;
      const r = Math.hypot(Math.max(x, innerWidth - x), Math.max(y, innerHeight - y));
      vt.ready.then(() => root.animate({ clipPath: [`circle(0 at ${x}px ${y}px)`, `circle(${r}px at ${x}px ${y}px)`] },
        { duration: 480, easing: 'cubic-bezier(.4, 0, .2, 1)', pseudoElement: '::view-transition-new(root)' })).catch(() => {});
    }
    vt.ready.catch(() => {});   // saltar la transición no es un error
    vt.finished.catch(() => {}).finally(() => delete root.dataset.vt);
  }
  function applyTheme() {
    const root = document.documentElement;
    if (prefs.theme === 'system') root.removeAttribute('data-theme'); else root.dataset.theme = prefs.theme;
    // La barra de título de la ventana sigue el color del fondo
    document.querySelector('meta[name="theme-color"]').content = getComputedStyle(root).getPropertyValue('--sky').trim();
  }
  matchMedia('(prefers-color-scheme: dark)').addEventListener('change', applyTheme);

  function itemHTML(it) {
    const lead = it.lead || (it.code ? `<span class="code">${esc(it.code)}</span>` : `<span class="num">${it.num || ''}</span>`);
    const title = it.titleFn ? it.titleFn() : L(it.title);
    const badges = [];
    if (it.type) badges.push(`<span class="badge t-${it.type}">${TYPE_ICONS[it.type]}${esc(C.storyTypes[it.type][prefs.lang].one)}</span>`);
    if (it.catBadge) badges.push(`<span class="badge plain">${esc(L(it.catBadge))}</span>`);
    (it.flags || []).forEach(f => badges.push(`<span class="flag">${esc(f.text())}</span>`));
    if (it.sub && prefs.lang === 'es' && it.sub.en !== it.sub.es) badges.push(`<span class="ename">${esc(it.sub.en)}</span>`);
    const sf = it.storyFlags && it.storyFlags[prefs.lang] || {};
    if (sf.miss) badges.push(`<span class="flag miss">${esc(sf.miss)}</span>`);
    if (sf.trophy) badges.push(`<span class="flag trophy">${esc(sf.trophy)}</span>`);
    if (sf.note) badges.push(`<span class="flag">${esc(sf.note)}</span>`);
    const extra = [it.prog ? `<span data-prog="${it.prog}"></span>` : '', it.link ? `<a href="#${it.link}" data-goto="${it.link}">${esc(t().goTo(t().tabs[it.link]))}</a>` : ''].join('');
    const tip = L(it.tip) || it.note || '';
    return `<li class="item" data-id="${it.id}"><label style="display:contents">
      <input type="checkbox" data-id="${it.id}"><span class="box">${CHECK}</span>${lead}
      <span class="main">
        <span class="meta">${title ? `<span class="ititle">${esc(title)}</span>` : ''}${badges.join('')}</span>
        ${L(it.text) ? `<span class="txt">${fmt(L(it.text))}</span>` : ''}
        ${tip ? `<span class="tip">${fmt(tip)}</span>` : ''}
        ${extra ? `<span class="extra">${extra}</span>` : ''}
      </span></label></li>`;
  }

  function groupHTML(sec, g) {
    const ck = sec.key + ':' + g.key, open = !prefs.closed.includes(ck);
    const sub = g.subFn ? g.subFn() : L(g.sub);
    return `<section class="card group" id="g-${sec.key}-${g.key}" data-group="${g.key}" data-open="${open}">
      <button type="button" class="g-head" aria-expanded="${open}">
        <span class="g-num">${g.num || TAB_ICONS[g.icon || sec.key]}</span>
        <span class="g-title"><span class="g-name">${esc(L(g.title))}</span>${sub ? `<span class="g-sub">${esc(sub)}</span>` : ''}<span class="g-mini"><span class="meter"><i></i></span></span></span>
        <span class="g-count"><b class="g-n"></b>${CHEV}</span>
      </button>
      <div class="g-body">
        ${L(g.intro) ? `<div class="intro g-intro">${fmt(L(g.intro))}</div>` : ''}
        <div class="g-actions"><button type="button" class="btn g-all" data-group="${g.key}"></button></div>
        <ol class="items">${g.items.map(itemHTML).join('')}</ol>
        <div class="g-empty" hidden></div>
      </div></section>`;
  }

  function renderRoute() {
    const u = t();
    $('view').innerHTML = `
      <ol class="steps">${u.routeSteps.map(([h, p, links]) => `<li class="card step"><h3>${esc(h)}</h3><p>${esc(p)}</p>
        <div class="chips">${links.map(k => `<button type="button" class="chip" data-goto="${k}"><span class="ico">${TAB_ICONS[k]}</span>${esc(u.tabs[k])} <span class="n" data-jcount="${k}"></span></button>`).join('')}</div></li>`).join('')}</ol>`;
  }

  function renderSection(key) {
    const sec = SEC[key], u = t();
    let filter = '';
    if (sec.typeFilter) filter = `<div class="chips">${Object.keys(C.storyTypes).map(k => `<button type="button" class="chip t-${k}" data-type="${k}" aria-pressed="${prefs.types.includes(k)}"><span class="ico">${TYPE_ICONS[k]}</span>${esc(C.storyTypes[k][prefs.lang].name)} <span class="n" data-count="${k}"></span></button>`).join('')}</div>`;
    if (sec.catFilter) {
      const cats = [...new Set(items('capsules').map(i => L(i.cat)))];
      filter = `<select class="select" id="cat-filter"><option value="">${esc(u.allCats)}</option>${cats.map(c => `<option${prefs.cat === c ? ' selected' : ''}>${esc(c)}</option>`).join('')}</select>`;
    }
    $('view').innerHTML = `<div class="filters">
      ${L(sec.intro) ? `<div class="intro">${fmt(L(sec.intro))}</div>` : ''}
      <div class="frow"><input class="search" id="search" type="search" placeholder="${esc(u.search)}" value="${esc(query[key] || '')}">${sec.catFilter ? filter : ''}</div>
      ${sec.typeFilter ? filter : ''}
      <div class="frow"><label class="toggle"><input type="checkbox" id="hide"${prefs.hide ? ' checked' : ''}> ${esc(u.hideGot)}</label>
      <span id="reset-slot"><button class="btn danger" id="reset" type="button">${esc(u.reset)}</button></span></div></div>
      <div class="groups">${sec.groups.map(g => groupHTML(sec, g)).join('')}</div>`;
  }

  function show(key) {
    if (!SEC[key]) key = 'route';
    current = key; prefs.tab = key; savePrefs();
    try { history.replaceState(null, '', '#' + key); } catch {}
    renderChrome();
    $('secbar').hidden = key === 'route';
    key === 'route' ? renderRoute() : renderSection(key);
    refresh();
  }

  function refresh() {
    document.querySelectorAll('.item').forEach(li => { const on = checked.has(li.dataset.id); li.classList.toggle('got', on); li.querySelector('input').checked = on; });
    document.querySelectorAll('.cap').forEach(c => c.classList.toggle('have', checked.has('c-' + c.dataset.cap)));
    let tg = 0, td = 0;
    SECTIONS.forEach(s => {
      if (s.key === 'route') return;
      const [g, d] = count(items(s.key)); tg += g; td += d;
      const el = document.querySelector(`[data-tc="${s.key}"]`);
      if (el) { el.textContent = `${g}/${d}`; el.closest('.tab').classList.toggle('done', g === d); }
      document.querySelectorAll(`[data-jcount="${s.key}"]`).forEach(e => e.textContent = `${g}/${d}`);
    });
    const r = document.querySelector('[data-tc="route"]');
    if (r) r.textContent = Math.round(tg / td * 100) + '% ' + t().overall;
    if (!current || current === 'route') return;
    const sec = SEC[current], [g, d] = count(items(current)), pct = Math.round(g / d * 100);
    $('sec-n').textContent = g; $('sec-d').textContent = d; $('sec-pct').textContent = pct + '%'; $('sec-fill').style.width = pct + '%';
    sec.groups.forEach(gr => {
      const el = $(`g-${sec.key}-${gr.key}`); if (!el) return;
      const [a, b] = count(gr.items);
      el.querySelector('.g-n').textContent = `${a}/${b}`;
      el.querySelector('.g-mini i').style.width = (a / b * 100) + '%';
      el.classList.toggle('complete', a === b);
      el.querySelector('.g-all').textContent = a === b ? t().unmarkAll : t().markAll;
    });
    if (sec.typeFilter) Object.keys(C.storyTypes).forEach(k => { const e = document.querySelector(`[data-count="${k}"]`); if (e) { const [a, b] = progFor('story:' + k); e.textContent = `${a}/${b}`; } });
    document.querySelectorAll('[data-prog]').forEach(e => {
      const p = e.dataset.prog, [a, b] = progFor(p);
      const label = p.startsWith('story:') ? C.storyTypes[p.slice(6)][prefs.lang].name.toLowerCase() : t().progLabel[p];
      e.textContent = t().progress(a, b, label);
    });
    applyFilters();
  }

  function applyFilters() {
    const sec = SEC[current], q = (query[current] || '').trim().toLowerCase();
    sec.groups.forEach(gr => {
      const el = $(`g-${sec.key}-${gr.key}`); let vis = 0, match = 0;
      gr.items.forEach(it => {
        const li = el.querySelector(`[data-id="${it.id}"]`);
        const hay = [L(it.title), L(it.text), L(it.tip), it.code, it.note, it.cat && L(it.cat)].join(' ').toLowerCase();
        const ok = (!q || hay.includes(q)) && (!sec.typeFilter || !prefs.types.length || prefs.types.includes(it.type)) && (!sec.catFilter || !prefs.cat || L(it.cat) === prefs.cat);
        if (ok) match++;
        const showIt = ok && !(prefs.hide && checked.has(it.id));
        li.hidden = !showIt; if (showIt) vis++;
      });
      el.hidden = match === 0;
      const empty = el.querySelector('.g-empty');
      empty.hidden = vis > 0;
      empty.textContent = q || prefs.types.length || prefs.cat ? t().allFilter : t().allHere;
    });
  }

  function setChecked(ids, on) {
    ids.forEach(id => on ? checked.add(id) : checked.delete(id));
    store.save(); refresh();
  }

  // ---------- sincronización ----------
  function toast(msg) {
    // Como popover queda por encima del panel de opciones
    const el = $('toast'); el.innerHTML = msg;
    const hide = () => el.matches(':popover-open') && el.hidePopover();
    hide(); el.showPopover();
    clearTimeout(toast.t); toast.t = setTimeout(hide, 7000);
  }
  // El estado se guarda como función para poder repintarlo al cambiar de idioma
  let lastStatus = null;
  function setStatus(s, text) { lastStatus = [s, typeof text === 'function' ? text : () => text]; const el = $('sync-status'); el.dataset.s = s; el.textContent = lastStatus[1](); }

  // buffer: la partida (o null si solo hay logros); steam: respuesta de /api/steam en la app de escritorio
  async function syncFrom(buffer, steam) {
    setStatus('busy', () => t().syncBusy);
    try {
      const r = buffer ? await TS3Save.readProgress(buffer, SAVEMAP) : null;
      const byName = Object.fromEntries(C.achievements.map(a => [a.name.en, a.id]));
      const ach = steam && steam.found ? steam.unlocked.map(n => byName[n]).filter(Boolean) : [];
      const before = checked.size;
      (r ? [...r.capsules, ...r.discs, ...r.stars, ...r.story] : []).concat(ach).forEach(id => checked.add(id));
      const added = checked.size - before;
      store.save(); refresh();
      const level = n => L(C.story.find(l => l.n === n).name);
      const lines = [added ? t().added(added) : t().nothingNew];
      if (r) {
        lines.push(r.starsChecked ? t().starsOk(r.starsGame) : t().starsDiff(r.stars.length, r.starsGame));
        r.storyCardsPartial.forEach(p => lines.push(t().storyCards(level(p.level), p.have, p.total)));
        if (r.pictoDone != null) lines.push(t().pictoDone(r.pictoDone));
      }
      if (steam) lines.push(steam.found ? t().steamOk(ach.length) : t().steamNone);
      toast(lines.filter(Boolean).map(esc).join('<br>'));
      const when = lastSync = new Date(); setStatus('ok', () => t().synced + ' · ' + when.toLocaleTimeString(prefs.lang));
      if ($('settings').open) renderSettings();
    } catch (e) {
      console.error(e);
      setStatus('err', () => t().syncErr);
    }
  }

  async function syncDesktop() {
    const [save, steam] = await Promise.all([
      fetch('/api/save').then(r => r.ok ? r.arrayBuffer() : null).catch(() => null),
      fetch('/api/steam').then(r => r.json()).catch(() => null),
    ]);
    if (!save && !(steam && steam.found)) return setStatus('err', () => t().syncNoSave);
    await syncFrom(save, steam);
  }

  // Diálogo de Windows para elegir la partida; el servidor recuerda la ruta en config.json
  async function pickSave() {
    try {
      const r = await (await fetch('/api/pick-save', { method: 'POST' })).json();
      if (r.cancelled) return;
      if (r.error) return toast(esc(t().notASave));
      toast(esc(t().saveChosen));
      desktop.mtime = null; desktop.steamMtime = null;
      await pollDesktop();
      await syncDesktop();
    } catch {}
  }

  async function pollDesktop() {
    try {
      const s = await (await fetch('/api/status')).json();
      if (s.found !== desktop.found || s.path !== desktop.path || s.steam !== desktop.steam || s.custom !== desktop.custom) {
        Object.assign(desktop, { found: s.found, path: s.path, steam: s.steam, custom: s.custom });
        syncButtons();
        if ($('settings').open) renderSettings();
      }
      // Sincroniza cuando el juego guarda o cuando Steam apunta un logro nuevo
      const first = desktop.mtime === null && desktop.steamMtime === null;
      const changed = !first && (s.mtime !== desktop.mtime || s.steamMtime !== desktop.steamMtime);
      desktop.mtime = s.mtime; desktop.steamMtime = s.steamMtime;
      if (!s.found) { setStatus('err', () => t().syncNoSave); return; }
      if (changed && prefs.auto) await syncDesktop();
      else if (first) setStatus('ok', () => t().syncDesktop);
    } catch {}
  }

  // ---------- eventos ----------
  document.addEventListener('change', e => {
    const el = e.target;
    if (el.matches('.item input')) setChecked([el.dataset.id], el.checked);
    else if (el.id === 'hide') { prefs.hide = el.checked; savePrefs(); applyFilters(); }
    else if (el.id === 'cat-filter') { prefs.cat = el.value; savePrefs(); applyFilters(); }
    else if (el.id === 'auto') { prefs.auto = el.checked; savePrefs(); }
    else if (el.id === 'save-file' && el.files[0]) { el.files[0].arrayBuffer().then(syncFrom); el.value = ''; }
    else if (el.id === 'import-file' && el.files[0]) {
      el.files[0].text().then(txt => { const d = JSON.parse(txt); (d.checked || []).forEach(id => INDEX[id] && checked.add(id)); store.save(); refresh(); toast(esc(t().imported)); }).catch(() => toast(esc(t().syncErr)));
      el.value = '';
    }
  });
  document.addEventListener('input', e => { if (e.target.id === 'search') { query[current] = e.target.value; applyFilters(); } });
  document.addEventListener('click', e => {
    const tab = e.target.closest('.tab'); if (tab) return show(tab.dataset.tab);
    const go = e.target.closest('[data-goto]'); if (go) { e.preventDefault(); show(go.dataset.goto); window.scrollTo({ top: 0 }); return; }
    const opt = e.target.closest('.seg [data-pref]');
    if (opt) {
      if (prefs[opt.dataset.pref] === opt.dataset.v) return;
      prefs[opt.dataset.pref] = opt.dataset.v; savePrefs();
      if (opt.dataset.pref === 'theme') return animate('theme', () => { applyTheme(); renderSettings(); }, opt);
      if (opt.dataset.pref === 'lang') return animate('lang', () => show(current));
      return show(current);
    }
    if (e.target.closest('#settings-open')) { renderSettings(); return $('settings').showModal(); }
    if (e.target.closest('#settings-close') || e.target.id === 'settings') return $('settings').close();
    if (e.target.id === 'reset-all') { $('reset-all-slot').innerHTML = `<span class="desc">${esc(t().resetAllAsk)}</span> <button class="btn danger" id="reset-all-yes" type="button">${esc(t().yes)}</button> <button class="btn" id="reset-all-no" type="button">${esc(t().no)}</button>`; return $('reset-all-no').focus(); }
    if (e.target.id === 'reset-all-yes') { setChecked(Object.keys(INDEX), false); toast(esc(t().cleared)); return renderSettings(); }
    if (e.target.id === 'reset-all-no') return renderSettings();
    const chip = e.target.closest('.chip[data-type]');
    if (chip) { const k = chip.dataset.type; prefs.types = prefs.types.includes(k) ? prefs.types.filter(x => x !== k) : [...prefs.types, k]; savePrefs(); chip.setAttribute('aria-pressed', prefs.types.includes(k)); return applyFilters(); }
    const head = e.target.closest('.g-head');
    if (head) { const g = head.closest('.group'), ck = current + ':' + g.dataset.group, open = g.dataset.open !== 'true'; g.dataset.open = open; head.setAttribute('aria-expanded', open); prefs.closed = open ? prefs.closed.filter(x => x !== ck) : [...prefs.closed, ck]; return savePrefs(); }
    const all = e.target.closest('.g-all');
    if (all) { const ids = SEC[current].groups.find(g => g.key === all.dataset.group).items.map(i => i.id); return setChecked(ids, !ids.every(id => checked.has(id))); }
    if (e.target.id === 'reset') { $('reset-slot').innerHTML = `${esc(t().resetAsk)} <button class="btn danger" id="reset-yes" type="button">${esc(t().yes)}</button> <button class="btn" id="reset-no" type="button">${esc(t().no)}</button>`; return; }
    if (e.target.id === 'reset-yes' || e.target.id === 'reset-no') { if (e.target.id === 'reset-yes') setChecked(items(current).map(i => i.id), false); return show(current); }
    if (e.target.id === 'sync-now') return syncDesktop();
    if (e.target.id === 'pick-save' || e.target.id === 'change-save') return pickSave();
    if (e.target.id === 'reset-save') return fetch('/api/reset-save', { method: 'POST' }).then(() => { desktop.mtime = null; return pollDesktop(); });
    if (e.target.id === 'load-save') return $('save-file').click();
    if (e.target.id === 'import') return $('import-file').click();
    if (e.target.id === 'export') {
      const blob = new Blob([JSON.stringify({ app: 'ts3-companion', version: 1, checked: [...checked].sort() }, null, 1)], { type: 'application/json' });
      const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = 'ts3-companion-progress.json'; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    }
  });
  window.addEventListener('hashchange', () => { const k = location.hash.slice(1); if (SEC[k] && k !== current) show(k); });
  // En la app de escritorio los enlaces externos se abren en el navegador normal, no en la ventana de la app
  document.addEventListener('click', e => {
    const a = e.target.closest('a[target="_blank"]');
    if (!a || !desktop.on) return;
    e.preventDefault();
    fetch('/api/open?u=' + encodeURIComponent(a.href)).catch(() => {});
  });

  // ---------- arranque ----------
  async function start() {
    applyTheme();
    [C, SAVEMAP] = await Promise.all([fetch('data/content.json').then(r => r.json()), fetch('data/savemap.json').then(r => r.json())]);
    try { const s = await fetch('/api/status'); desktop.on = s.ok && (await s.json()).app === 'ts3-companion'; } catch { desktop.on = false; }
    checked = new Set(await store.load());
    build();
    const h = location.hash.slice(1);
    show(SEC[h] ? h : prefs.tab);
    if (desktop.on) { pollDesktop(); setInterval(pollDesktop, 4000); }
    else setStatus('', () => t().syncWeb);
  }
  start();
})();
