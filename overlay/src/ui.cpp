// El Companion dentro del juego: la misma interfaz que la app (app/styles.css y app/app.js, con sus medidas en píxeles CSS,
// sus fuentes y sus temas claro y oscuro) dibujada con Dear ImGui. ImGui solo aporta la entrada y unos pocos controles
// (buscador, desplegable): el resto se dibuja aquí a mano, por eso los colores y tamaños se leen igual que en el CSS.
// Se maneja con el ratón (cursor del juego), el teclado o el mando; mientras está abierto el juego no recibe nada
// (main.cpp, d3d12.cpp).
#include <windows.h>
#include <xinput.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "gamefiles.h"
#include "imgui.h"
#include "overlay.h"
#include "progress.h"

// overlay/fonts, metidas en la DLL por overlay/build.py
struct EmbeddedFont { const unsigned char *data; int size; float ascent, descent; };
extern const EmbeddedFont embeddedFonts[];

namespace overlay {

// ---------- estado ----------
static int tab = 1;                    // 0 Ruta, 1.. secciones de Progress
static std::set<std::string> typeFilter;   // historia: tipos elegidos (vacío = todos)
static std::string catFilter;          // cápsulas: categoría en inglés (vacío = todas)
static std::set<std::string> collapsed;    // "pestaña:grupo"
static char search[8][96];
static float S = 1.0f, screenH = 1080;  // S: píxeles de pantalla por píxel CSS
static bool lastWasPad, settingsOpen, themeDirty = true;
static SYSTEMTIME readAt;
static int layoutGen;                  // cambia cuando hay que volver a medir los elementos
static std::vector<ImVec2> demoClicks; // pruebas: clics escritos en TS3C_OVERLAY_DEMO (click=x:y), sin tocar el ratón real

static bool es() { return cfg.lang == "es"; }
static const char *L(const char *s, const char *e) { return es() ? s : e; }
static int li() { return es() ? 0 : 1; }
static const std::string &T(const Text &t) { return t[li()].empty() ? t[0] : t[li()]; }

static void saveIni(const wchar_t *key, const std::string &value) {
    std::wstring v(value.begin(), value.end());
    WritePrivateProfileStringW(L"Overlay", key, v.c_str(), (dataDir + L"\\overlay.ini").c_str());
}

// ---------- datos: se leen en otro hilo para no trabar el juego ----------
static Progress prog, pending;
static std::map<std::string, const Item *> capIndex;   // para las marcas {CP12} dentro de los textos
static volatile LONG refreshState;     // 0 libre, 1 leyendo, 2 listo para usar
static bool contentLoaded, contentFailed;

static DWORD WINAPI refreshThread(void *) {
    if (!contentLoaded) {
        contentLoaded = true;
        contentFailed = !pending.load(dataDir);
        if (contentFailed) log("Could not read content.json / savemap.json in the overlay folder");
    }
    if (!contentFailed) pending.refresh(cfg.savePath);
    InterlockedExchange(&refreshState, 2);
    return 0;
}

static void requestRefresh() {
    if (InterlockedCompareExchange(&refreshState, 1, 0) == 0) {
        if (!contentFailed && contentLoaded && !prog.sections.empty()) pending = prog;
        CloseHandle(CreateThread(nullptr, 0, refreshThread, nullptr, 0, nullptr));
    }
}

void toggle() {
    visible = !visible;
    cursorFree(visible);
    settingsOpen = false;
    if (visible) { themeDirty = true; requestRefresh(); }
}

void demo(const char *spec) {
    tab = atoi(spec) % 6;
    cfg.hideDone = strstr(spec, "hide") != nullptr;
    if (strstr(spec, "light")) cfg.theme = "light";
    if (strstr(spec, "dark")) cfg.theme = "dark";
    settingsOpen = false;
    log("Demo mode: %s", spec);
    toggle();
    settingsOpen = strstr(spec, "settings") != nullptr;
    for (const char *c = strstr(spec, "click="); c; c = strstr(c + 1, "click="))
        demoClicks.push_back(ImVec2((float)atof(c + 6), (float)atof(strchr(c, ':') + 1)));
}

bool uiWantsFrame() { return visible; }   // nada de GPU hasta que se abre el panel

// ---------- colores: las variables de app/styles.css ----------
struct Pal {
    unsigned sky, cloud, paper, paper2, ink, soft, line, woody, accent, accentInk, cc, sp, tp, am, zp, al, onTag;
    float shadow;                      // opacidad de la sombra de las tarjetas
    unsigned shadowColor;
};
static const Pal LIGHT = {0xc9e3f6, 0xeaf5fd, 0xffffff, 0xf2f7fc, 0x1b2742, 0x586a88, 0xd5e2ef, 0xf0b000, 0x2c6bd3, 0x1d55b8,
                          0x2c6bd3, 0xc98a00, 0xcc322a, 0xe0661b, 0x7a3fc4, 0x23913b, 0xffffff, 0.35f, 0x1b2742};
static const Pal DARK = {0x0e1829, 0x142239, 0x16223a, 0x1b2a46, 0xe7edf8, 0x9badc9, 0x283a5a, 0xf3bf2e, 0xf3bf2e, 0xf3bf2e,
                         0x78a6ff, 0xf0c24f, 0xff7a70, 0xe0a56f, 0xb994ff, 0x63d47e, 0x0e1829, 0.7f, 0x000000};
static const Pal *P = &DARK;
static const unsigned ON_WOODY = 0x2a1d00;

static ImVec4 rgb(unsigned hex, float a = 1.0f) { return ImVec4(((hex >> 16) & 255) / 255.0f, ((hex >> 8) & 255) / 255.0f, (hex & 255) / 255.0f, a); }
static ImU32 col(unsigned hex, float a = 1.0f) {
    return IM_COL32((hex >> 16) & 255, (hex >> 8) & 255, hex & 255, (int)(a * 255 + 0.5f));
}
// color-mix(in srgb, c pct%, over)
static unsigned mix(unsigned c, float pct, unsigned over) {
    auto ch = [&](int shift) { return (unsigned)(((c >> shift) & 255) * pct + ((over >> shift) & 255) * (1 - pct) + 0.5f) << shift; };
    return ch(16) | ch(8) | ch(0);
}
static unsigned typeColor(const std::string &k) {
    return k == "CC" ? P->cc : k == "SP" ? P->sp : k == "TP" ? P->tp : k == "AM" ? P->am : k == "ZP" ? P->zp : k == "AL" ? P->al : P->soft;
}

static bool systemIsLight() {
    DWORD v = 1, size = sizeof v;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                 RRF_RT_REG_DWORD, nullptr, &v, &size);
    return v != 0;
}

static void applyTheme() {
    themeDirty = false;
    P = cfg.theme == "light" ? &LIGHT : cfg.theme == "dark" ? &DARK : systemIsLight() ? &LIGHT : &DARK;
    ImVec4 *c = ImGui::GetStyle().Colors;
    c[ImGuiCol_WindowBg] = rgb(P->sky);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = rgb(P->paper);
    c[ImGuiCol_Border] = rgb(P->line);
    c[ImGuiCol_Text] = rgb(P->ink);
    c[ImGuiCol_TextDisabled] = rgb(P->soft);
    c[ImGuiCol_FrameBg] = rgb(P->paper);
    c[ImGuiCol_FrameBgHovered] = rgb(P->paper);
    c[ImGuiCol_FrameBgActive] = rgb(P->paper);
    c[ImGuiCol_Button] = rgb(P->paper);
    c[ImGuiCol_ButtonHovered] = rgb(P->paper2);
    c[ImGuiCol_ButtonActive] = rgb(P->paper2);
    c[ImGuiCol_Header] = rgb(P->paper2);
    c[ImGuiCol_HeaderHovered] = rgb(P->paper2);
    c[ImGuiCol_HeaderActive] = rgb(P->paper2);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = rgb(P->soft, 0.45f);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(P->soft, 0.7f);
    c[ImGuiCol_ScrollbarGrabActive] = rgb(P->soft, 0.9f);
    c[ImGuiCol_NavCursor] = rgb(P->cc);
    c[ImGuiCol_TextSelectedBg] = rgb(P->cc, 0.35f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(8 / 255.0f, 16 / 255.0f, 32 / 255.0f, 0.42f);
}

// ---------- arte del juego: un atlas con los iconos, las imágenes de coleccionables y el cursor ----------
// Cada imagen va en tres tamaños: sin mipmaps, un icono grande reducido a 20 píxeles se vería con dientes
static ImTextureID atlas;
struct Cell { ImVec2 uv0[3], uv1[3]; int w; };
static std::map<std::string, Cell> cells;

static Image halve(const Image &in) {
    Image out;
    out.w = std::max(1, in.w / 2);
    out.h = std::max(1, in.h / 2);
    out.rgba.resize((size_t)out.w * out.h * 4);
    for (int y = 0; y < out.h; y++)
        for (int x = 0; x < out.w; x++) {
            unsigned r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < 4; k++) {   // ponderado por la transparencia, para que no salgan bordes oscuros
                const uint8_t *p = &in.rgba[((size_t)std::min(y * 2 + (k >> 1), in.h - 1) * in.w + std::min(x * 2 + (k & 1), in.w - 1)) * 4];
                r += p[0] * p[3]; g += p[1] * p[3]; b += p[2] * p[3]; a += p[3];
            }
            uint8_t *o = &out.rgba[((size_t)y * out.w + x) * 4];
            o[0] = a ? (uint8_t)(r / a) : 0; o[1] = a ? (uint8_t)(g / a) : 0; o[2] = a ? (uint8_t)(b / a) : 0; o[3] = (uint8_t)(a / 4);
        }
    return out;
}

void uiLoadArt() {
    std::set<std::string> want = {"icon_compass", "icon_trophygold", "icon_trophysilver", "icon_trophybronze", "icon_trophynone",
                                  "icon_collectcards", "icon_gift", "icon_stargold", "icon_pictomatic", "icon_customizeblockheadclothes",
                                  "icon_customizeblockheadhair", "icon_customizeblockheadhat", "icon_customizelawnornament", "icon_paintbrush",
                                  "icon_customizewindows", "icon_customizedoor", "icon_customizetrim", "icon_customizeattachments",
                                  "icon_customizewalls", "icon_collectcardsingle", "icon_collectspecialsingle", "icon_collectblockhead",
                                  "icon_collectandy", "icon_collectzurg", "icon_collectlgm"};
    // Las imágenes de los coleccionables únicos de la historia (content.json) y el cursor
    Progress tmp;
    if (tmp.load(dataDir))
        for (auto &s : tmp.sections)
            for (auto &g : s.groups)
                for (auto &it : g.items)
                    if (it.lead.compare(0, 4, "art:") == 0) want.insert(it.lead.substr(4));
    std::wstring ui = dataDir.substr(0, dataDir.find_last_of(L'\\')) + L"\\assets\\ui\\";
    GameZip icons, cards, cursor;
    bool haveIcons = icons.open(ui + L"icons\\icons.zip"), haveCards = cards.open(ui + L"alscards\\alscards.zip"),
         haveCursor = cursor.open(ui + L"pointercursor\\pointercursor.zip");
    std::vector<std::pair<std::string, Image>> imgs;
    for (auto &name : want) {
        std::vector<uint8_t> oct;
        Image img;
        const GameZip *z = name.compare(0, 4, "smc_") == 0 ? (haveCards ? &cards : nullptr) : (haveIcons ? &icons : nullptr);
        if (z && z->read(name + ".oct", oct) && octImage(oct, 128, img)) imgs.push_back({name, img});
    }
    for (const char *c : {"arrow", "pointer"}) {
        std::vector<uint8_t> oct;
        Image img;
        if (haveCursor && cursor.read(std::string(c) + ".oct", oct) && octImage(oct, 128, img)) imgs.push_back({std::string("cursor_") + c, img});
    }
    if (imgs.empty()) { log("Game art not found: the panel uses plain text"); return; }
    // Bandas una debajo de otra: 8 celdas de 128 por fila, 16 de 64, 32 de 32
    const int W = 1024, N = (int)imgs.size();
    int rows[3], top[3], H = 0;
    for (int l = 0; l < 3; l++) {
        int cols = 8 << l;
        rows[l] = (N + cols - 1) / cols;
        top[l] = H;
        H += rows[l] * (128 >> l);
    }
    std::vector<uint8_t> px((size_t)W * H * 4, 0);
    for (int k = 0; k < N; k++) {
        Image im = imgs[k].second;
        Cell &cell = cells[imgs[k].first];
        cell.w = im.w;
        for (int l = 0; l < 3; l++) {
            int size = 128 >> l, cols = 8 << l, cx = (k % cols) * size, cy = top[l] + (k / cols) * size;
            for (int y = 0; y < im.h && y < size; y++)
                memcpy(&px[((size_t)(cy + y) * W + cx) * 4], &im.rgba[(size_t)y * im.w * 4], (size_t)std::min(im.w, size) * 4);
            cell.uv0[l] = ImVec2((float)cx / W, (float)cy / H);
            cell.uv1[l] = ImVec2((float)(cx + im.w) / W, (float)(cy + im.h) / H);
            if (l < 2) im = halve(im);
        }
    }
    atlas = (ImTextureID)uploadTexture(px.data(), W, H);
    log("Game art: %u images%s", (unsigned)imgs.size(), atlas ? "" : " (upload failed)");
    if (!atlas) cells.clear();
}

static bool hasArt(const std::string &name) { return atlas && cells.count(name); }
static void art(ImDrawList *dl, const std::string &name, float x, float y, float size, float alpha = 1.0f) {
    if (!hasArt(name)) return;
    const Cell &c = cells[name];
    int l = 0;
    while (l < 2 && (c.w >> (l + 1)) >= size) l++;
    dl->AddImage(atlas, ImVec2(x, y), ImVec2(x + size, y + size), c.uv0[l], c.uv1[l], IM_COL32(255, 255, 255, (int)(alpha * 255)));
}

// ---------- fuentes ----------
enum Fn { F500, F700, F800, FDISP, FPAD, FMONO, FCOUNT };
struct Face { ImFont *font; float asc, desc; };
static Face faces[FCOUNT];

void uiSetup(float height) {
    screenH = height;
    ImGuiIO &io = ImGui::GetIO();
    wchar_t win[MAX_PATH];
    GetWindowsDirectoryW(win, MAX_PATH);
    char fonts[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, win, -1, fonts, sizeof fonts, nullptr, nullptr);
    std::string dir = std::string(fonts) + "\\Fonts\\";
    auto exists = [](const std::string &p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; };
    std::string sym = dir + "seguisym.ttf", mono = dir + "consolab.ttf";
    // ImGui entiende el tamaño como altura total de la fuente, y CSS como em: ExtraSizeScale deja el tamaño en em
    for (int i = 0; i < 5; i++) {
        const EmbeddedFont &e = embeddedFonts[i];
        ImFontConfig c;
        c.FontDataOwnedByAtlas = false;
        c.ExtraSizeScale = e.ascent + e.descent;
        faces[i] = {io.Fonts->AddFontFromMemoryTTF((void *)e.data, e.size, 16.0f, &c), e.ascent, e.descent};
        if (i <= F800 && exists(sym)) {   // flechas y demás símbolos que no están en Nunito
            ImFontConfig m;
            m.MergeMode = true;
            m.ExtraSizeScale = 1.33f;
            io.Fonts->AddFontFromFileTTF(sym.c_str(), 16.0f, &m);
        }
    }
    faces[FMONO] = faces[F800];
    if (exists(mono))
        if (ImFont *f = io.Fonts->AddFontFromFileTTF(mono.c_str(), 16.0f)) faces[FMONO] = {f, 0.743f, 0.257f};
    io.FontDefault = faces[F700].font;
    ImGuiStyle &st = ImGui::GetStyle();
    st.WindowRounding = 16;
    st.ChildRounding = 14;
    st.FrameRounding = 10;
    st.PopupRounding = 10;
    st.GrabRounding = 6;
    st.WindowBorderSize = 1;
    st.ChildBorderSize = 0;
    st.FrameBorderSize = 1;
    st.PopupBorderSize = 1;
    st.ScrollbarSize = 10;
    st.ScrollbarRounding = 6;
    st.WindowPadding = ImVec2(8, 6);
    st.FramePadding = ImVec2(12, 8);
    st.ItemSpacing = ImVec2(8, 7);
    float s = std::max(0.6f, height / 1080.0f * 1.15f) * cfg.scale;
    st.ScaleAllSizes(s);
    st.FontSizeBase = 13.5f * s;
}

// ---------- entrada ----------
// Lo que llega a la ventana del juego (otro hilo): caracteres para el buscador y la rueda del ratón
static SRWLOCK inputLock = SRWLOCK_INIT;
static std::vector<WCHAR> chars;
static volatile LONG wheel;

void uiWndMessage(UINT m, WPARAM w, LPARAM) {
    if (m == WM_CHAR) {
        AcquireSRWLockExclusive(&inputLock);
        if (chars.size() < 64) chars.push_back((WCHAR)w);
        ReleaseSRWLockExclusive(&inputLock);
    } else if (m == WM_MOUSEWHEEL) InterlockedExchangeAdd(&wheel, (SHORT)HIWORD(w));
}

static bool gameInFront(HWND wnd = nullptr) {
    HWND f = GetForegroundWindow();
    if (wnd) return f == wnd || GetAncestor(f, GA_ROOT) == wnd;
    DWORD pid = 0;
    GetWindowThreadProcessId(f, &pid);
    return pid == GetCurrentProcessId();
}
static bool keyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

static XINPUT_STATE pad;
static bool havePad;
static WORD padPrev;

// Antes de cada fotograma: ratón (escalado de la ventana a la imagen del juego), teclado y mando a ImGui
void uiFeedInput(HWND wnd, int width, int height) {
    ImGuiIO &io = ImGui::GetIO();
    bool front = gameInFront(wnd);
    POINT pt;
    RECT rc;
    if (!demoClicks.empty()) {
        // Un clic cada 16 fotogramas; el ratón se queda en el último
        static int frame;
        size_t k = std::min((size_t)(frame / 16), demoClicks.size() - 1);
        bool down = frame / 16 < (int)demoClicks.size() && (frame % 16 == 8 || frame % 16 == 9);
        frame++;
        io.AddMousePosEvent(demoClicks[k].x, demoClicks[k].y);
        io.AddMouseButtonEvent(0, down);
        front = false;
    } else if (GetCursorPos(&pt) && ScreenToClient(wnd, &pt) && GetClientRect(wnd, &rc) && rc.right > 0 && rc.bottom > 0) {
        ImVec2 m((float)pt.x * width / rc.right, (float)pt.y * height / rc.bottom);
        static ImVec2 prev(-1, -1);
        if (m.x != prev.x || m.y != prev.y) { lastWasPad = false; prev = m; }
        io.AddMousePosEvent(m.x, m.y);
    }
    bool buttons[3] = {front && keyDown(VK_LBUTTON), front && keyDown(VK_RBUTTON), front && keyDown(VK_MBUTTON)};
    for (int b = 0; b < 3 && demoClicks.empty(); b++) {
        if (buttons[b]) lastWasPad = false;
        io.AddMouseButtonEvent(b, buttons[b]);
    }
    LONG wd = InterlockedExchange(&wheel, 0);
    if (wd) { io.AddMouseWheelEvent(0, wd / 120.0f); lastWasPad = false; }

    static const struct { int vk; ImGuiKey key; } keys[] = {{VK_TAB, ImGuiKey_Tab}, {VK_LEFT, ImGuiKey_LeftArrow}, {VK_RIGHT, ImGuiKey_RightArrow},
        {VK_UP, ImGuiKey_UpArrow}, {VK_DOWN, ImGuiKey_DownArrow}, {VK_PRIOR, ImGuiKey_PageUp}, {VK_NEXT, ImGuiKey_PageDown},
        {VK_HOME, ImGuiKey_Home}, {VK_END, ImGuiKey_End}, {VK_DELETE, ImGuiKey_Delete}, {VK_BACK, ImGuiKey_Backspace},
        {VK_SPACE, ImGuiKey_Space}, {VK_RETURN, ImGuiKey_Enter}, {'A', ImGuiKey_A}, {'C', ImGuiKey_C}, {'V', ImGuiKey_V}, {'X', ImGuiKey_X},
        {'Z', ImGuiKey_Z}};
    for (auto &k : keys) io.AddKeyEvent(k.key, front && keyDown(k.vk));
    io.AddKeyEvent(ImGuiMod_Ctrl, front && keyDown(VK_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, front && keyDown(VK_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, front && keyDown(VK_MENU));
    AcquireSRWLockExclusive(&inputLock);
    for (WCHAR c : chars)
        if (c >= 32) io.AddInputCharacterUTF16(c);
    chars.clear();
    ReleaseSRWLockExclusive(&inputLock);

    // Mando: la cruceta, el stick izquierdo y A navegan; el stick derecho desplaza
    WORD b = havePad ? pad.Gamepad.wButtons : 0;
    struct { WORD bit; ImGuiKey key; } map[] = {{XINPUT_GAMEPAD_DPAD_UP, ImGuiKey_GamepadDpadUp}, {XINPUT_GAMEPAD_DPAD_DOWN, ImGuiKey_GamepadDpadDown},
        {XINPUT_GAMEPAD_DPAD_LEFT, ImGuiKey_GamepadDpadLeft}, {XINPUT_GAMEPAD_DPAD_RIGHT, ImGuiKey_GamepadDpadRight},
        {XINPUT_GAMEPAD_A, ImGuiKey_GamepadFaceDown}, {XINPUT_GAMEPAD_X, ImGuiKey_GamepadFaceLeft}};
    for (auto &e : map) io.AddKeyEvent(e.key, (b & e.bit) != 0);
    auto axis = [](SHORT v) { float f = v / 32767.0f; return std::fabs(f) < 0.25f ? 0.0f : f; };
    float lx = havePad ? axis(pad.Gamepad.sThumbLX) : 0, ly = havePad ? axis(pad.Gamepad.sThumbLY) : 0;
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, lx < 0, -std::min(lx, 0.0f));
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, lx > 0, std::max(lx, 0.0f));
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, ly > 0, std::max(ly, 0.0f));
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, ly < 0, -std::min(ly, 0.0f));
    float ry = havePad ? axis(pad.Gamepad.sThumbRY) : 0;
    if (ry != 0) io.AddMouseWheelEvent(0, ry * ry * ry * 0.6f);
}

// Abrir/cerrar y pestañas; corre en cada fotograma del juego (también con el panel cerrado)
void uiInput() {
    static ULONGLONG started = GetTickCount64();
    if (cfg.openAfter > 0 && GetTickCount64() - started > (ULONGLONG)cfg.openAfter * 1000) {
        cfg.openAfter = 0;
        log("Test: opening the panel");
        if (!visible) toggle();
    }
    bool front = gameInFront();
    static bool prevHot, prevEsc;
    bool hot = keyDown((int)cfg.hotkey), esc = keyDown(VK_ESCAPE);
    bool hotPressed = hot && !prevHot && front, escPressed = esc && !prevEsc && front;
    prevHot = hot;
    prevEsc = esc;
    if (hotPressed) { toggle(); lastWasPad = false; return; }

    havePad = false;
    for (DWORD u = 0; u < 4 && !havePad; u++) havePad = padRead(u, &pad);   // padRead también detecta el botón que abre
    WORD b = havePad ? pad.Gamepad.wButtons : 0, edge = b & ~padPrev;
    padPrev = b;
    if (edge || (havePad && (std::abs(pad.Gamepad.sThumbLY) > 8000 || std::abs(pad.Gamepad.sThumbRY) > 8000))) lastWasPad = true;
    if (!visible) return;
    // ImGui solo existe desde el primer fotograma dibujado: antes no se le pregunta nada
    bool typing = ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
    bool back = (escPressed && !typing) || (edge & (XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_B));
    if (back && settingsOpen) { settingsOpen = false; return; }
    if (back) { toggle(); return; }
    if (edge & XINPUT_GAMEPAD_LEFT_SHOULDER) tab = (tab + 5) % 6;
    if (edge & XINPUT_GAMEPAD_RIGHT_SHOULDER) tab = (tab + 1) % 6;
    if (edge & XINPUT_GAMEPAD_Y) {
        cfg.hideDone = !cfg.hideDone;
        saveIni(L"HideCompleted", cfg.hideDone ? "1" : "0");
    }
}

// ---------- texto: tamaños y alturas de línea en píxeles CSS, y la misma colocación que un navegador ----------
static float tw(Fn f, float css, const char *b, const char *e = nullptr) { return faces[f].font->CalcTextSizeA(css * S, FLT_MAX, 0, b, e).x; }
static float textTop(Fn f, float css, float top, float lineCss) {
    float sz = css * S;
    const Face &fc = faces[f];
    return top + lineCss * S / 2 + (fc.asc - fc.desc) / 2 * sz - fc.font->GetFontBaked(sz)->Ascent;
}
static void put(ImDrawList *dl, Fn f, float css, float x, float top, float lineCss, ImU32 c, const char *b, const char *e = nullptr) {
    dl->AddText(faces[f].font, css * S, ImVec2(x, textTop(f, css, top, lineCss)), c, b, e);
}
static int charLen(unsigned char c) { return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; }
// letter-spacing, en em
static float twSp(Fn f, float css, const std::string &s, float sp) {
    float w = 0;
    for (size_t i = 0; i < s.size();) {
        int n = charLen((unsigned char)s[i]);
        w += tw(f, css, s.data() + i, s.data() + i + n) + sp * css * S;
        i += n;
    }
    return w;
}
static void putSp(ImDrawList *dl, Fn f, float css, float x, float top, float lineCss, ImU32 c, const std::string &s, float sp) {
    for (size_t i = 0; i < s.size();) {
        int n = charLen((unsigned char)s[i]);
        put(dl, f, css, x, top, lineCss, c, s.data() + i, s.data() + i + n);
        x += tw(f, css, s.data() + i, s.data() + i + n) + sp * css * S;
        i += n;
    }
}
// text-transform y búsqueda: ASCII y Latin-1
static std::string upper(std::string s) {
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = s[i];
        if (c >= 'a' && c <= 'z') s[i] = (char)(c - 32);
        else if (c == 0xC3 && i + 1 < s.size() && (unsigned char)s[i + 1] >= 0xA0 && (unsigned char)s[i + 1] != 0xB7) { s[i + 1] = (char)(s[i + 1] - 0x20); i++; }
    }
    return s;
}
static std::string lowerUtf8(std::string s) {
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = s[i];
        if (c >= 'A' && c <= 'Z') s[i] = (char)(c + 32);
        else if (c == 0xC3 && i + 1 < s.size() && (unsigned char)s[i + 1] < 0xA0 && (unsigned char)s[i + 1] != 0x97) { s[i + 1] = (char)(s[i + 1] + 0x20); i++; }
    }
    return s;
}

// ---------- formas ----------
static void dashedRect(ImDrawList *dl, ImVec2 a, ImVec2 b, float r, ImU32 c, float th, float dash, float gap) {
    std::vector<ImVec2> pts;
    auto arc = [&](float cx, float cy, float a0) {
        for (int i = 0; i <= 6; i++) {
            float t = a0 + i * (3.14159265f / 2) / 6;
            pts.push_back(ImVec2(cx + cosf(t) * r, cy + sinf(t) * r));
        }
    };
    arc(b.x - r, a.y + r, -3.14159265f / 2);
    arc(b.x - r, b.y - r, 0);
    arc(a.x + r, b.y - r, 3.14159265f / 2);
    arc(a.x + r, a.y + r, 3.14159265f);
    pts.push_back(pts[0]);
    float left = dash;
    bool on = true;
    for (size_t i = 0; i + 1 < pts.size(); i++) {
        ImVec2 p = pts[i], q = pts[i + 1];
        float len = sqrtf((q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y));
        while (len > 0) {
            float step = std::min(left, len);
            ImVec2 n(p.x + (q.x - p.x) * step / len, p.y + (q.y - p.y) * step / len);
            if (on) dl->AddLine(p, n, c, th);
            p = n;
            len -= step;
            left -= step;
            if (left <= 0.001f) { on = !on; left = on ? dash : gap; }
        }
    }
}

// box-shadow de .card: una línea de 1 px y un difuminado corto hacia abajo
static void cardShadow(ImDrawList *dl, ImVec2 a, ImVec2 b, float r) {
    const int N = 6;
    for (int i = 0; i < N; i++) {
        float grow = (-4 + i * 12.0f / N) * S, off = 6 * S;
        dl->AddRectFilled(ImVec2(a.x - grow, a.y + off - grow), ImVec2(b.x + grow, b.y + off + grow), col(P->shadowColor, P->shadow / N * (1.0f - (float)i / N)), r + std::max(grow, 0.0f));
    }
    dl->AddRectFilled(ImVec2(a.x, a.y + S), ImVec2(b.x, b.y + S), col(P->shadowColor, 0.06f), r);
}
static void card(ImDrawList *dl, ImVec2 a, ImVec2 b, float rCss = 14) {
    float r = rCss * S;
    cardShadow(dl, a, b, r);
    dl->AddRectFilled(a, b, col(P->paper), r);
    dl->AddRect(a, b, col(P->line), r, 0, std::max(1.0f, S));
}

// Contorno de foco (:focus-visible), solo al moverse con teclado o mando
static void ring(ImDrawList *dl, ImVec2 a, ImVec2 b, float r) {
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        dl->AddRect(ImVec2(a.x - 2 * S, a.y - 2 * S), ImVec2(b.x + 2 * S, b.y + 2 * S), col(P->cc), r + 2 * S, 0, 3 * S);
}

static void meter(ImDrawList *dl, float x, float y, float w, float hCss, float frac) {
    float h = hCss * S;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col(P->paper2), h / 2);
    if (frac > 0) dl->AddRectFilled(ImVec2(x + S, y + S), ImVec2(x + S + std::max(h - 2 * S, (w - 2 * S) * frac), y + h - S), col(P->woody), (h - 2 * S) / 2);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col(P->line), h / 2, 0, std::max(1.0f, S));
}

// Casilla de 'sz' píxeles CSS; la marca es el trazado M3 8.5l3.2 3L13 4.5 de la app
static void checkbox(ImDrawList *dl, float x, float y, float sz, float rCss, bool on, bool hov, unsigned idleBorder) {
    ImVec2 a(x, y), b(x + sz * S, y + sz * S);
    dl->AddRectFilled(a, b, col(on ? P->woody : P->paper), rCss * S);
    float th = 2 * S;
    ImVec2 ia(a.x + th / 2, a.y + th / 2), ib(b.x - th / 2, b.y - th / 2);
    dl->AddRect(ia, ib, on || hov ? col(P->woody) : col(idleBorder, 0.6f), (rCss - 1) * S, 0, th);
    if (on) {
        float u = (sz * 0.625f) * S / 16, ox = x + (sz * S - 16 * u) / 2, oy = y + (sz * S - 16 * u) / 2;
        ImVec2 pts[3] = {{ox + 3 * u, oy + 8.5f * u}, {ox + 6.2f * u, oy + 11.5f * u}, {ox + 13 * u, oy + 4.5f * u}};
        dl->AddPolyline(pts, 3, col(ON_WOODY), 0, 2.6f * u);
    }
}

// Chevron M5 7.5l5 5 5-5, de 18 píxeles
static void chevron(ImDrawList *dl, float x, float y, bool open, ImU32 c) {
    float u = 18 * S / 20;
    auto p = [&](float px, float py) {
        return open ? ImVec2(x + px * u, y + py * u) : ImVec2(x + py * u, y + (20 - px) * u);
    };
    ImVec2 pts[3] = {p(5, 7.5f), p(10, 12.5f), p(15, 7.5f)};
    dl->AddPolyline(pts, 3, c, 0, 2.2f * u);
}

static void star(ImDrawList *dl, float cx, float cy, float r, ImU32 c) {
    ImVec2 pts[10];
    for (int i = 0; i < 10; i++) {
        float t = -3.14159265f / 2 + i * 3.14159265f / 5, q = i % 2 ? r * 0.42f : r;
        pts[i] = ImVec2(cx + cosf(t) * q, cy + sinf(t) * q);
    }
    dl->AddConcavePolyFilled(pts, 10, c);
}

static void gear(ImDrawList *dl, float x, float y, float sz, ImU32 c, ImU32 hole) {
    static const float raw[][2] = {{10.22f, 4.30f}, {10.46f, 1.71f}, {13.54f, 1.71f}, {13.78f, 4.30f}, {16.19f, 5.30f}, {18.19f, 3.64f}, {20.36f, 5.81f}, {18.70f, 7.81f},
                                   {19.70f, 10.22f}, {22.29f, 10.46f}, {22.29f, 13.54f}, {19.70f, 13.78f}, {18.70f, 16.19f}, {20.36f, 18.19f}, {18.19f, 20.36f}, {16.19f, 18.70f},
                                   {13.78f, 19.70f}, {13.54f, 22.29f}, {10.46f, 22.29f}, {10.22f, 19.70f}, {7.81f, 18.70f}, {5.81f, 20.36f}, {3.64f, 18.19f}, {5.30f, 16.19f},
                                   {4.30f, 13.78f}, {1.71f, 13.54f}, {1.71f, 10.46f}, {4.30f, 10.22f}, {5.30f, 7.81f}, {3.64f, 5.81f}, {5.81f, 3.64f}, {7.81f, 5.30f}};
    ImVec2 pts[32];
    float u = sz / 24;
    for (int i = 0; i < 32; i++) pts[i] = ImVec2(x + raw[i][0] * u, y + raw[i][1] * u);
    dl->AddConcavePolyFilled(pts, 32, c);
    dl->AddCircleFilled(ImVec2(x + 12 * u, y + 12 * u), 3.3f * u, hole);
}

// ---------- controles ----------
static bool hit(const char *id, float x, float y, float w, float h, bool *hov = nullptr) {
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    bool c = ImGui::InvisibleButton(id, ImVec2(std::max(w, 1.0f), std::max(h, 1.0f)));
    if (hov) *hov = ImGui::IsItemHovered();
    return c;
}

// .btn y .btn.primary
static float btnW(const char *label, bool primary = false) {
    return primary ? tw(F800, 13.5f, label) + 30 * S : tw(F800, 13, label) + 26 * S;
}
static float btnH(bool primary = false) { return (primary ? 38.25f : 33.5f) * S; }
static bool btn(const char *id, float x, float y, const char *label, bool primary = false) {
    float w = btnW(label, primary), h = btnH(primary), r = 9 * S;
    bool hov, c = hit(id, x, y, w, h, &hov);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 a(x, y), b(x + w, y + h);
    dl->AddRectFilled(a, b, col(primary ? P->woody : P->paper2), r);
    dl->AddRect(a, b, col(primary ? P->woody : hov ? P->soft : P->line), r, 0, std::max(1.0f, S));
    if (primary) put(dl, F800, 13.5f, x + 15 * S, y + S, 36.25f, col(ON_WOODY), label);
    else put(dl, F800, 13, x + 13 * S, y + S, 31.5f, col(P->ink), label);
    ring(dl, a, b, r);
    return c;
}

// .icon-btn
static bool iconBtn(const char *id, float x, float y, int kind) {
    float sz = 44 * S, r = 12 * S;
    bool hov, c = hit(id, x, y, sz, sz, &hov);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 a(x, y), b(x + sz, y + sz);
    cardShadow(dl, a, b, r);
    dl->AddRectFilled(a, b, col(P->paper), r);
    dl->AddRect(a, b, col(hov ? P->soft : P->line), r, 0, std::max(1.0f, S));
    if (kind == 0) gear(dl, x + 11 * S, y + 11 * S, 22 * S, col(P->ink), col(P->paper));
    else {
        float m = 15 * S;
        dl->AddLine(ImVec2(x + m, y + m), ImVec2(x + sz - m, y + sz - m), col(P->ink), 2.4f * S);
        dl->AddLine(ImVec2(x + sz - m, y + m), ImVec2(x + m, y + sz - m), col(P->ink), 2.4f * S);
    }
    ring(dl, a, b, r);
    return c;
}

// .chip: botón con forma de píldora; devuelve su ancho en *w
static float chipW(const std::string &label, const std::string &icon, const std::string &n) {
    float w = tw(F700, 13, label.c_str()) + 23 * S;
    if (hasArt(icon)) w += 24 * S;
    if (!n.empty()) w += tw(F700, 13, " ") + tw(F700, 13, n.c_str());
    return w;
}
static const float CHIP_H = 32.5f;
static bool chip(const char *id, float x, float y, const std::string &label, const std::string &icon, const std::string &n, unsigned c, bool pressed) {
    float w = chipW(label, icon, n), h = CHIP_H * S;
    bool cl = hit(id, x, y, w, h);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 a(x, y), b(x + w, y + h);
    dl->AddRectFilled(a, b, col(pressed ? mix(c, 0.16f, P->paper) : P->paper), h / 2);
    dl->AddRect(a, b, col(c, pressed ? 1.0f : 0.35f), h / 2, 0, 1.5f * S);
    float tx = x + 11.5f * S;
    if (hasArt(icon)) {
        art(dl, icon, tx, y + (h - 18 * S) / 2, 18 * S);
        tx += 24 * S;
    }
    put(dl, F700, 13, tx, y + 1.5f * S, 29.5f, col(P->ink), label.c_str());
    if (!n.empty()) {
        tx += tw(F700, 13, label.c_str()) + tw(F700, 13, " ");
        put(dl, F700, 13, tx, y + 1.5f * S, 29.5f, col(P->soft), n.c_str());
    }
    ring(dl, a, b, h / 2);
    return cl;
}

// ---------- texto con marcas: botones del mando [X], cápsulas {CP12} y origen {START} {SHOP} ----------
struct PadKey { const char *mark; int xbox, ps; };
static const PadKey padKeys[] = {{"[X]", 0, 8}, {"[\xE2\x96\xA1]", 2, 10}, {"[\xE2\x96\xB3]", 3, 11}, {"[\xE2\x97\x8B]", 1, 9},
                                 {"[R1]", 5, 13}, {"[R2]", 7, 15}, {"[L1]", 4, 12}, {"[L2]", 6, 14}};
// Glifos de overlay/fonts/pad.ttf; 0 = color del texto (currentColor en la app)
static const unsigned padColors[18] = {0x7DB700, 0xEF4E29, 0x009FEB, 0xFEB504, 0, 0, 0, 0, 0x7C66E8, 0xFF6666, 0xFF69F8, 0x40E2A0, 0, 0, 0, 0, 0, 0};
static void padGlyph(ImDrawList *dl, int idx, float x, float y, float size, ImU32 fallback) {
    char u[4] = {(char)0xEE, (char)(0x80 + (idx >> 6)), (char)(0x80 + (idx & 63)), 0};
    dl->AddText(faces[FPAD].font, size, ImVec2(x, y), padColors[idx] ? col(padColors[idx]) : fallback, u);
}

struct Seg { int kind; const char *b, *e; float w; int glyph; };   // 0 texto, 1 cápsula, 2 inicio, 3 tienda, 4 botón

static void splitChunk(const char *b, const char *e, Fn f, float css, std::vector<Seg> &out) {
    const char *t = b;
    auto flush = [&](const char *to) {
        if (to > t) out.push_back({0, t, to, tw(f, css, t, to), 0});
    };
    for (const char *p = b; p < e;) {
        if (*p == '[') {
            bool found = false;
            for (auto &k : padKeys) {
                size_t n = strlen(k.mark);
                if ((size_t)(e - p) >= n && !strncmp(p, k.mark, n)) {
                    flush(p);
                    out.push_back({4, p, p + n, 1.6f * css * S, cfg.buttons == "ps" ? k.ps : k.xbox});
                    p += n;
                    t = p;
                    found = true;
                    break;
                }
            }
            if (found) continue;
        } else if (*p == '{') {
            const char *q = (const char *)memchr(p, '}', e - p);
            if (q) {
                flush(p);
                std::string id(p + 1, q);
                if (id == "START" || id == "SHOP") {
                    std::string label = upper(id == "START" ? L("de inicio", "starter") : L("tienda", "candy shop"));
                    out.push_back({id == "START" ? 2 : 3, p, q + 1, twSp(F800, 10.5f, label, 0.04f) + 14 * S, 0});
                } else out.push_back({1, p + 1, q, tw(FMONO, 11.5f, p + 1, q) + 15 * S, 0});
                p = q + 1;
                t = p;
                continue;
            }
        }
        p++;
    }
    flush(e);
}

// Párrafo que se parte por palabras. Sin dl solo mide; devuelve la altura. css y lineCss como en el CSS
static float rich(ImDrawList *dl, const std::string &s, float x, float y, float w, Fn f, float css, float lineCss, ImU32 c, bool strike = false) {
    static std::vector<Seg> segs;
    float space = tw(f, css, " "), lineH = lineCss * S, cx = x, cy = y, endX = x;
    bool lineUsed = false;
    auto endLine = [&]() {
        if (dl && strike && endX > x) {
            float ly = cy + lineH / 2 + (faces[f].asc - faces[f].desc) / 2 * css * S - 0.27f * css * S;
            dl->AddLine(ImVec2(x, ly), ImVec2(endX, ly), c, std::max(1.0f, css * S / 15));
        }
    };
    const char *p = s.c_str(), *end = p + s.size();
    while (p < end) {
        if (*p == ' ') { p++; continue; }
        if (*p == '\n') {
            endLine();
            cy += lineH; cx = x; endX = x; lineUsed = false;
            p++;
            continue;
        }
        const char *q = p;
        while (q < end && *q != ' ' && *q != '\n') q++;
        segs.clear();
        splitChunk(p, q, f, css, segs);
        float cw = 0;
        for (auto &g : segs) cw += g.w;
        float gap = lineUsed ? space : 0;
        if (lineUsed && cx + gap + cw > x + w) {
            endLine();
            cy += lineH; cx = x; endX = x; gap = 0;
        }
        cx += gap;
        if (dl) {
            float dx = cx;
            for (auto &g : segs) {
                float mid = cy + lineH / 2;
                if (g.kind == 0) put(dl, f, css, dx, cy, lineCss, c, g.b, g.e);
                else if (g.kind == 1) {
                    std::string id(g.b, g.e);
                    auto it = capIndex.find("c-" + id);
                    bool have = it != capIndex.end() && it->second->done;
                    float h = (11.5f + 9) * S;
                    ImVec2 a(dx, mid - h / 2), b(dx + g.w, mid + h / 2);
                    unsigned cc = have ? P->al : P->soft;
                    if (have) dl->AddRect(a, b, col(cc), 6 * S, 0, 1.5f * S);
                    else dashedRect(dl, a, b, 6 * S, col(cc), 1.5f * S, 3 * S, 2.5f * S);
                    put(dl, FMONO, 11.5f, dx + 7.5f * S, a.y, 20.5f, col(cc), id.c_str());
                } else if (g.kind == 2 || g.kind == 3) {
                    unsigned cc = g.kind == 2 ? P->al : P->zp;
                    std::string label = upper(g.kind == 2 ? L("de inicio", "starter") : L("tienda", "candy shop"));
                    float h = (10.5f + 7) * S;
                    ImVec2 a(dx, mid - h / 2), b(dx + g.w, mid + h / 2);
                    dl->AddRectFilled(a, b, col(cc, 0.15f), h / 2);
                    putSp(dl, F800, 10.5f, dx + 7 * S, a.y, 17.5f, col(cc), label, 0.04f);
                } else {
                    float sz = 1.5f * css * S;
                    padGlyph(dl, g.glyph, dx + 0.05f * css * S, mid - sz / 2, sz, col(P->ink));
                }
                dx += g.w;
            }
        }
        cx += cw;
        endX = cx;
        lineUsed = true;
        p = q;
    }
    endLine();
    return (cy - y) + lineH;
}

// ---------- recuentos ----------
static std::string count(int a, int b) { return std::to_string(a) + "/" + std::to_string(b); }

static bool matches(const Item &it, const std::string &q) {
    if (q.empty()) return true;
    std::string hay = lowerUtf8(T(it.title) + " " + T(it.sub) + " " + T(it.text) + " " + T(it.tip) + " " + it.lead + " " + T(it.cat));
    return hay.find(q) != std::string::npos;
}

static int sectionIndex(const std::string &key) {
    for (size_t i = 0; i < prog.sections.size(); i++)
        if (prog.sections[i].key == key) return (int)i + 1;
    return 0;
}

static std::string progText(const std::string &p) {
    if (p.empty()) return "";
    int a = 0, b = 0;
    std::string label;
    if (p == "para") {
        if (prog.para < 0) return "";
        a = std::min(prog.para, prog.paraTotal);
        b = prog.paraTotal;
        label = "army men";
    } else {
        std::string sec = p.compare(0, 5, "story") == 0 ? "story" : p == "stars" ? "stars" : "picto";
        std::string type = p.compare(0, 6, "story:") == 0 ? p.substr(6) : "";
        int si = sectionIndex(sec);
        if (!si) return "";
        for (auto &g : prog.sections[si - 1].groups)
            for (auto &it : g.items) {
                bool disc = it.id.compare(0, 3, "pd-") == 0;
                if ((!type.empty() && it.type != type) || (p == "discs" && !disc) || (p == "picto" && disc)) continue;
                b++;
                a += it.done;
            }
        if (!type.empty()) {
            for (auto &t : prog.types)
                if (t.key == type) label = lowerUtf8(T(t.name));
        } else label = p == "story" ? L("coleccionables", "collectibles") : p == "stars" ? L("estrellas", "stars") : p == "discs" ? L("discos", "discs") : L("escenas", "scenes");
    }
    return es() ? "Llevas " + count(a, b) + " " + label : count(a, b) + " " + label;
}

// ---------- un elemento (.item). Sin dl solo mide; devuelve la altura ----------
struct Piece { int kind; std::string text, icon; float w, h; unsigned c; };   // 0 título, 1 insignia, 2 insignia neutra, 3 aviso, 4 nombre inglés

static float itemLayout(ImDrawList *dl, Item &it, float x, float y, float w, bool arts, bool linkTarget = false) {
    static std::vector<Piece> pieces;
    float s = S, top = y + 10 * s, left = x + 8 * s;
    unsigned softC = P->soft;

    // Casilla (26 + hueco) y entrada: número, código, trofeo o imagen
    if (dl) checkbox(dl, left, top + s, 24, 7, it.done, ImGui::IsItemHovered(), softC);
    float leadX = left + 36 * s, leadW = 0, leadH = 0;
    const std::string &lead = it.lead;
    std::string lv = lead.substr(lead.find(':') == std::string::npos ? lead.size() : lead.find(':') + 1);
    if (lead.compare(0, 4, "art:") == 0) {
        leadW = 44 * s;
        leadH = 40 * s;
        if (dl) art(dl, lv, leadX, top - 4 * s, 44 * s);
    } else if (lead.compare(0, 5, "icon:") == 0) {
        leadW = 30 * s;
        leadH = 27 * s;
        if (dl) art(dl, lv, leadX, top - 3 * s, 30 * s);
    } else if (lead.compare(0, 5, "code:") == 0) {
        float tw0 = tw(FMONO, 12, lv.c_str());
        leadW = std::max(tw0, 5.2f * tw(FMONO, 12, "0"));
        leadH = 18 * s;
        if (dl) put(dl, FMONO, 12, leadX, top + 6 * s, 12, col(softC), lv.c_str());
    } else {
        float tw0 = lv.empty() ? 0 : tw(F800, 15, lv.c_str());
        leadW = arts ? 44 * s : std::max(tw0, 1.6f * tw(F800, 15, "0"));
        leadH = 24.5f * s;
        if (dl && !lv.empty()) put(dl, F800, 15, arts ? leadX + (leadW - tw0) / 2 : leadX + leadW - tw0, top + 2 * s, 22.5f, col(softC), lv.c_str());
    }
    float mainX = leadX + leadW + 10 * s, mainW = x + w - 8 * s - mainX, cy = top;

    // .meta: título, insignias y avisos en filas centradas
    pieces.clear();
    if (!T(it.title).empty()) pieces.push_back({0, T(it.title), "", tw(F800, 15, T(it.title).c_str()), 22.5f * s, 0});
    for (auto &b : it.badges) {
        std::string label = upper(T(b.text));
        float tx = twSp(F800, 11.5f, label, 0.04f);
        if (b.type.empty()) pieces.push_back({2, label, b.icon, tx + 18 * s + (hasArt(b.icon) ? 23 * s : 0), 23.25f * s, 0});
        else pieces.push_back({1, label, b.icon, tx + 15 * s + (hasArt(b.icon) ? 23 * s : 0), 23.25f * s, typeColor(b.type)});
    }
    auto addFlag = [&](const Flag &f) {
        pieces.push_back({3, T(f.text), "", tw(F800, 11.5f, T(f.text).c_str()) + 18 * s, 23.25f * s, (unsigned)f.kind});
    };
    for (auto &f : it.flags) if (!f.kind && it.type.empty()) addFlag(f);
    if (!it.sub[li()].empty()) pieces.push_back({4, it.sub[li()], "", tw(F700, 12, it.sub[li()].c_str()), 18 * s, 0});
    for (auto &f : it.flags) if (f.kind || !it.type.empty()) addFlag(f);
    float gap = 6 * s;
    for (size_t i = 0; i < pieces.size();) {
        size_t j = i;
        float rowW = 0, rowH = 0;
        while (j < pieces.size() && (j == i || rowW + gap + pieces[j].w <= mainW)) {
            rowW += (j > i ? gap : 0) + pieces[j].w;
            rowH = std::max(rowH, pieces[j].h);
            j++;
        }
        if (dl) {
            float px = mainX;
            for (size_t k = i; k < j; k++) {
                Piece &p = pieces[k];
                float py = cy + (rowH - p.h) / 2;
                ImVec2 a(px, py), b(px + p.w, py + p.h);
                float fade = it.done ? 0.5f : 1.0f;
                if (p.kind == 0) {
                    ImU32 c = col(it.done ? softC : P->ink);
                    put(dl, F800, 15, px, py, 22.5f, c, p.text.c_str());
                    if (it.done) {
                        float ly = py + 11.25f * s + (faces[F800].asc - faces[F800].desc) / 2 * 15 * s - 0.27f * 15 * s;
                        dl->AddLine(ImVec2(px, ly), ImVec2(px + p.w, ly), c, std::max(1.0f, s));
                    }
                } else if (p.kind == 1 || p.kind == 2) {
                    bool plain = p.kind == 2;
                    dl->AddRectFilled(a, b, plain ? col(P->paper2, fade) : col(p.c, fade), 6 * s);
                    if (plain) dl->AddRect(a, b, col(P->line, fade), 6 * s, 0, std::max(1.0f, s));
                    float tx = px + (plain ? 8 : 6) * s + (plain ? s : 0);
                    if (hasArt(p.icon)) {
                        art(dl, p.icon, tx, py + (p.h - 18 * s) / 2, 18 * s, fade);
                        tx += 23 * s;
                    }
                    putSp(dl, F800, 11.5f, tx, py, 23.25f, plain ? col(softC, fade) : col(P->onTag, fade), p.text, 0.04f);
                } else if (p.kind == 3) {
                    unsigned c = p.c == 'm' ? P->tp : p.c == 't' ? P->accentInk : softC;
                    unsigned border = p.c == 'm' ? mix(P->tp, 0.45f, P->paper) : p.c == 't' ? mix(P->accent, 0.5f, P->paper) : P->line;
                    dl->AddRect(a, b, col(border), 6 * s, 0, std::max(1.0f, s));
                    put(dl, F800, 11.5f, px + 9 * s, py, 23.25f, col(c), p.text.c_str());
                } else {
                    put(dl, F700, 12, px, py, 18, col(softC), p.text.c_str());
                }
                px += p.w + gap;
            }
        }
        cy += rowH + (j < pieces.size() ? gap : 0);
        i = j;
    }

    // Textos, con 3 píxeles entre uno y otro
    bool any = !pieces.empty();
    auto sep = [&]() { if (any) cy += 3 * s; any = true; };
    if (!T(it.text).empty()) {
        sep();
        cy += rich(dl, T(it.text), mainX, cy, mainW, F500, 15, 22.5f, col(it.done ? softC : P->ink), it.done);
    }
    if (!T(it.tip).empty()) {
        sep();
        cy += rich(dl, T(it.tip), mainX, cy, mainW, F500, 13.5f, 20.25f, col(softC));
    }
    std::string pt = progText(it.prog);
    int to = it.link.empty() ? 0 : sectionIndex(it.link);
    if (!pt.empty() || to) {
        sep();
        float ex = mainX;
        if (!pt.empty()) {
            if (dl) put(dl, F700, 12.5f, ex, cy, 18.75f, col(softC), pt.c_str());
            ex += tw(F700, 12.5f, pt.c_str()) + 12 * s;
        }
        if (to) {
            std::string label = std::string(L("Ir a ", "Go to ")) + T(prog.sections[to - 1].title) + " \xE2\x86\x92";
            float lw = tw(F700, 12.5f, label.c_str());
            if (dl) {
                bool hov = false;
                if (linkTarget && hit("##link", ex, cy, lw, 18.75f * s, &hov) && to) tab = to;
                put(dl, F700, 12.5f, ex, cy, 18.75f, col(P->cc), label.c_str());
                float ly = cy + 15.5f * s;
                dl->AddLine(ImVec2(ex, ly), ImVec2(ex + lw, ly), col(P->cc), std::max(1.0f, s));
                if (linkTarget) ring(dl, ImVec2(ex, cy), ImVec2(ex + lw, cy + 18.75f * s), 3 * s);
            }
        }
        cy += 18.75f * s;
    }
    return std::max(cy - top, leadH) + 20 * s;
}

// ---------- un grupo (.card.group) ----------
static void intro(ImDrawList *dl, const std::string &s, float x, float &cy, float w) {
    float h = rich(nullptr, s, x, 0, w - 26 * S, F500, 13.5f, 20.25f, 0) + 22 * S;
    ImVec2 a(x, cy), b(x + w, cy + h);
    dl->AddRectFilled(a, b, col(P->paper, 0.88f), 12 * S);
    dashedRect(dl, a, b, 12 * S, col(P->line), std::max(1.0f, S), 3 * S, 3 * S);
    rich(dl, s, x + 13 * S, cy + 11 * S, w - 26 * S, F500, 13.5f, 20.25f, col(P->soft));
    cy += h;
}

static void groupCard(Section &sec, Group &g, const std::string &q, float x, float &cy, float w) {
    std::vector<Item *> show;
    int matchCount = 0;
    bool arts = false;
    for (auto &it : g.items) {
        arts = arts || it.lead.compare(0, 4, "art:") == 0;
        if (sec.key == "story" && !typeFilter.empty() && !typeFilter.count(it.type)) continue;
        if (sec.key == "capsules" && !catFilter.empty() && it.type != catFilter) continue;
        if (!matches(it, q)) continue;
        matchCount++;
        if (cfg.hideDone && it.done) continue;
        show.push_back(&it);
    }
    if (!matchCount) return;
    std::string ck = sec.key + ":" + g.key;
    bool open = !collapsed.count(ck), complete = g.done == (int)g.items.size();
    float s = S, y0 = cy;
    ImGui::PushID(ck.c_str());
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    // .g-head
    std::string sub = T(g.sub);
    float titleH = 23.1f * s + (sub.empty() ? 0 : (2 + 18.75f) * s) + 13 * s;
    float headH = 28 * s + std::max(44 * s, titleH);
    bool hhov;
    if (hit("##head", x, cy, w, headH, &hhov)) {
        if (open) collapsed.insert(ck);
        else collapsed.erase(ck);
    }
    ring(dl, ImVec2(x, cy), ImVec2(x + w, cy + headH), 14 * s);
    float nb = 44 * s, nx = x + 16 * s, ny = cy + (headH - nb) / 2;
    ImVec2 na(nx, ny), nz(nx + nb, ny + nb);
    if (complete) dl->AddRectFilled(na, nz, col(P->woody), 12 * s);
    else {
        dl->AddRectFilled(na, nz, col(P->accent, 0.12f), 12 * s);
        dl->AddRect(ImVec2(nx + s, ny + s), ImVec2(nx + nb - s, ny + nb - s), col(P->accent, 0.4f), 11 * s, 0, 2 * s);
    }
    if (g.num) {
        std::string n = std::to_string(g.num);
        put(dl, FDISP, 24, nx + (nb - tw(FDISP, 24, n.c_str())) / 2, ny, 44, col(complete ? ON_WOODY : P->accentInk), n.c_str());
    } else art(dl, sec.icon, nx + 6 * s, ny + 6 * s, 32 * s);
    float tx = nx + nb + 14 * s, ty = cy + (headH - titleH) / 2;
    put(dl, FDISP, 21, tx, ty, 23.1f, col(P->ink), T(g.title).c_str());
    ty += 23.1f * s;
    if (!sub.empty()) {
        ty += 2 * s;
        put(dl, F700, 12.5f, tx, ty, 18.75f, col(P->soft), sub.c_str());
        ty += 18.75f * s;
    }
    std::string c = count(g.done, (int)g.items.size());
    float cw = tw(FDISP, 21, c.c_str()), countW = std::max(cw, 18 * s);
    float rightX = x + w - 16 * s;
    float miniW = std::min(260 * s, rightX - countW - 14 * s - tx);
    meter(dl, tx, ty + 6 * s, miniW, 7, g.items.empty() ? 0 : g.done / (float)g.items.size());
    float cTotal = 31.5f * s + 2 * s + 18 * s, cTop = cy + (headH - cTotal) / 2;
    put(dl, FDISP, 21, rightX - cw, cTop, 31.5f, col(P->ink), c.c_str());
    chevron(dl, rightX - 18 * s, cTop + 33.5f * s, open, col(P->soft));
    cy += headH;

    if (open) {
        // .g-body
        dl->AddLine(ImVec2(x, cy), ImVec2(x + w, cy), col(P->line), std::max(1.0f, s));
        if (!T(g.intro).empty()) {
            cy += 10 * s;
            intro(dl, T(g.intro), x + 16 * s, cy, w - 32 * s);
        }
        // Marcar o desmarcar todo lo que no sale de la partida
        bool allDone = true, manual = false;
        for (auto &it : g.items) {
            allDone = allDone && it.done;
            manual = manual || !it.fromSave;
        }
        if (manual) {
            const char *label = allDone ? L("Desmarcar todo", "Unmark all") : L("Marcar todo", "Mark all");
            cy += 8 * s;
            if (btn("##all", x + w - 16 * s - btnW(label), cy, label)) {
                for (auto &it : g.items)
                    if (!it.fromSave) prog.setManual(it.id, !allDone);
            }
            cy += btnH();
        }
        if (show.empty()) {
            const char *msg = q.empty() && typeFilter.empty() && catFilter.empty() ? L("Ya tienes todo lo de aquí.", "You have everything here.")
                                                                                     : L("Ya tienes todo lo de aquí con este filtro.", "You have everything here with this filter.");
            put(dl, F700, 13.5f, x + 16 * s, cy + 14 * s, 20.25f, col(P->soft), msg);
            cy += 48.25f * s;
        } else {
            // ol.items
            cy += 6 * s;
            float ix = x + 8 * s, iw = w - 16 * s;
            for (auto *itp : show) {
                Item &it = *itp;
                if (it.layoutGen != layoutGen) {
                    it.layoutH = itemLayout(nullptr, it, ix, 0, iw, arts);
                    it.layoutGen = layoutGen;
                }
                ImGui::PushID(it.id.c_str());
                ImGui::SetCursorScreenPos(ImVec2(ix, cy));
                ImGui::SetNextItemAllowOverlap();
                bool clicked = ImGui::InvisibleButton("##row", ImVec2(iw, it.layoutH));
                if (clicked && !(it.fromSave && it.done))
                    if (!prog.setManual(it.id, !it.done)) log("Could not save progress.json");
                if (ImGui::IsItemVisible()) {
                    bool hov = ImGui::IsItemHovered();
                    if (hov) dl->AddRectFilled(ImVec2(ix, cy), ImVec2(ix + iw, cy + it.layoutH), col(P->paper2), 10 * s);
                    if (hov && it.fromSave && it.done) ImGui::SetTooltip("%s", L("Detectado en tu partida", "Found in your save"));
                    ring(dl, ImVec2(ix, cy), ImVec2(ix + iw, cy + it.layoutH), 10 * s);
                    itemLayout(dl, it, ix, cy, iw, arts, true);
                }
                ImGui::PopID();
                cy += it.layoutH + 2 * s;
            }
            cy += 8 * s;
        }
    }
    dl->ChannelsSetCurrent(0);
    card(dl, ImVec2(x, y0), ImVec2(x + w, cy));
    dl->ChannelsMerge();
    cy += 14 * s;
    ImGui::PopID();
}

// ---------- pestaña Ruta (.steps) ----------
static void route(float x, float &cy, float w) {
    struct Step { const char *t[2], *d[2]; const char *links[2]; };
    static const Step steps[] = {
        {{"Modo historia", "Story mode"}, {"Los 8 niveles con todos sus coleccionables.", "All 8 levels with every collectible."}, {"story", "achievements"}},
        {{"Toy Box", "Toy Box"}, {"Misiones, cápsulas y estrellas doradas. Guarda a menudo: el juego no guarda solo.",
                                  "Missions, capsules and gold stars. Save often: the game does not autosave."}, {"capsules", "stars"}},
        {{"Pict-O-Matic", "Pict-O-Matic"}, {"Mejor cuando tengas casi todas las cápsulas: así sabes qué piezas usar.",
                                            "Best once you own almost every capsule, so you know which pieces to use."}, {"picto", nullptr}},
        {{"Limpieza", "Clean-up"}, {"Repasa lo que falte.", "Check what is left."}, {"achievements", nullptr}}};
    float s = S;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    cy += 14 * s;
    int n = 0;
    for (auto &st : steps) {
        n++;
        ImGui::PushID(n);
        float y0 = cy;
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);
        cy += 14 * s;
        std::string num = std::to_string(n) + ". ";
        float tx = x + 16 * s;
        put(dl, FDISP, 21, tx, cy, 31.5f, col(P->accentInk), num.c_str());
        put(dl, FDISP, 21, tx + tw(FDISP, 21, num.c_str()), cy, 31.5f, col(P->ink), st.t[li()]);
        cy += 31.5f * s + 4 * s;
        cy += rich(dl, st.d[li()], tx, cy, w - 32 * s, F500, 15, 22.5f, col(P->soft)) + 4 * s;
        float cx = tx;
        for (const char *k : st.links) {
            if (!k) continue;
            int to = sectionIndex(k);
            if (!to) continue;
            Section &sec = prog.sections[to - 1];
            std::string nn = count(sec.done, sec.total);
            if (chip(k, cx, cy, T(sec.title), sec.icon, nn, P->soft, false)) tab = to;
            cx += chipW(T(sec.title), sec.icon, nn) + 6 * s;
        }
        cy += CHIP_H * s + 14 * s;
        dl->ChannelsSetCurrent(0);
        card(dl, ImVec2(x, y0), ImVec2(x + w, cy));
        dl->ChannelsMerge();
        cy += 12 * s;
        ImGui::PopID();
    }
}

// ---------- opciones (.sheet) ----------
struct SegOpt { const char *label; int glyph; };

// .seg, alineado a la derecha; devuelve la opción pulsada o -1
static int seg(const char *id, float right, float y, const SegOpt *opts, int n, int active) {
    float s = S, h = 31.5f * s + 8 * s, w = 8 * s + (n - 1) * 2 * s;
    float ow[4];
    for (int i = 0; i < n; i++) {
        ow[i] = tw(F800, 13, opts[i].label) + 22 * s + (opts[i].glyph >= 0 ? 25 * s : 0);
        w += ow[i];
    }
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float x = right - w;
    ImVec2 a(x, y), b(right, y + h);
    dl->AddRectFilled(a, b, col(P->paper), 11 * s);
    dl->AddRect(a, b, col(P->line), 11 * s, 0, std::max(1.0f, s));
    int picked = -1;
    float cx = x + 4 * s;
    ImGui::PushID(id);
    for (int i = 0; i < n; i++) {
        ImGui::PushID(i);
        bool hov;
        if (hit("##o", cx, y + 4 * s, ow[i], 31.5f * s, &hov)) picked = i;
        ImVec2 oa(cx, y + 4 * s), ob(cx + ow[i], y + 35.5f * s);
        if (i == active) dl->AddRectFilled(oa, ob, col(P->woody), 8 * s);
        unsigned tc = i == active ? ON_WOODY : hov ? P->ink : P->soft;
        float tx = cx + 11 * s;
        if (opts[i].glyph >= 0) {
            padGlyph(dl, opts[i].glyph, tx, y + 4 * s + (31.5f - 20) / 2 * s, 20 * s, col(tc));
            tx += 25 * s;
        }
        put(dl, F800, 13, tx, y + 4 * s, 31.5f, col(tc), opts[i].label);
        ring(dl, oa, ob, 8 * s);
        ImGui::PopID();
        cx += ow[i] + 2 * s;
    }
    ImGui::PopID();
    return picked;
}

static void sheet(float panelX, float panelY, float panelW, float panelH) {
    const char *name = "##sheet";
    float s = S, w = std::min(440 * s, panelW);
    if (settingsOpen && !ImGui::IsPopupOpen(name)) ImGui::OpenPopup(name);
    ImGui::SetNextWindowPos(ImVec2(panelX + panelW - w, panelY));
    ImGui::SetNextWindowSize(ImVec2(w, panelH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 16 * s);
    bool open = ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    if (!open) return;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + panelH), col(P->sky), 16 * s);
    float x = p.x + 16 * s, cw = w - 32 * s, cy = p.y + 14 * s;
    put(dl, FDISP, 30, p.x + 20 * s, cy, 44, col(P->ink), L("Opciones", "Settings"));
    if (iconBtn("##closeset", p.x + w - 60 * s, cy, 1)) settingsOpen = false;
    cy += 58 * s;

    // .set-group: título y tarjeta con filas
    auto groupTitle = [&](const char *t) {
        putSp(dl, F800, 11.5f, x + 4 * s, cy, 17.25f, col(P->soft), upper(t), 0.13f);
        cy += 24 * s;
    };
    auto rowText = [&](const char *lbl, const char *desc, float rowY, float textW) {
        put(dl, F800, 14, x + 14 * s, rowY, 21, col(P->ink), lbl);
        if (desc) rich(dl, desc, x + 14 * s, rowY + 23 * s, textW, F500, 12.5f, 18.75f, col(P->soft));
    };

    groupTitle(L("Apariencia", "Appearance"));
    float y0 = cy, rowH = 64 * s;
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    {
        rowText(L("Idioma", "Language"), nullptr, cy + (rowH - 21 * s) / 2, 0);
        SegOpt o[] = {{"Español", -1}, {"English", -1}};
        int k = seg("lang", x + cw - 14 * s, cy + (rowH - 39.5f * s) / 2, o, 2, es() ? 0 : 1);
        if (k >= 0 && (k == 0) != es()) {
            cfg.lang = k == 0 ? "es" : "en";
            saveIni(L"Language", cfg.lang);
            layoutGen++;
        }
        cy += rowH;
        dl->AddLine(ImVec2(x, cy), ImVec2(x + cw, cy), col(P->line), std::max(1.0f, s));
    }
    {
        rowText(L("Tema", "Theme"), nullptr, cy + (rowH - 21 * s) / 2, 0);
        SegOpt o[] = {{L("Sistema", "System"), -1}, {L("Claro", "Light"), -1}, {L("Oscuro", "Dark"), -1}};
        int active = cfg.theme == "light" ? 1 : cfg.theme == "dark" ? 2 : 0;
        int k = seg("theme", x + cw - 14 * s, cy + (rowH - 39.5f * s) / 2, o, 3, active);
        if (k >= 0 && k != active) {
            cfg.theme = k == 1 ? "light" : k == 2 ? "dark" : "auto";
            saveIni(L"Theme", cfg.theme);
            themeDirty = true;
        }
        cy += rowH;
        dl->AddLine(ImVec2(x, cy), ImVec2(x + cw, cy), col(P->line), std::max(1.0f, s));
    }
    {
        SegOpt o[] = {{"Xbox", 16}, {"PlayStation", 17}};
        const char *desc = L("Cómo se muestran los botones en las pistas", "How buttons are shown in the hints");
        float textW = cw - 28 * s - 225 * s;
        float h = std::max(rowH, 24 * s + 23 * s + rich(nullptr, desc, 0, 0, textW, F500, 12.5f, 18.75f, 0));
        rowText(L("Botones", "Buttons"), desc, cy + 12 * s, textW);
        int active = cfg.buttons == "ps" ? 1 : 0;
        int k = seg("pad", x + cw - 14 * s, cy + (h - 39.5f * s) / 2, o, 2, active);
        if (k >= 0 && k != active) {
            cfg.buttons = k == 1 ? "ps" : "xbox";
            saveIni(L"Buttons", cfg.buttons);
            layoutGen++;
        }
        cy += h;
    }
    dl->ChannelsSetCurrent(0);
    card(dl, ImVec2(x, y0), ImVec2(x + cw, cy));
    dl->ChannelsMerge();
    cy += 20 * s;

    groupTitle(L("Acerca de", "About"));
    y0 = cy;
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    const char *note = L("Hecho por fans · sin relación con Disney, Pixar ni Avalanche Software", "Fan-made · not affiliated with Disney, Pixar or Avalanche Software");
    std::string ver = std::string(L("Versión ", "Version ")) + prog.version;
    put(dl, F800, 14, x + 14 * s, cy + 12 * s, 21, col(P->ink), ver.c_str());
    cy += 12 * s + 23 * s;
    cy += rich(dl, note, x + 14 * s, cy, cw - 28 * s, F500, 12.5f, 18.75f, col(P->soft)) + 12 * s;
    dl->ChannelsSetCurrent(0);
    card(dl, ImVec2(x, y0), ImVec2(x + cw, cy));
    dl->ChannelsMerge();

    // Cerrar: la X, Esc o B (uiInput), o pulsar fuera
    ImGui::SetCursorScreenPos(ImVec2(p.x, cy));
    ImGui::Dummy(ImVec2(1, 1));
    if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) settingsOpen = false;
    if (!settingsOpen) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------- panel ----------
static void clouds(ImDrawList *dl, ImVec2 a, ImVec2 b) {
    struct Shape { float x, y, rx, ry; };
    static const Shape shapes[] = {{70, 92, 30, 30}, {112, 70, 42, 42}, {160, 88, 34, 34}, {115, 112, 92, 26},
                                   {352, 318, 24, 24}, {386, 300, 34, 34}, {424, 316, 26, 26}, {388, 336, 72, 20}};
    float tw0 = 480 * S, th0 = 420 * S;
    dl->PushClipRect(a, b, true);
    for (float oy = a.y; oy < b.y; oy += th0)
        for (float ox = a.x; ox < b.x; ox += tw0)
            for (auto &sh : shapes)
                dl->AddEllipseFilled(ImVec2(ox + sh.x * S, oy + sh.y * S), ImVec2(sh.rx * S, sh.ry * S), col(P->cloud));
    dl->PopClipRect();
}

static void syncStatus(std::string &text, unsigned &dot) {
    bool reading = refreshState == 1;
    char b[96];
    if (contentFailed) { text = L("Faltan content.json y savemap.json", "content.json and savemap.json are missing"); dot = P->tp; }
    else if (prog.sections.empty() || reading) { text = L("Leyendo la partida…", "Reading save…"); dot = P->woody; }
    else if (!prog.saveFound) { text = L("No encuentro la partida", "Save not found"); dot = P->tp; }
    else {
        snprintf(b, sizeof b, "%s · %02d:%02d:%02d", L("Sincronizado", "Synced"), readAt.wHour, readAt.wMinute, readAt.wSecond);
        text = b;
        dot = P->al;
    }
}

static void hero(ImDrawList *dl, float x, float &cy, float w) {
    float s = S;
    // A la derecha: tarjeta de sincronizar, opciones y cerrar
    const char *syncTitle = L("Sincronizar con tu partida", "Sync with your save"), *syncNow = L("Sincronizar ahora", "Sync now");
    std::string status;
    unsigned dot;
    syncStatus(status, dot);
    float textW = std::max(tw(F800, 14, syncTitle), tw(F700, 12.5f, status.c_str()) + 13 * s);
    float cardW = 14 * s + textW + 14 * s + btnW(syncNow, true) + 10 * s, cardH = std::max(btnH(true), 37 * s) + 20 * s;
    float toolsW = cardW + 8 * s + 44 * s + 8 * s + 44 * s;

    // El título se ajusta al hueco que quede (clamp(32px, 6vw, 52px) en la app)
    std::string title = "Toy Story 3 Companion";
    float at52 = tw(FDISP, 52, title.c_str()) + 52 * s;
    float size = std::max(26.0f, std::min(52.0f, 52 * (w - toolsW - 24 * s) / at52));
    float leftH = 22 * s + size * 0.95f * s, heroH = std::max(leftH, cardH);

    float ly = cy + heroH - leftH;
    putSp(dl, F800, 12, x, ly, 18, col(P->soft), upper(L("Herramienta no oficial · Steam / PC", "Unofficial tool · Steam / PC")), 0.14f);
    ly += 22 * s;
    put(dl, FDISP, size, x, ly, size * 0.95f, col(P->ink), title.c_str());
    float tl = tw(FDISP, size, title.c_str());
    star(dl, x + tl + 0.62f * size * s, ly + size * 0.46f * s, 0.36f * size * s, col(P->woody));

    float rx = x + w - toolsW, ry = cy + heroH - cardH;
    card(dl, ImVec2(rx, ry), ImVec2(rx + cardW, ry + cardH));
    put(dl, F800, 14, rx + 14 * s, ry + 10 * s + (cardH - 20 * s - 37 * s) / 2, 17, col(P->ink), syncTitle);
    float sy = ry + 10 * s + (cardH - 20 * s - 37 * s) / 2 + 18 * s;
    dl->AddCircleFilled(ImVec2(rx + 14 * s + 3.5f * s, sy + 9.5f * s), 3.5f * s, col(dot));
    put(dl, F700, 12.5f, rx + 14 * s + 13 * s, sy, 19, col(P->soft), status.c_str());
    if (btn("##sync", rx + 14 * s + textW + 14 * s, ry + (cardH - btnH(true)) / 2, syncNow, true)) requestRefresh();
    float ix = rx + cardW + 8 * s, iy = cy + heroH - 44 * s - (cardH - 44 * s) / 2;
    if (iconBtn("##settings", ix, iy, 0)) settingsOpen = true;
    if (iconBtn("##close", ix + 52 * s, iy, 1)) toggle();
    cy += heroH + 16 * s;
}

// Tarjeta fija: pestañas y barra de progreso
static void topCard(ImDrawList *dl, float x, float &cy, float w) {
    float s = S, y0 = cy;
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    struct TabInfo { std::string label, icon, n; bool done; };
    std::vector<TabInfo> tabs;
    std::string overall = std::to_string(prog.total ? (int)std::lround(prog.done * 100.0 / prog.total) : 0) + "% " + L("total", "overall");
    tabs.push_back({L("Ruta", "Guide"), "icon_compass", overall, false});
    for (auto &sec : prog.sections) tabs.push_back({T(sec.title), sec.icon, count(sec.done, sec.total), sec.done == sec.total});
    tab = std::max(0, std::min(tab, (int)prog.sections.size()));
    std::vector<float> tws;
    float sum = 0;
    for (auto &t : tabs) {
        float tw1 = std::max(tw(F800, 13.5f, t.label.c_str()), tw(F700, 11.5f, t.n.c_str())) + 25 * s + 22 * s;
        tws.push_back(tw1);
        sum += tw1;
    }
    float avail = w - 12 * s - 2 * s * (tabs.size() - 1), extra = std::min(0.0f, (avail - sum) / tabs.size());
    float tx = x + 6 * s, ty = cy + 6 * s, th = 44 * s;
    for (size_t i = 0; i < tabs.size(); i++) {
        char id[16];
        snprintf(id, sizeof id, "##t%u", (unsigned)i);
        float tw1 = tws[i] + extra;
        bool hov, sel = tab == (int)i;
        if (hit(id, tx, ty, tw1, th, &hov)) tab = (int)i;
        ImVec2 a(tx, ty), b(tx + tw1, ty + th);
        if (sel) dl->AddRectFilled(a, b, col(P->accent, 0.14f), 11 * s);
        else if (hov) dl->AddRectFilled(a, b, col(P->paper2), 11 * s);
        art(dl, tabs[i].icon, tx + (11 + extra / s / 2) * s, ty + (th - 18 * s) / 2, 18 * s);
        float lx = tx + (11 + 25 + extra / s / 2) * s;
        put(dl, F800, 13.5f, lx, ty + 7 * s, 16.2f, col(sel ? P->accentInk : P->ink), tabs[i].label.c_str());
        put(dl, F700, 11.5f, lx, ty + 23.2f * s, 13.8f, col(tabs[i].done ? P->al : sel ? P->accentInk : P->soft), tabs[i].n.c_str());
        ring(dl, a, b, 11 * s);
        tx += tw1 + 2 * s;
    }
    cy += 56 * s;
    if (tab > 0) {
        // .secbar
        Section &sec = prog.sections[tab - 1];
        dl->AddLine(ImVec2(x, cy), ImVec2(x + w, cy), col(P->line), std::max(1.0f, s));
        float frac = sec.total ? sec.done / (float)sec.total : 0;
        std::string n = std::to_string(sec.done), d = " / " + std::to_string(sec.total), pct = std::to_string((int)std::lround(frac * 100)) + "%";
        float bx = x + 14 * s, by = cy + 10 * s;
        put(dl, FDISP, 24, bx, by, 24, col(P->ink), n.c_str());
        bx += tw(FDISP, 24, n.c_str());
        put(dl, F700, 13, bx, by + 3 * s, 24, col(P->soft), d.c_str());
        bx += tw(F700, 13, d.c_str()) + 12 * s;
        float pw = std::max(tw(F800, 15, pct.c_str()), 4 * tw(F800, 15, "0"));
        float rx = x + w - 14 * s;
        meter(dl, bx, by + 6 * s, rx - pw - 12 * s - bx, 12, frac);
        put(dl, F800, 15, rx - tw(F800, 15, pct.c_str()), by, 24, col(P->ink), pct.c_str());
        cy += 44 * s;
    }
    dl->ChannelsSetCurrent(0);
    card(dl, ImVec2(x, y0), ImVec2(x + w, cy));
    dl->ChannelsMerge();
}

// .filters: introducción, buscador, categoría, tipos y «ocultar lo conseguido»
static std::string filters(Section &sec, float x, float &cy, float w) {
    float s = S;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    cy += 14 * s;
    if (!T(sec.intro).empty()) {
        intro(dl, T(sec.intro), x, cy, w);
        cy += 10 * s;
    }
    if (sec.key == "stars" && prog.starsGame >= 0 && prog.starsGame != prog.starsComputed) {
        char b[220];
        snprintf(b, sizeof b, L("El juego dice %d estrellas y aquí se calculan %d: algunas dependen del pueblo; márcalas a mano.",
                                "The game says %d stars and %d are worked out here: some depend on the town; tick them by hand."),
                 prog.starsGame, prog.starsComputed);
        intro(dl, b, x, cy, w);
        cy += 10 * s;
    }
    if (sec.key == "picto" && prog.pictoDone >= 0) {
        char b[220];
        snprintf(b, sizeof b, L("El juego dice %d escenas hechas; la partida no dice cuáles: márcalas a mano.",
                                "The game says %d scenes done; the save doesn't say which: tick them by hand."), prog.pictoDone);
        intro(dl, b, x, cy, w);
        cy += 10 * s;
    }

    // Buscador y categoría
    ImGui::PushFont(faces[F700].font, 14 * s);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12 * s, 11.5f * s));
    float selW = 0;
    std::vector<Text> cats;
    std::string current;
    if (sec.key == "capsules") {
        for (auto &g : sec.groups)
            for (auto &it : g.items)
                if (std::none_of(cats.begin(), cats.end(), [&](const Text &t) { return t[1] == it.cat[1]; })) cats.push_back(it.cat);
        current = L("Todas las categorías", "All categories");
        selW = tw(F700, 14, current.c_str());
        for (auto &c : cats) {
            selW = std::max(selW, tw(F700, 14, T(c).c_str()));
            if (c[1] == catFilter) current = T(c);
        }
        selW += 58 * s;
    }
    ImGui::SetCursorScreenPos(ImVec2(x, cy));
    ImGui::SetNextItemWidth(w - (selW ? selW + 14 * s : 0));
    ImGui::InputTextWithHint("##search", L("Buscar…", "Search…"), search[tab], sizeof search[tab]);
    std::string q = lowerUtf8(search[tab]);
    if (selW) {
        ImGui::SetCursorScreenPos(ImVec2(x + w - selW, cy));
        ImGui::SetNextItemWidth(selW);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * s, 6 * s));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * s, 4 * s));
        if (ImGui::BeginCombo("##cat", current.c_str(), ImGuiComboFlags_HeightLargest)) {
            if (ImGui::Selectable(L("Todas las categorías", "All categories"), catFilter.empty())) catFilter.clear();
            for (auto &c : cats)
                if (ImGui::Selectable(T(c).c_str(), catFilter == c[1])) catFilter = c[1];
            ImGui::EndCombo();
        }
        ImGui::PopStyleVar(2);
    }
    ImGui::PopStyleVar();
    ImGui::PopFont();
    cy += 37 * s + 10 * s;

    // Tipos de coleccionable
    if (sec.key == "story") {
        float cx = x;
        for (auto &t : prog.types) {
            int a = 0, b = 0;
            for (auto &g : sec.groups)
                for (auto &it : g.items)
                    if (it.type == t.key) { b++; a += it.done; }
            bool on = typeFilter.count(t.key) > 0;
            std::string nn = count(a, b);
            float cw = chipW(T(t.name), t.icon, nn);
            if (cx > x && cx + cw > x + w) { cx = x; cy += (CHIP_H + 6) * s; }
            if (chip(t.key.c_str(), cx, cy, T(t.name), t.icon, nn, typeColor(t.key), on)) {
                if (on) typeFilter.erase(t.key);
                else typeFilter.insert(t.key);
            }
            cx += cw + 6 * s;
        }
        cy += (CHIP_H + 10) * s;
    }

    // .toggle
    const char *label = L("Ocultar lo conseguido", "Hide completed");
    float lw = 28 * s + tw(F700, 13.5f, label);
    bool hov;
    if (hit("##hide", x, cy, lw, 22 * s, &hov)) {
        cfg.hideDone = !cfg.hideDone;
        saveIni(L"HideCompleted", cfg.hideDone ? "1" : "0");
    }
    checkbox(dl, x, cy + s, 20, 6, cfg.hideDone, hov, P->soft);
    put(dl, F700, 13.5f, x + 28 * s, cy, 22, col(P->ink), label);
    ring(dl, ImVec2(x, cy), ImVec2(x + lw, cy + 22 * s), 6 * s);
    cy += 22 * s + 14 * s;
    return q;
}

static void panel() {
    ImGuiIO &io = ImGui::GetIO();
    float W = io.DisplaySize.x, H = io.DisplaySize.y, s = S;
    float pw = std::min(W, 940 * s), ph = H - 32 * s;
    float px = (W - pw) / 2, py = 16 * s;
    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##ts3c", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    clouds(dl, ImVec2(px + s, py + s), ImVec2(px + pw - s, py + ph - s));
    float x = px + 16 * s, w = pw - 32 * s, cy = py + 16 * s;
    hero(dl, x, cy, w);
    if (!visible) { ImGui::End(); return; }   // cerrado con la X
    if (prog.sections.empty()) { ImGui::End(); return; }
    topCard(dl, x, cy, w);

    // Contenido, con su propio desplazamiento por pestaña
    float footer = 30 * s;
    ImGui::SetCursorScreenPos(ImVec2(px, cy));
    char cid[20];
    snprintf(cid, sizeof cid, "##content%d", tab);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild(cid, ImVec2(pw, py + ph - footer - cy), ImGuiChildFlags_None);
    ImGui::PopStyleVar();
    float ccy = ImGui::GetCursorScreenPos().y;
    if (tab == 0) route(x, ccy, w);
    else {
        Section &sec = prog.sections[tab - 1];
        std::string q = filters(sec, x, ccy, w);
        for (auto &g : sec.groups) groupCard(sec, g, q, x, ccy, w);
    }
    ImGui::SetCursorScreenPos(ImVec2(x, ccy));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();

    // Pie: controles
    char vk[8] = "F2";
    if (cfg.hotkey >= VK_F1 && cfg.hotkey <= VK_F12) snprintf(vk, sizeof vk, "F%u", cfg.hotkey - VK_F1 + 1);
    char foot[200];
    if (lastWasPad)
        snprintf(foot, sizeof foot, "%s", L("Cruceta / stick: moverse · A: marcar · LB/RB: pestaña · Stick derecho: desplazar · Y: ocultar lo conseguido · B: cerrar",
                                            "D-pad / stick: move · A: tick · LB/RB: tab · Right stick: scroll · Y: hide completed · B: close"));
    else
        snprintf(foot, sizeof foot, es() ? "Ratón: pulsa para marcar · Rueda: desplazar · %s o Esc: cerrar" : "Mouse: click to tick · Wheel: scroll · %s or Esc: close", vk);
    put(dl, F500, 12.5f, x + (w - tw(F500, 12.5f, foot)) / 2, py + ph - footer, 30, col(P->soft), foot);

    sheet(px, py, pw, ph);
    ImGui::End();
}

// Cursor del juego (flecha, o la mano sobre lo que se puede pulsar)
static void drawCursor() {
    if (lastWasPad || !atlas) return;
    bool hand = ImGui::IsAnyItemHovered();
    const char *name = hand ? "cursor_pointer" : "cursor_arrow";
    if (!cells.count(name)) return;
    const Cell &c = cells[name];
    ImVec2 m = ImGui::GetIO().MousePos;
    float sz = 38 * S;
    ImVec2 hot = hand ? ImVec2(sz * 0.40f, sz * 0.08f) : ImVec2(sz * 0.5f, sz * 0.06f);
    ImGui::GetForegroundDrawList()->AddImage(atlas, ImVec2(m.x - hot.x, m.y - hot.y), ImVec2(m.x - hot.x + sz, m.y - hot.y + sz), c.uv0[0], c.uv1[0]);
}

void uiFrame() {
    ImGuiIO &io = ImGui::GetIO();
    screenH = io.DisplaySize.y;
    if (refreshState == 2) {
        // Las marcas hechas a mano mientras se leía ya están en progress.json, que también se ha leído
        prog = pending;
        capIndex.clear();
        for (auto &sec : prog.sections)
            for (auto &g : sec.groups)
                for (auto &it : g.items)
                    if (it.id.compare(0, 2, "c-") == 0) capIndex[it.id] = &it;
        layoutGen++;
        GetLocalTime(&readAt);
        InterlockedExchange(&refreshState, 0);
        log("Progress read: %d/%d, save %s, stars %d (game %d)", prog.done, prog.total, prog.saveFound ? "found" : "NOT found", prog.starsComputed, prog.starsGame);
    }
    if (!visible) return;
    if (themeDirty) applyTheme();
    float s = std::max(0.6f, std::min(screenH / 1080.0f * 1.15f * cfg.scale, io.DisplaySize.x / 940.0f));
    if (s != S) { S = s; layoutGen++; }
    panel();
    drawCursor();
}

}  // namespace overlay
