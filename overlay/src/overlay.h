// Estado compartido del overlay entre el proxy de XInput, los ganchos de D3D12 y el panel.
#pragma once
#include <windows.h>
#include <xinput.h>

#include <string>

namespace overlay {

struct Config {
    UINT hotkey = VK_F2;               // tecla para abrir/cerrar
    WORD padToggle = XINPUT_GAMEPAD_DPAD_DOWN;
    std::string lang = "es";           // "es" o "en"
    std::string theme = "auto";        // "auto" (el de Windows), "light" o "dark"
    std::string buttons = "xbox";      // cómo se dibujan los botones en las pistas: "xbox" o "ps"
    bool hideDone = false;             // ocultar lo conseguido
    float scale = 1.0f;
    std::wstring savePath;             // vacío = la de la app o la ubicación estándar
    int openAfter = 0;                 // [Test] OpenAfterSeconds: abre el panel solo a los N segundos (pruebas)
};

extern Config cfg;
extern std::wstring dataDir;           // <juego>\TS3Companion Overlay
extern volatile bool visible;
extern const char *volatile stage;     // paso actual del dibujo (lo apunta el registro de excepciones)

void log(const char *fmt, ...);

// Mando: lo llaman el filtro del XInputGetState del juego (cada vez que lee el mando) y el panel (una vez por fotograma).
// Detecta la pulsación que abre/cierra y devuelve lo que debe ver el juego (nada mientras el panel está abierto).
void padFilter(DWORD user, XINPUT_STATE *state);
// Estado real del mando (sin filtrar) para manejar el panel
bool padRead(DWORD user, XINPUT_STATE *state);

void hookFactory(IUnknown *factory);   // d3d12.cpp: la fábrica DXGI que crea el juego
void hookDirectInput(void *di, bool wide);   // d3d12.cpp: el DirectInput que crea el juego
unsigned long long uploadTexture(const void *rgba, int w, int h);   // d3d12.cpp: imagen a descriptor para ImGui
void cursorFree(bool panelOpen);   // main.cpp: suelta el cursor al abrir el panel y lo devuelve al juego al cerrar
void *systemDxgi(const char *name);    // main.cpp: función del dxgi.dll de Windows
void toggle();                         // ui.cpp
void demo(const char *spec);           // ui.cpp: TS3C_OVERLAY_DEMO="pestaña[,all]" abre el panel al arrancar (pruebas)
}  // namespace overlay
