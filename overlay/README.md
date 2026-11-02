# TS3 Companion Overlay

El Companion completo dentro del juego: las mismas pestañas que la app (Ruta, Logros, Historia, Cápsulas, Estrellas y
Pict-O-Matic) con buscador, filtros, «ocultar lo conseguido», marcar a mano y las imágenes del propio juego. Se ve como la app: sus fuentes, sus medidas y sus temas claro y oscuro. Se maneja con
el ratón (con el cursor del juego), el teclado o el mando. Lee la partida él solo cada vez que lo abres; no hace falta
tener abierto el Companion. Lo que marques a mano se guarda en el mismo sitio que la app de escritorio, así que las dos
ven lo mismo.

## Instalar

Se puede tener sin la app de escritorio: la descarga `TS3Companion-InGame-<versión>.zip` se descomprime en la carpeta del
juego. Con la app, basta con Opciones › Dentro del juego › Instalar, y la app lo mantiene al día.

En la carpeta del juego (`...\steamapps\common\Toy Story 3 Complete Edition`), junto a `Toy Story 3 Complete Edition.exe`:

- `dxgi.dll`
- la carpeta `TS3Companion Overlay` (con `content.json` y `savemap.json`)

Para quitarlo, borra esos dos. No cambia ningún archivo del juego ni la partida.

Funciona junto al TS3 Mod Menu (ese usa `dinput8.dll`; este, `dxgi.dll`). No se puede usar a la vez que ReShade, que también
usa `dxgi.dll`. (Una primera versión usaba `xinput1_4.dll`, pero el overlay de Steam se engancha a ese módulo por su nombre y
el juego entraba en bucle al arrancar.)

## Controles

| | Ratón y teclado | Mando |
|---|---|---|
| Abrir / cerrar | F2 (cerrar también con Esc o la ✕) | Abrir: D-pad abajo · Cerrar: B o Vista |
| Marcar / desmarcar | Clic en la casilla | A sobre la casilla |
| Cambiar de pestaña | Clic en la pestaña | LB / RB |
| Desplazar | Rueda | Stick derecho |
| Moverse | Tab / flechas | Cruceta o stick izquierdo |
| Ocultar lo conseguido | Casilla «Ocultar lo conseguido» | Y |

Mientras el panel está abierto el juego no recibe el ratón, el teclado ni el mando, y suelta el cursor. Lo que se detecta
en la partida o en Steam se marca solo y no se puede desmarcar a mano; lo demás (escenas de Pict-O-Matic, algunas estrellas)
se marca con un clic.

## Opciones

El engranaje del panel cambia el idioma, el tema y los botones de las pistas. `TS3Companion Overlay\overlay.ini` (se crea solo
la primera vez) guarda eso (`Language`: auto, es, en; `Theme`: auto, light, dark; `Buttons`: xbox, ps) y además la tecla
(`Hotkey`), el botón del mando (`PadButton`), el tamaño (`Scale`) y otra ruta de partida (`SavePath`). `overlay.log` cuenta qué pasó al arrancar, por si algo no va.

## Compilar

Desde `companion/`: `python tools/build_content.py` y después `python overlay/build.py` (necesita llvm-mingw; ver el propio
script). Sale en `build/overlay/`; `python tools/package_release.py` hace los dos zip (`README.txt` es el que va en el de esta versión). `python overlay/build.py --test` compila además `progress_test.exe`, que imprime lo
conseguido según la partida para compararlo con `tools/sync_cli.py`.

`fonts/*.ttf` salen de las fuentes y los iconos de la app con `tools/make_overlay_fonts.py` (solo hay que repetirlo si cambian) y
`build.py` los mete en la DLL.

Usa [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9b (licencia MIT, `third_party/imgui/LICENSE.txt`).
