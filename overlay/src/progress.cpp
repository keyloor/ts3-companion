#include "progress.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>

#include "inflate.h"
#include "json.h"

namespace {

std::string readFile(const std::wstring &path) {
    std::string s;
    FILE *f = _wfopen(path.c_str(), L"rb");
    if (!f) return s;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

std::wstring env(const wchar_t *name) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH);
    return n && n < MAX_PATH ? std::wstring(buf, n) : std::wstring();
}

std::wstring widen(const std::string &s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

// Hash de los nombres de campo de la partida (el mismo que app/save.js)
uint32_t jshash(const std::string &s) {
    uint32_t h = 0x4E67C6A7;
    for (unsigned char ch : s) {
        int c = ch >= 0x80 ? (int)ch - 0x100 : ch;
        h ^= (uint32_t)((h << 5) + (h >> 2) + (uint32_t)c);
    }
    return h;
}

// Campos de la partida: <clave u32><04 00 00 00><valor 4 bytes>; vale la primera aparición de cada clave
struct Save {
    std::unordered_map<uint32_t, uint32_t> f;
    const Json *modifiers = nullptr;

    bool open(const std::string &raw) {
        auto u32 = [](const uint8_t *p) { return (uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; };
        const uint8_t *d = (const uint8_t *)raw.data();
        if (raw.size() < 0x18) return false;
        size_t n1 = u32(d + 12);
        if (0x18 + n1 > raw.size()) return false;
        std::vector<uint8_t> outer, inner;
        if (!zlib_inflate(d + 0x18, n1, outer) || outer.size() < 0x14) return false;
        size_t n2 = u32(outer.data() + 8);
        if (0x14 + n2 > outer.size() || !zlib_inflate(outer.data() + 0x14, n2, inner)) return false;
        const uint8_t *p = inner.data();
        size_t len = inner.size();
        for (size_t i = 0; i + 12 <= len;) {
            if (p[i + 4] == 4 && p[i + 5] == 0 && p[i + 6] == 0 && p[i + 7] == 0) {
                f.emplace(u32(p + i), u32(p + i + 8));
                i += 12;
            } else i++;
        }
        return true;
    }
    int lock(int n) const {
        char key[16];
        snprintf(key, sizeof key, "LOCK%03d", n - 1);
        auto it = f.find(jshash(key));
        return it == f.end() ? 0 : (int32_t)it->second;
    }
    bool stat(const std::string &name, double &v) const {
        auto it = f.find(jshash("Stat_" + name));
        if (it == f.end()) return false;
        int32_t i = (int32_t)it->second;
        if (std::abs((long long)i) > 10000000) {
            float fl;
            memcpy(&fl, &it->second, 4);
            v = fl;
        } else v = i;
        return true;
    }
    // % de los objetos de un tema que tienes (EvalFetch_Locks, QueryMode 2)
    bool lockPct(const Json &q, double &v) const {
        if (!modifiers || q["QueryMode"].num() != 2) return false;
        std::string m = q["Modifier"].str();
        for (auto &c : m) c = (char)toupper((unsigned char)c);
        const Json &locks = (*modifiers)[m];
        if (!locks.size()) return false;
        int have = 0;
        for (auto &l : locks.arr) have += lock(l.num()) >= 5;
        v = 100.0 * have / locks.size();
        return true;
    }
    static bool op(const std::string &o, double a, double b) {
        if (o == "MOREOREQUAL") return a >= b;
        if (o == "MORE") return a > b;
        if (o == "EQUAL") return a == b;
        if (o == "LESSOREQUAL") return a <= b;
        if (o == "LESS") return a < b;
        if (o == "EQUALNONZERO") return a == b && a != 0;
        return false;
    }
    // 1 conseguida, 0 no, -1 no se puede saber con la partida
    int evaluate(const Json &ev) const {
        const std::string &t = ev["Type"].str();
        const Json &d = ev["Data"];
        if (t == "Eval_Stat") {
            double v;
            return stat(d["Stat"].str(), v) ? op(d["Operator"].str(), v, d["Value"].n) : -1;
        }
        if (t == "Eval_LogicAND") {
            bool unknown = false;
            for (auto &c : d["Children"].arr) {
                int r = evaluate(c);
                if (r == 0) return 0;
                if (r < 0) unknown = true;
            }
            return unknown ? -1 : 1;
        }
        if (t == "Eval_NumericCompare") {
            double v[2];
            const Json &ch = d["Children"];
            for (int k = 0; k < 2; k++) {
                const Json &c = ch[k];
                const std::string &ct = c["Type"].str();
                bool ok = ct == "EvalFetch_Stat" ? stat(c["Data"]["Stat"].str(), v[k])
                        : ct == "EvalFetch_Number" ? (v[k] = c["Data"]["Value"].n, true)
                        : ct == "EvalFetch_Locks" ? lockPct(c["Data"], v[k]) : false;
                if (!ok) return -1;
            }
            return op(d["Operator"].str(), v[0], v[1]);
        }
        return -1;
    }
};

// KeyValues binario de Steam (appcache/stats): 0 sección, 1 texto, 2/4/6 entero, 3 float, 7 uint64, 8 fin
struct KV {
    std::map<std::string, KV> sub;
    std::string s;
    long long i = 0;
};

bool readKV(const std::string &d, size_t &p, KV &out) {
    while (p < d.size()) {
        unsigned char t = d[p++];
        if (t == 8) return true;
        size_t z = d.find('\0', p);
        if (z == std::string::npos) return false;
        std::string key = d.substr(p, z - p);
        p = z + 1;
        KV &v = out.sub[key];
        if (t == 0) {
            if (!readKV(d, p, v)) return false;
        } else if (t == 1) {
            z = d.find('\0', p);
            if (z == std::string::npos) return false;
            v.s = d.substr(p, z - p);
            p = z + 1;
        } else if (t == 2 || t == 4 || t == 6 || t == 3) {
            if (p + 4 > d.size()) return false;
            int32_t x;
            memcpy(&x, d.data() + p, 4);
            v.i = x;
            p += 4;
        } else if (t == 7) {
            if (p + 8 > d.size()) return false;
            memcpy(&v.i, d.data() + p, 8);
            p += 8;
        } else return false;
    }
    return true;
}

// Nombres en inglés de los logros desbloqueados según la caché local de Steam (igual que desktop/companion.py)
bool steamAchievements(std::set<std::string> &names) {
    const unsigned APP = 4049180;
    HKEY k;
    wchar_t steam[MAX_PATH];
    DWORD size = sizeof steam, active = 0, asize = sizeof active;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &k)) return false;
    LONG r = RegQueryValueExW(k, L"SteamPath", nullptr, nullptr, (BYTE *)steam, &size);
    RegCloseKey(k);
    if (r) return false;
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", 0, KEY_READ, &k)) {
        RegQueryValueExW(k, L"ActiveUser", nullptr, nullptr, (BYTE *)&active, &asize);
        RegCloseKey(k);
    }
    std::wstring stats = std::wstring(steam) + L"\\appcache\\stats\\";
    std::wstring schemaPath = stats + L"UserGameStatsSchema_" + std::to_wstring(APP) + L".bin";
    std::wstring user = stats + L"UserGameStats_" + std::to_wstring(active) + L"_" + std::to_wstring(APP) + L".bin";
    if (GetFileAttributesW(user.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // el usuario más reciente
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((stats + L"UserGameStats_*_" + std::to_wstring(APP) + L".bin").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return false;
        FILETIME best = {};
        do {
            if (CompareFileTime(&fd.ftLastWriteTime, &best) > 0) { best = fd.ftLastWriteTime; user = stats + fd.cFileName; }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::string sch = readFile(schemaPath), usr = readFile(user);
    if (sch.empty() || usr.empty()) return false;
    KV s, u;
    size_t p = 0;
    if (!readKV(sch, p, s) || s.sub.empty()) return false;
    p = 0;
    if (!readKV(usr, p, u)) return false;
    const KV &schema = s.sub.begin()->second;
    auto cache = u.sub.find("cache");
    auto st = schema.sub.find("stats");
    if (st == schema.sub.end()) return true;
    for (auto &kv : st->second.sub) {
        long long data = 0;
        if (cache != u.sub.end()) {
            auto e = cache->second.sub.find(kv.first);
            if (e != cache->second.sub.end()) {
                auto dv = e->second.sub.find("data");
                if (dv != e->second.sub.end()) data = dv->second.i;
            }
        }
        auto bits = kv.second.sub.find("bits");
        if (bits == kv.second.sub.end()) continue;
        for (auto &b : bits->second.sub) {
            if (!((data >> atoi(b.first.c_str())) & 1)) continue;
            auto disp = b.second.sub.find("display");
            if (disp == b.second.sub.end()) continue;
            auto nm = disp->second.sub.find("name");
            if (nm == disp->second.sub.end()) continue;
            auto en = nm->second.sub.find("english");
            if (en != nm->second.sub.end()) names.insert(en->second.s);
        }
    }
    return true;
}

void replaceAll(std::string &s, const std::string &a, const std::string &b) {
    for (size_t p = 0; (p = s.find(a, p)) != std::string::npos; p += b.size()) s.replace(p, a.size(), b);
}

}  // namespace

// content.json y savemap.json se leen una sola vez (los usan a la vez el hilo que lee la partida y el del dibujo)
static Json g_content, g_savemap;
static bool g_loaded;
static SRWLOCK g_loadLock = SRWLOCK_INIT;

// Textos de las dos lenguas desde {"es": ..., "en": ...} o un texto único
static void both(Text &out, const Json &j) {
    if (j.kind == Json::Obj) { out[0] = j["es"].str(); out[1] = j["en"].str(); }
    else out[0] = out[1] = j.str();
}
static void set2(Text &out, const std::string &es, const std::string &en) { out[0] = es; out[1] = en; }

// Iconos del juego por categoría de cápsula (en inglés)
static const char *catIcon(const std::string &en) {
    static const char *map[][2] = {{"Outfit", "icon_customizeblockheadclothes"}, {"Alien outfit", "icon_customizeblockheadclothes"},
        {"Hair", "icon_customizeblockheadhair"}, {"Alien hair", "icon_customizeblockheadhair"}, {"Hat", "icon_customizeblockheadhat"},
        {"Alien hat", "icon_customizeblockheadhat"}, {"Yard", "icon_customizelawnornament"}, {"Paint", "icon_paintbrush"},
        {"Windows", "icon_customizewindows"}, {"Doors", "icon_customizedoor"}, {"Roof", "icon_customizetrim"},
        {"Porch", "icon_customizeattachments"}, {"Sides", "icon_customizewalls"}};
    for (auto &m : map)
        if (en == m[0]) return m[1];
    return "";
}

bool Progress::load(const std::wstring &dataDir) {
    AcquireSRWLockExclusive(&g_loadLock);
    if (!g_loaded) {
        g_loaded = Json::parse(readFile(dataDir + L"\\content.json"), g_content) && Json::parse(readFile(dataDir + L"\\savemap.json"), g_savemap);
    }
    bool ok = g_loaded;
    ReleaseSRWLockExclusive(&g_loadLock);
    if (!ok) {
        status = "content.json / savemap.json";
        return false;
    }
    version = g_content["version"].str();
    sections.clear();
    types.clear();
    const Json &C = g_content;

    // Tipos de coleccionable de la historia
    struct { const char *key, *icon; } tdef[] = {{"CC", "icon_collectcardsingle"}, {"SP", "icon_collectspecialsingle"},
        {"TP", "icon_collectblockhead"}, {"AM", "icon_collectandy"}, {"ZP", "icon_collectzurg"}, {"AL", "icon_collectlgm"}};
    for (auto &t : tdef) {
        const Json &j = C["storyTypes"][t.key];
        StoryType st;
        st.key = t.key;
        st.icon = t.icon;
        set2(st.name, j["es"]["name"].str(), j["en"]["name"].str());
        set2(st.one, j["es"]["one"].str(), j["en"]["one"].str());
        types.push_back(st);
    }

    // Logros, por grupos; el trofeo indica la rareza (porcentaje de jugadores en Steam)
    Section ach;
    ach.key = "achievements";
    ach.icon = "icon_trophygold";
    set2(ach.title, "Logros", "Achievements");
    for (auto &g : C["achievementGroups"].obj) {
        Group gr;
        gr.key = g.first;
        both(gr.title, g.second);
        for (auto &a : C["achievements"].arr) {
            if (a["group"].str() != g.first) continue;
            Item it;
            it.id = a["id"].str();
            both(it.title, a["name"]);
            if (it.title[0] != it.title[1]) it.sub[0] = it.title[1];   // en español se ve también el nombre de Steam
            both(it.text, a["desc"]);
            both(it.tip, a["tip"]);
            bool hidden = a["pct"].isNull();
            double p = a["pct"].n;
            const char *r = hidden ? "P" : p < 10 ? "G" : p < 30 ? "S" : "B";
            static const char *trophy[] = {"icon_trophygold", "icon_trophysilver", "icon_trophybronze", "icon_trophynone"};
            static const char *rname[][2] = {{"Raro", "Rare"}, {"Poco com\xC3\xBAn", "Uncommon"}, {"Com\xC3\xBAn", "Common"}, {"Oculto en Steam", "Hidden on Steam"}};
            int ri = *r == 'G' ? 0 : *r == 'S' ? 1 : *r == 'B' ? 2 : 3;
            it.lead = std::string("icon:") + trophy[ri];
            Flag rf;
            set2(rf.text, rname[ri][0], rname[ri][1]);
            if (!hidden) {
                char es[32], en[32];
                snprintf(en, sizeof en, " \xC2\xB7 %.1f %%", p);
                snprintf(es, sizeof es, " \xC2\xB7 %.1f %%", p);
                for (char *c = es; *c; c++) if (*c == '.') *c = ',';
                rf.text[0] += es;
                rf.text[1] += en;
            }
            it.flags.push_back(rf);
            std::string link = a["link"].str();
            it.link = link;
            it.prog = a["prog"].str();
            if (it.id == "t-airborn-ranger") it.prog = "para";
            gr.items.push_back(it);
        }
        ach.groups.push_back(gr);
    }

    // Historia, por niveles
    Section story;
    story.key = "story";
    story.icon = "icon_collectcards";
    set2(story.title, "Historia", "Story");
    for (auto &lv : C["story"].arr) {
        Group gr;
        gr.key = "l" + std::to_string(lv["n"].num());
        gr.num = lv["n"].num();
        both(gr.title, lv["name"]);
        gr.sub[0] = gr.sub[1] = lv["sub"].str();
        int n = 0;
        for (auto &s : lv["items"].arr) {
            Item it;
            it.id = s["id"].str();
            it.type = s["type"].str();
            n++;
            it.lead = s["icon"].str().empty() ? "num:" + std::to_string(n) : "art:" + s["icon"].str();
            for (auto &t : types)
                if (t.key == it.type) {
                    Badge b;
                    b.text[0] = t.one[0];
                    b.text[1] = t.one[1];
                    b.icon = t.icon;
                    b.type = t.key;
                    it.badges.push_back(b);
                }
            both(it.text, s["hint"]);
            for (const char *k : {"miss", "trophy", "note"}) {
                Flag f;
                f.text[0] = s["flags"]["es"][k].str();
                f.text[1] = s["flags"]["en"][k].str();
                f.kind = *k == 'm' ? 'm' : *k == 't' ? 't' : 0;
                if (!f.text[0].empty() || !f.text[1].empty()) {
                    if (f.text[0].empty()) f.text[0] = f.text[1];
                    if (f.text[1].empty()) f.text[1] = f.text[0];
                    it.flags.push_back(f);
                }
            }
            gr.items.push_back(it);
        }
        story.groups.push_back(gr);
    }

    // Cápsulas, por zonas
    Section caps;
    caps.key = "capsules";
    caps.icon = "icon_gift";
    set2(caps.title, "C\xC3\xA1psulas", "Capsules");
    int ai = 0;
    for (auto &a : C["capsules"].arr) {
        Group gr;
        gr.key = a["key"].str();
        gr.num = ++ai;
        both(gr.title, a["name"]);
        both(gr.intro, a["intro"]);
        for (auto &c : a["items"].arr) {
            Item it;
            it.id = c["id"].str();
            it.lead = c["code"].str() == "!" ? "icon:icon_gift" : "code:" + c["code"].str();
            it.title[0] = it.title[1] = c["name"].str();
            both(it.cat, c["cat"]);
            it.type = it.cat[1];
            Badge b;
            b.text[0] = it.cat[0];
            b.text[1] = it.cat[1];
            b.icon = catIcon(it.cat[1]);
            it.badges.push_back(b);
            both(it.text, c["hint"]);
            it.tip[0] = it.tip[1] = c["note"].str();
            gr.items.push_back(it);
        }
        caps.groups.push_back(gr);
    }

    // Estrellas doradas, por tarea
    Section stars;
    stars.key = "stars";
    stars.icon = "icon_stargold";
    set2(stars.title, "Estrellas", "Gold Stars");
    for (auto &g : C["stars"].arr) {
        Group gr;
        gr.key = g["key"].str();
        both(gr.title, g["title"]);
        both(gr.intro, g["intro"]);
        for (auto &s : g["items"].arr) {
            Item it;
            it.id = s["id"].str();
            it.title[0] = it.title[1] = s["name"].str();
            both(it.text, s["desc"]);
            both(it.tip, s["tip"]);
            it.link = s["link"].str();
            if (it.id == "g-parachute-king") it.prog = "para";
            gr.items.push_back(it);
        }
        stars.groups.push_back(gr);
    }

    // Pict-O-Matic, por discos: el disco y sus escenas
    Section picto;
    picto.key = "picto";
    picto.icon = "icon_pictomatic";
    set2(picto.title, "Pict-O-Matic", "Pict-O-Matic");
    both(picto.intro, C["picto"]["intro"]);
    for (auto &d : C["picto"]["discs"].arr) {
        Group gr;
        int n = d["n"].num();
        gr.key = "d" + std::to_string(n);
        gr.num = n;
        set2(gr.title, "Disco " + std::to_string(n), "Disc " + std::to_string(n));
        int scenes = (int)d["missions"].size();
        set2(gr.sub, std::to_string(scenes) + " escenas", std::to_string(scenes) + " scenes");
        Item disc;
        disc.id = "pd-" + std::to_string(n);
        disc.lead = "icon:icon_pictomatic";
        set2(disc.title, "Disco encontrado", "Disc found");
        both(disc.text, d["where"]);
        gr.items.push_back(disc);
        for (auto &m : d["missions"].arr) {
            Item it;
            it.id = m["id"].str();
            it.title[0] = it.title[1] = m["name"].str();
            both(it.text, m["req"]);
            gr.items.push_back(it);
        }
        picto.groups.push_back(gr);
    }

    sections = {ach, story, caps, stars, picto};
    return true;
}

Item *Progress::find(const std::string &id) {
    for (auto &s : sections)
        for (auto &g : s.groups)
            for (auto &it : g.items)
                if (it.id == id) return &it;
    return nullptr;
}

void Progress::recount() {
    done = total = 0;
    for (auto &s : sections) {
        s.done = s.total = 0;
        for (auto &g : s.groups) {
            g.done = 0;
            for (auto &it : g.items) g.done += it.done;
            s.done += g.done;
            s.total += (int)g.items.size();
        }
        done += s.done;
        total += s.total;
    }
}

static std::wstring progressPath() { return env(L"APPDATA") + L"\\TS3Companion\\progress.json"; }

static std::set<std::string> readChecked(bool *valid = nullptr) {
    std::set<std::string> out;
    Json prog;
    bool ok = Json::parse(readFile(progressPath()), prog) && prog["checked"].kind == Json::Arr;
    if (ok)
        for (auto &id : prog["checked"].arr) out.insert(id.str());
    if (valid) *valid = ok;
    return out;
}

void Progress::refresh(const std::wstring &savePathIn) {
    std::set<std::string> got;   // según la partida y Steam
    status.clear();
    std::wstring appdata = env(L"APPDATA");
    std::set<std::string> manual = readChecked();

    // La partida: la indicada, la que eligió la app o la ubicación estándar
    std::wstring savePath = savePathIn;
    if (savePath.empty()) {
        Json cfg;
        if (Json::parse(readFile(appdata + L"\\TS3Companion\\config.json"), cfg) && !cfg["save"].str().empty())
            savePath = widen(cfg["save"].str());
        if (savePath.empty() || GetFileAttributesW(savePath.c_str()) == INVALID_FILE_ATTRIBUTES)
            savePath = appdata + L"\\Disney\\Toy Story 3 Complete Edition\\savedata\\Saved Game";
    }
    Save save;
    save.modifiers = &g_savemap["modifiers"];
    std::string raw = readFile(savePath);
    saveFound = !raw.empty() && save.open(raw);
    starsComputed = 0;
    starsGame = pictoDone = para = -1;
    if (saveFound) {
        const Json &M = g_savemap;
        for (auto &kv : M["capsules"].obj)
            if (save.lock(kv.second.num()) >= 5) got.insert(kv.first);
        for (auto &kv : M["discs"].obj)
            if (save.lock(kv.second.num()) >= 5) got.insert(kv.first);
        for (auto &kv : M["stars"].obj)
            if (save.evaluate(kv.second) == 1) { got.insert(kv.first); starsComputed++; }
        double v;
        if (save.stat(M["starTotalStat"].str(), v)) starsGame = (int)v;
        if (save.stat(M["pictoDoneStat"].str(), v)) pictoDone = (int)v;
        const Json &pc = M["counters"]["para"];
        if (save.stat(pc["stat"].str(), v)) para = (int)v;
        if (pc["total"].num()) paraTotal = pc["total"].num();
        // Historia: los objetos únicos uno a uno; las cartas de un nivel, todas juntas al tenerlas todas
        for (auto &kv : M["story"]["items"].obj)
            if (save.lock(kv.second.num()) >= 5) got.insert(kv.first);
        for (auto &lv : M["story"]["cards"].arr) {
            bool all = true;
            for (auto &l : lv["locks"].arr) all = all && save.lock(l.num()) >= 5;
            if (all)
                for (auto &id : lv["ids"].arr) got.insert(id.str());
        }
    } else status = "save";

    // Logros de Steam
    std::set<std::string> names;
    steamFound = steamAchievements(names);
    for (auto &a : g_content["achievements"].arr)
        if (names.count(a["name"]["en"].str())) got.insert(a["id"].str());

    for (auto &s : sections)
        for (auto &g : s.groups)
            for (auto &it : g.items) {
                it.fromSave = got.count(it.id) > 0;
                it.done = it.fromSave || manual.count(it.id) > 0;
            }
    recount();
}

// Igual que la app de escritorio (desktop/companion.py): se aplica el cambio a la lista guardada, se copia la versión
// anterior a progress.json.bak y se escribe por un archivo temporal
bool Progress::setManual(const std::string &id, bool on) {
    Item *it = find(id);
    if (!it || (!on && it->fromSave)) return false;
    bool valid;
    std::set<std::string> ids = readChecked(&valid);
    if (on) ids.insert(id);
    else ids.erase(id);
    std::wstring path = progressPath(), dir = env(L"APPDATA") + L"\\TS3Companion";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::string body = "{\"checked\": [";
    bool first = true;
    for (auto &x : ids) {
        if (!first) body += ", ";
        first = false;
        body += "\"";
        for (char c : x) {
            if (c == '"' || c == '\\') body += '\\';
            body += c;
        }
        body += "\"";
    }
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[64];
    snprintf(stamp, sizeof stamp, "%04d-%02d-%02dT%02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    body += std::string("], \"updated\": \"") + stamp + "\"}";
    std::wstring tmp = path + L".tmp";
    FILE *f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return false;
    bool wrote = fwrite(body.data(), 1, body.size(), f) == body.size();
    fclose(f);
    if (!wrote) return false;
    if (valid) CopyFileW(path.c_str(), (path + L".bak").c_str(), FALSE);
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) return false;
    it->done = on || it->fromSave;
    recount();
    return true;
}
