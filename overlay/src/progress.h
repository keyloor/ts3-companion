// Datos del Companion (content.json + savemap.json) y lo conseguido según la partida, la caché de logros de Steam
// y las marcas guardadas por la app (progress.json). Misma lógica que app/save.js y app/app.js.
// Las marcas a mano se escriben en el mismo progress.json que usa la app de escritorio.
#pragma once
#include <array>
#include <string>
#include <vector>

// Textos en los dos idiomas: [0] español, [1] inglés
typedef std::array<std::string, 2> Text;

struct Badge {
    Text text;
    std::string icon;          // icono del juego (o vacío)
    std::string type;          // tipo de coleccionable (su color en el tema); vacío = insignia neutra
};

struct Flag {
    Text text;
    char kind = 0;             // 0 normal, 'm' se puede perder, 't' logro
};

struct Item {
    std::string id;
    Text title, sub, text, tip;
    std::string lead;          // "num:3", "code:CP12", "icon:<nombre>", "art:<nombre>"
    std::string type;          // historia: CC, SP...; cápsulas: categoría en inglés
    Text cat;                  // categoría de la cápsula (para el filtro)
    std::vector<Badge> badges;
    std::vector<Flag> flags;   // rareza y avisos («Se puede perder», «Logro»...)
    std::string link;          // pestaña a la que lleva («Ir a…»)
    std::string prog;          // contador que se muestra (para, story, stars...)
    bool done = false;
    bool fromSave = false;     // conseguido según la partida o Steam (la marca no se puede quitar a mano)
    float layoutH = 0;         // altura medida por el panel (ui.cpp) y para qué disposición
    int layoutGen = -1;
};

struct Group {
    std::string key;
    Text title, sub, intro;
    int num = 0;               // número del nivel/zona/disco (0 = icono de la pestaña)
    std::vector<Item> items;
    int done = 0;
};

struct Section {
    std::string key, icon;
    Text title, intro;
    std::vector<Group> groups;
    int done = 0, total = 0;
};

struct StoryType {
    std::string key, icon;
    Text name, one;
};

struct Progress {
    std::vector<Section> sections;     // achievements, story, capsules, stars, picto
    std::vector<StoryType> types;
    int done = 0, total = 0;
    int starsComputed = 0, starsGame = -1, pictoDone = -1;
    int para = -1, paraTotal = 30;     // army men (paracaidistas)
    bool saveFound = false, steamFound = false;
    std::string status;
    std::string version;

    // dataDir: carpeta con content.json y savemap.json. Los textos conservan sus marcas ([X], {CP12}, {START}): las dibuja el panel
    bool load(const std::wstring &dataDir);
    // Vuelve a leer la partida, Steam y progress.json. savePath vacío = la de la app o la ubicación estándar
    void refresh(const std::wstring &savePath);
    // Marca o desmarca a mano (progress.json); false si no se pudo guardar
    bool setManual(const std::string &id, bool on);
    Item *find(const std::string &id);
    void recount();
};
