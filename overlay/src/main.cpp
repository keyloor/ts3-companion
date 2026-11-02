// TS3 Companion Overlay: dxgi.dll que el juego carga desde su carpeta (como ReShade). Reenvía todo al dxgi.dll de
// Windows y, al arrancar, engancha D3D12 para dibujar el panel del Companion encima del juego (d3d12.cpp, ui.cpp).
// No cambia nada del juego ni de la partida: solo lee archivos y dibuja.
// No se usa xinput1_4.dll: el overlay de Steam busca ese módulo por nombre y, con un proxy, entraba en bucle (desbordamiento
// de pila al arrancar). El mando del juego se filtra cambiando su entrada de XInputGetState en la tabla de importaciones.
#include <windows.h>
#include <xinput.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <cstdarg>
#include <cstdio>
#include <cwchar>

#include "overlay.h"

namespace overlay {
Config cfg;
std::wstring dataDir;
volatile bool visible = false;
const char *volatile stage = "idle";

static void logStack();

// Registro de excepciones graves (primera oportunidad): código, módulo + desplazamiento y paso del dibujo.
// No las maneja: el juego sigue como siempre; solo deja constancia en overlay.log.
static LONG CALLBACK onException(EXCEPTION_POINTERS *e) {
    static volatile LONG count;
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_STACK_OVERFLOW &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != 0xC0000409 && code != 0xC0000374)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&count) > 10) return EXCEPTION_CONTINUE_SEARCH;
    void *at = e->ExceptionRecord->ExceptionAddress;
    HMODULE mod = nullptr;
    wchar_t name[MAX_PATH] = L"?";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)at, &mod))
        GetModuleFileNameW(mod, name, MAX_PATH);
    const wchar_t *base = wcsrchr(name, L'\\');
    char n[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, base ? base + 1 : name, -1, n, sizeof n, nullptr, nullptr);
    ULONG_PTR info = e->ExceptionRecord->NumberParameters > 1 ? e->ExceptionRecord->ExceptionInformation[1] : 0;
    log("Exception %08lX at %s+0x%llX (data 0x%llX), thread %lu, overlay step: %s", code, n,
        (unsigned long long)((char *)at - (char *)mod), (unsigned long long)info, GetCurrentThreadId(), stage);
    if (code == EXCEPTION_STACK_OVERFLOW) logStack();
    return EXCEPTION_CONTINUE_SEARCH;
}

static FILE *logFile;

// Pila de llamadas tras un desbordamiento: queda muy poca pila, así que todo va en búferes estáticos
static void logStack() {
    static void *frames[48];
    static char name[MAX_PATH], line[MAX_PATH + 64];
    USHORT n = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
    for (USHORT i = 0; i < n; i++) {
        HMODULE mod = nullptr;
        strcpy(name, "?");
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)frames[i], &mod))
            GetModuleFileNameA(mod, name, MAX_PATH);
        const char *b = strrchr(name, '\\');
        wsprintfA(line, "  #%u %s+0x%lX\n", i, b ? b + 1 : name, (unsigned long)((char *)frames[i] - (char *)mod));
        if (logFile) { fputs(line, logFile); fflush(logFile); }
    }
}
static SRWLOCK logLock = SRWLOCK_INIT;

void log(const char *fmt, ...) {
    AcquireSRWLockExclusive(&logLock);
    if (logFile) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(logFile, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        va_list a;
        va_start(a, fmt);
        vfprintf(logFile, fmt, a);
        va_end(a);
        fputc('\n', logFile);
        fflush(logFile);
    }
    ReleaseSRWLockExclusive(&logLock);
}

// ---------- mando ----------
static SRWLOCK padLock = SRWLOCK_INIT;
static WORD prevButtons[4];
static WORD suppress[4];               // botones que el juego no debe ver hasta soltarlos

// Detecta la pulsación que abre el panel (con el panel abierto la cruceta navega: se cierra con B o Vista, ui.cpp);
// al cerrar, lo que sigue pulsado no llega al juego hasta soltarlo
static void padEdge(DWORD user, WORD b) {
    if (!visible && b & cfg.padToggle && !(prevButtons[user] & cfg.padToggle)) {
        toggle();
        suppress[user] |= b;
    }
    prevButtons[user] = b;
    suppress[user] &= b;
}

void padFilter(DWORD user, XINPUT_STATE *s) {
    if (user >= 4 || !s) return;
    AcquireSRWLockExclusive(&padLock);
    padEdge(user, s->Gamepad.wButtons);
    if (visible) {
        suppress[user] = s->Gamepad.wButtons;
        DWORD packet = s->dwPacketNumber;
        ZeroMemory(s, sizeof *s);
        s->dwPacketNumber = packet;
    } else s->Gamepad.wButtons &= ~suppress[user];
    ReleaseSRWLockExclusive(&padLock);
}

// ---------- xinput1_4.dll de Windows ----------
static HMODULE real() {
    static HMODULE h = [] {
        wchar_t p[MAX_PATH];
        UINT n = GetSystemDirectoryW(p, MAX_PATH);
        wcscpy(p + n, L"\\xinput1_4.dll");
        return LoadLibraryW(p);
    }();
    return h;
}
template <class F> static F fn(const char *name) { return real() ? (F)GetProcAddress(real(), name) : nullptr; }

bool padRead(DWORD user, XINPUT_STATE *s) {
    static auto f = fn<DWORD(WINAPI *)(DWORD, XINPUT_STATE *)>("XInputGetState");
    if (!f || f(user, s) != ERROR_SUCCESS) return false;
    AcquireSRWLockExclusive(&padLock);
    padEdge(user, s->Gamepad.wButtons);
    ReleaseSRWLockExclusive(&padLock);
    return true;
}

// ---------- mando del juego: su import de XInputGetState pasa por padFilter ----------
typedef DWORD(WINAPI *GetStateFn)(DWORD, XINPUT_STATE *);
static GetStateFn gameGetState;
static DWORD WINAPI hkGameGetState(DWORD user, XINPUT_STATE *s) {
    DWORD r = gameGetState(user, s);
    if (r == ERROR_SUCCESS) padFilter(user, s);
    return r;
}

// Cambia en la tabla de importaciones del exe la función `name` (u ordinal) de un módulo que empieza por `dll`.
// Devuelve la que había, o nullptr si el exe no la importa.
static void *hookImport(const char *dll, const char *name, WORD ordinal, void *hook) {
    auto base = (BYTE *)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    auto &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return nullptr;
    for (auto imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); imp->Name; imp++) {
        if (_strnicmp((char *)(base + imp->Name), dll, strlen(dll)) != 0) continue;
        auto names = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto slots = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            bool match = IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)
                             ? ordinal && IMAGE_ORDINAL(names->u1.Ordinal) == ordinal
                             : strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name, name) == 0;
            if (!match) continue;
            DWORD old;
            if (!VirtualProtect(&slots->u1.Function, sizeof(void *), PAGE_READWRITE, &old)) return nullptr;
            void *orig = (void *)slots->u1.Function;
            slots->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&slots->u1.Function, sizeof(void *), old, &old);
            return orig;
        }
    }
    return nullptr;
}

// ---------- cursor: con el panel abierto el juego no lo mueve, no lo encierra y lo ve quieto ----------
typedef BOOL(WINAPI *SetCursorPosFn)(int, int);
typedef BOOL(WINAPI *ClipCursorFn)(const RECT *);
typedef BOOL(WINAPI *GetCursorPosFn)(POINT *);
static SetCursorPosFn gameSetCursorPos;
static ClipCursorFn gameClipCursor;
static GetCursorPosFn gameGetCursorPos;
static RECT gameClip;
static bool gameClipped;
static POINT gameSeen;              // lo último que el juego vio con el panel cerrado
static bool gameSeenValid;

static BOOL WINAPI hkSetCursorPos(int x, int y) {
    if (visible) return TRUE;
    return gameSetCursorPos(x, y);
}
static BOOL WINAPI hkClipCursor(const RECT *r) {
    gameClipped = r != nullptr;
    if (r) gameClip = *r;
    if (visible) return TRUE;
    return gameClipCursor(r);
}
static BOOL WINAPI hkGetCursorPos(POINT *p) {
    if (visible && gameSeenValid && p) {
        *p = gameSeen;
        return TRUE;
    }
    BOOL r = gameGetCursorPos(p);
    if (r && p) { gameSeen = *p; gameSeenValid = true; }
    return r;
}

void overlay::cursorFree(bool panelOpen) {
    if (panelOpen) ClipCursor(nullptr);                     // la función de Windows, no la del juego
    else if (gameClipped) ClipCursor(&gameClip);
    else ClipCursor(nullptr);
    if (!panelOpen && gameSeenValid) SetCursorPos(gameSeen.x, gameSeen.y);   // donde el juego lo dejó
}

// DirectInput del juego: el objeto que crea se engancha al salir (d3d12.cpp)
typedef HRESULT(WINAPI *DI8CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
static DI8CreateFn gameDI8Create;
static HRESULT WINAPI hkGameDI8Create(HINSTANCE inst, DWORD ver, REFIID riid, LPVOID *out, LPUNKNOWN outer) {
    HRESULT r = gameDI8Create(inst, ver, riid, out, outer);
    if (SUCCEEDED(r) && out) hookDirectInput(*out, IsEqualIID(riid, IID_IDirectInput8W));
    return r;
}

// ---------- configuración: <juego>\TS3Companion Overlay\overlay.ini ----------
static std::string ini(const wchar_t *key, const wchar_t *def) {
    wchar_t buf[512];
    GetPrivateProfileStringW(L"Overlay", key, def, buf, 512, (dataDir + L"\\overlay.ini").c_str());
    char out[1024];
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, sizeof out, nullptr, nullptr);
    return out;
}

static void loadConfig() {
    std::wstring path = dataDir + L"\\overlay.ini";
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (FILE *f = _wfopen(path.c_str(), L"w")) {
            fputs("; TS3 Companion Overlay\n"
                  "; Hotkey: F1..F12.  PadButton: DPadDown, DPadUp, DPadLeft, DPadRight, Back, Start, LeftThumb, RightThumb.\n"
                  "; Language: auto, es, en.  Theme: auto, light, dark.  Buttons: xbox, ps.  Scale: 1.0 = normal size.\n"
                  "; SavePath: empty = the Companion's save, or the standard location.\n"
                  "[Overlay]\nHotkey=F2\nPadButton=DPadDown\nLanguage=auto\nTheme=auto\nButtons=xbox\nScale=1.0\nSavePath=\n", f);
            fclose(f);
        }
    }
    std::string k = ini(L"Hotkey", L"F2");
    if ((k[0] == 'F' || k[0] == 'f') && atoi(k.c_str() + 1) >= 1 && atoi(k.c_str() + 1) <= 12) cfg.hotkey = VK_F1 + atoi(k.c_str() + 1) - 1;
    struct { const char *name; WORD bit; } pads[] = {{"dpaddown", XINPUT_GAMEPAD_DPAD_DOWN}, {"dpadup", XINPUT_GAMEPAD_DPAD_UP},
        {"dpadleft", XINPUT_GAMEPAD_DPAD_LEFT}, {"dpadright", XINPUT_GAMEPAD_DPAD_RIGHT}, {"back", XINPUT_GAMEPAD_BACK},
        {"start", XINPUT_GAMEPAD_START}, {"leftthumb", XINPUT_GAMEPAD_LEFT_THUMB}, {"rightthumb", XINPUT_GAMEPAD_RIGHT_THUMB}};
    std::string p = ini(L"PadButton", L"DPadDown");
    for (auto &c : p) c = (char)tolower((unsigned char)c);
    for (auto &e : pads)
        if (p == e.name) cfg.padToggle = e.bit;
    std::string l = ini(L"Language", L"auto");
    cfg.lang = l == "es" || l == "en" ? l : PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_SPANISH ? "es" : "en";
    std::string th = ini(L"Theme", L"auto");
    cfg.theme = th == "light" || th == "dark" ? th : "auto";
    cfg.buttons = ini(L"Buttons", L"xbox") == "ps" ? "ps" : "xbox";
    cfg.hideDone = ini(L"HideCompleted", L"0") == "1";
    float s = (float)atof(ini(L"Scale", L"1.0").c_str());
    cfg.scale = s >= 0.5f && s <= 3.0f ? s : 1.0f;
    wchar_t t[16];
    GetPrivateProfileStringW(L"Test", L"OpenAfterSeconds", L"0", t, 16, path.c_str());
    cfg.openAfter = _wtoi(t);
    std::string sp = ini(L"SavePath", L"");
    if (!sp.empty()) {
        wchar_t w[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, sp.c_str(), -1, w, MAX_PATH);
        cfg.savePath = w;
    }
}

// Se llama desde DllMain, antes de que el juego empiece: solo archivos y la tabla de importaciones del exe
static void start() {
    CreateDirectoryW(dataDir.c_str(), nullptr);
    logFile = _wfopen((dataDir + L"\\overlay.log").c_str(), L"w");
    loadConfig();
    AddVectoredExceptionHandler(1, onException);
    log("TS3 Companion Overlay started (hotkey VK %u, pad 0x%04X, lang %s)", cfg.hotkey, cfg.padToggle, cfg.lang.c_str());
    gameGetState = (GetStateFn)hookImport("xinput", "XInputGetState", 2, (void *)hkGameGetState);
    log(gameGetState ? "Controller filter ready" : "The game's XInput import was not found: the controller also reaches the game");
    gameDI8Create = (DI8CreateFn)hookImport("dinput8", "DirectInput8Create", 0, (void *)hkGameDI8Create);
    log(gameDI8Create ? "Waiting for the game's DirectInput" : "The game's DirectInput import was not found: keys also reach the game");
    gameSetCursorPos = (SetCursorPosFn)hookImport("user32", "SetCursorPos", 0, (void *)hkSetCursorPos);
    gameClipCursor = (ClipCursorFn)hookImport("user32", "ClipCursor", 0, (void *)hkClipCursor);
    gameGetCursorPos = (GetCursorPosFn)hookImport("user32", "GetCursorPos", 0, (void *)hkGetCursorPos);
    if (!gameSetCursorPos) gameSetCursorPos = SetCursorPos;   // el juego no las importa: no hace falta engancharlas
    if (!gameClipCursor) gameClipCursor = ClipCursor;
    if (!gameGetCursorPos) gameGetCursorPos = GetCursorPos;
    log("Cursor hooks ready");
    char spec[512];
    DWORD n = GetEnvironmentVariableA("TS3C_OVERLAY_DEMO", spec, sizeof spec);
    if (n && n < sizeof spec) demo(spec);
}
}  // namespace overlay

using namespace overlay;

// ---------- dxgi.dll de Windows: las 20 exportaciones saltan a la de verdad ----------
// Los punteros se rellenan en DllMain, antes de que el juego pueda llamar a nada de DXGI.
extern "C" {
void *dxgi_real[20];
}
static const char *const DXGI_NAMES[20] = {
    "ApplyCompatResolutionQuirking",
    "CompatString",
    "CompatValue",
    "DXGIDumpJournal",
    "PIXBeginCapture",
    "PIXEndCapture",
    "PIXGetCaptureState",
    "SetAppCompatStringPointer",
    "UpdateHMDEmulationStatus",
    "CreateDXGIFactory",
    "CreateDXGIFactory1",
    "CreateDXGIFactory2",
    "DXGID3D10CreateDevice",
    "DXGID3D10CreateLayeredDevice",
    "DXGID3D10GetLayeredDeviceSize",
    "DXGID3D10RegisterLayers",
    "DXGIDeclareAdapterRemovalSupport",
    "DXGIDisableVBlankVirtualization",
    "DXGIGetDebugInterface1",
    "DXGIReportAdapterConfiguration"};
static HRESULT WINAPI dxgiMissing() { return E_NOTIMPL; }

// Cada exportación es un salto a dxgi_real[i] (mismos nombres y orden que dxgi.def)
__asm__(".text\n.globl ApplyCompatResolutionQuirking\n.def ApplyCompatResolutionQuirking; .scl 2; .type 32; .endef\nApplyCompatResolutionQuirking:\n\tjmpq *dxgi_real+0(%rip)\n");
__asm__(".text\n.globl CompatString\n.def CompatString; .scl 2; .type 32; .endef\nCompatString:\n\tjmpq *dxgi_real+8(%rip)\n");
__asm__(".text\n.globl CompatValue\n.def CompatValue; .scl 2; .type 32; .endef\nCompatValue:\n\tjmpq *dxgi_real+16(%rip)\n");
__asm__(".text\n.globl DXGIDumpJournal\n.def DXGIDumpJournal; .scl 2; .type 32; .endef\nDXGIDumpJournal:\n\tjmpq *dxgi_real+24(%rip)\n");
__asm__(".text\n.globl PIXBeginCapture\n.def PIXBeginCapture; .scl 2; .type 32; .endef\nPIXBeginCapture:\n\tjmpq *dxgi_real+32(%rip)\n");
__asm__(".text\n.globl PIXEndCapture\n.def PIXEndCapture; .scl 2; .type 32; .endef\nPIXEndCapture:\n\tjmpq *dxgi_real+40(%rip)\n");
__asm__(".text\n.globl PIXGetCaptureState\n.def PIXGetCaptureState; .scl 2; .type 32; .endef\nPIXGetCaptureState:\n\tjmpq *dxgi_real+48(%rip)\n");
__asm__(".text\n.globl SetAppCompatStringPointer\n.def SetAppCompatStringPointer; .scl 2; .type 32; .endef\nSetAppCompatStringPointer:\n\tjmpq *dxgi_real+56(%rip)\n");
__asm__(".text\n.globl UpdateHMDEmulationStatus\n.def UpdateHMDEmulationStatus; .scl 2; .type 32; .endef\nUpdateHMDEmulationStatus:\n\tjmpq *dxgi_real+64(%rip)\n");
__asm__(".text\n.globl DXGID3D10CreateDevice\n.def DXGID3D10CreateDevice; .scl 2; .type 32; .endef\nDXGID3D10CreateDevice:\n\tjmpq *dxgi_real+96(%rip)\n");
__asm__(".text\n.globl DXGID3D10CreateLayeredDevice\n.def DXGID3D10CreateLayeredDevice; .scl 2; .type 32; .endef\nDXGID3D10CreateLayeredDevice:\n\tjmpq *dxgi_real+104(%rip)\n");
__asm__(".text\n.globl DXGID3D10GetLayeredDeviceSize\n.def DXGID3D10GetLayeredDeviceSize; .scl 2; .type 32; .endef\nDXGID3D10GetLayeredDeviceSize:\n\tjmpq *dxgi_real+112(%rip)\n");
__asm__(".text\n.globl DXGID3D10RegisterLayers\n.def DXGID3D10RegisterLayers; .scl 2; .type 32; .endef\nDXGID3D10RegisterLayers:\n\tjmpq *dxgi_real+120(%rip)\n");
__asm__(".text\n.globl DXGIDeclareAdapterRemovalSupport\n.def DXGIDeclareAdapterRemovalSupport; .scl 2; .type 32; .endef\nDXGIDeclareAdapterRemovalSupport:\n\tjmpq *dxgi_real+128(%rip)\n");
__asm__(".text\n.globl DXGIDisableVBlankVirtualization\n.def DXGIDisableVBlankVirtualization; .scl 2; .type 32; .endef\nDXGIDisableVBlankVirtualization:\n\tjmpq *dxgi_real+136(%rip)\n");
__asm__(".text\n.globl DXGIGetDebugInterface1\n.def DXGIGetDebugInterface1; .scl 2; .type 32; .endef\nDXGIGetDebugInterface1:\n\tjmpq *dxgi_real+144(%rip)\n");
__asm__(".text\n.globl DXGIReportAdapterConfiguration\n.def DXGIReportAdapterConfiguration; .scl 2; .type 32; .endef\nDXGIReportAdapterConfiguration:\n\tjmpq *dxgi_real+152(%rip)\n");

// Las tres que crean fábricas: llaman a la de Windows y enganchan la fábrica que recibe el juego (d3d12.cpp)
static bool active = true;   // false fuera del juego: solo reenvía
extern "C" HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **out) {
    HRESULT r = ((HRESULT(WINAPI *)(REFIID, void **))dxgi_real[9])(riid, out);
    if (active && SUCCEEDED(r) && out) hookFactory((IUnknown *)*out);
    return r;
}
extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **out) {
    HRESULT r = ((HRESULT(WINAPI *)(REFIID, void **))dxgi_real[10])(riid, out);
    if (active && SUCCEEDED(r) && out) hookFactory((IUnknown *)*out);
    return r;
}
extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, void **out) {
    HRESULT r = ((HRESULT(WINAPI *)(UINT, REFIID, void **))dxgi_real[11])(flags, riid, out);
    if (active && SUCCEEDED(r) && out) hookFactory((IUnknown *)*out);
    return r;
}

void *overlay::systemDxgi(const char *name) {
    for (int i = 0; i < 20; i++)
        if (!strcmp(DXGI_NAMES[i], name)) return dxgi_real[i];
    return nullptr;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        wchar_t sys[MAX_PATH];
        UINT n = GetSystemDirectoryW(sys, MAX_PATH);
        wcscpy(sys + n, L"\\dxgi.dll");
        HMODULE dxgi = LoadLibraryW(sys);
        for (int i = 0; i < 20; i++) {
            void *f = dxgi ? (void *)GetProcAddress(dxgi, DXGI_NAMES[i]) : nullptr;
            dxgi_real[i] = f ? f : (void *)dxgiMissing;
        }
        if (!dxgi) return TRUE;   // sin el dxgi de Windows no hay nada que hacer: las funciones devuelven error
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(inst, path, MAX_PATH);
        if (wchar_t *slash = wcsrchr(path, L'\\')) *slash = 0;
        dataDir = std::wstring(path) + L"\\TS3Companion Overlay";
        // Solo dentro del juego: cualquier otro programa que cargue este archivo recibe un dxgi normal
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (wcsstr(exe, L"Toy Story 3")) start();
        else active = false;
    }
    return TRUE;
}
