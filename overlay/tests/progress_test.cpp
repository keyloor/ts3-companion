// Prueba del lector de la partida fuera del juego: imprime lo conseguido por sección, para compararlo con
// tools/sync_cli.py. Uso: progress_test.exe <carpeta con content.json y savemap.json> [partida]
#include <cstdio>
#include <string>

#include "../src/progress.h"

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) return 2;
    Progress p;
    if (!p.load(argv[1])) { printf("load failed: %s\n", p.status.c_str()); return 1; }
    p.refresh(argc > 2 ? argv[2] : L"");
    printf("version %s  save %d  steam %d  status '%s'\n", p.version.c_str(), p.saveFound, p.steamFound, p.status.c_str());
    for (auto &s : p.sections) printf("%-12s %d/%d\n", s.key.c_str(), s.done, s.total);
    printf("overall %d/%d  stars computed %d  game %d\n", p.done, p.total, p.starsComputed, p.starsGame);
    for (auto &s : p.sections)
        for (auto &g : s.groups)
            for (auto &it : g.items)
                if (!it.done && s.key != "picto") printf("  missing %s %s | %s\n", it.id.c_str(), it.title[1].c_str(), it.text[1].substr(0, 60).c_str());
    return 0;
}
