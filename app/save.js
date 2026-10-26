// Lectura de la partida de Toy Story 3 (PC / Steam Complete Edition).
// Formato: "CMP1" + zlib desde 0x18; dentro, otro "CMP1" + zlib desde 0x14.
// Campos: <clave u32><04 00 00 00><valor 4 bytes>; la clave es el hash JS del nombre del campo.
(function (global) {
  'use strict';

  async function inflate(bytes) {
    const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate'));
    return new Uint8Array(await new Response(stream).arrayBuffer());
  }

  function tag(u8, at) {
    return String.fromCharCode(u8[at], u8[at + 1], u8[at + 2], u8[at + 3]);
  }

  async function decompress(file) {
    const u8 = new Uint8Array(file);
    if (tag(u8, 4) !== 'CMP1') throw new Error('not-a-save');
    const outerLen = new DataView(u8.buffer, u8.byteOffset).getUint32(12, true);
    const outer = await inflate(u8.subarray(0x18, 0x18 + outerLen));
    if (tag(outer, 0) !== 'CMP1') throw new Error('not-a-save');
    const innerLen = new DataView(outer.buffer).getUint32(8, true);
    return inflate(outer.subarray(0x14, 0x14 + innerLen));
  }

  // Hash del motor: semilla 0x4E67C6A7, h ^= (h << 5) + (h >> 2) + carácter con signo
  function jshash(s) {
    let h = 0x4E67C6A7;
    for (let i = 0; i < s.length; i++) {
      let c = s.charCodeAt(i) & 0xff;
      if (c >= 0x80) c -= 0x100;
      h = (h ^ ((((h << 5) >>> 0) + (h >>> 2) + c) >>> 0)) >>> 0;
    }
    return h;
  }

  function readFields(u8) {
    const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);
    const fields = new Map();
    let i = 0;
    while (i <= u8.length - 12) {
      if (u8[i + 4] === 4 && u8[i + 5] === 0 && u8[i + 6] === 0 && u8[i + 7] === 0) {
        const key = dv.getUint32(i, true);
        if (!fields.has(key)) fields.set(key, i + 8);
        i += 12;
      } else {
        i++;
      }
    }
    return { dv, fields };
  }

  const OPS = {
    MOREOREQUAL: (a, b) => a >= b, MORE: (a, b) => a > b, EQUAL: (a, b) => a === b,
    LESSOREQUAL: (a, b) => a <= b, LESS: (a, b) => a < b, EQUALNONZERO: (a, b) => a === b && a !== 0,
  };

  class SaveGame {
    constructor(u8) {
      Object.assign(this, readFields(u8));
    }
    int(key) {
      const at = this.fields.get(key);
      return at === undefined ? undefined : this.dv.getInt32(at, true);
    }
    lock(n) {
      return this.int(jshash('LOCK' + String(n - 1).padStart(3, '0'))) || 0;
    }
    stat(name) {
      const at = this.fields.get(jshash('Stat_' + name));
      if (at === undefined) return undefined;
      const i = this.dv.getInt32(at, true);
      return Math.abs(i) > 10000000 ? this.dv.getFloat32(at, true) : i;   // las distancias son float
    }
    evaluate(ev) {
      const d = ev.Data || {};
      if (ev.Type === 'Eval_Stat') {
        const v = this.stat(d.Stat);
        return v === undefined ? null : OPS[d.Operator](v, d.Value);
      }
      if (ev.Type === 'Eval_LogicAND') {
        const rs = (d.Children || []).map(c => this.evaluate(c));
        if (rs.includes(false)) return false;
        return rs.includes(null) ? null : true;
      }
      if (ev.Type === 'Eval_NumericCompare') {
        const [a, b] = d.Children.map(c => c.Type === 'EvalFetch_Stat' ? this.stat(c.Data.Stat) : c.Type === 'EvalFetch_Number' ? c.Data.Value : undefined);
        return a === undefined || b === undefined ? null : OPS[d.Operator](a, b);
      }
      return null;   // condiciones que dependen del estado del mundo
    }
  }

  // Devuelve los ids conseguidos según la partida
  async function readProgress(file, savemap) {
    const save = new SaveGame(await decompress(file));
    const OWNED = 5;
    const capsules = Object.entries(savemap.capsules).filter(([, l]) => save.lock(l) >= OWNED).map(([id]) => id);
    const discs = Object.entries(savemap.discs).filter(([, l]) => save.lock(l) >= OWNED).map(([id]) => id);
    const results = Object.entries(savemap.stars).map(([id, cond]) => [id, save.evaluate(cond)]);
    const stars = results.filter(([, r]) => r === true).map(([id]) => id);
    const unknown = results.filter(([, r]) => r === null).map(([id]) => id);
    const total = save.stat(savemap.starTotalStat);
    // Historia: los objetos únicos van uno a uno; las cartas de un nivel se marcan juntas al tenerlas todas,
    // porque el juego no dice qué carta está en qué punto del nivel
    const story = Object.entries(savemap.story.items).filter(([, l]) => save.lock(l) >= OWNED).map(([id]) => id);
    const storyCardsPartial = [];
    for (const lv of savemap.story.cards) {
      const have = lv.locks.filter(l => save.lock(l) >= OWNED).length;
      if (have === lv.locks.length) story.push(...lv.ids);
      else if (have > 0) storyCardsPartial.push({ level: lv.level, have, total: lv.locks.length });
    }
    return {
      capsules, discs, stars, story, storyCardsPartial, starsGame: total, starsChecked: total === stars.length,
      starsUnknown: total === stars.length ? [] : unknown, pictoDone: save.stat(savemap.pictoDoneStat),
    };
  }

  global.TS3Save = { readProgress, jshash };
})(window);
