// Ganchos sobre los objetos del propio juego, puestos cuando el juego los crea (como el TS3 Mod Menu): su fábrica DXGI
// (llega por nuestra CreateDXGIFactory*, main.cpp), la cadena de presentación que crea con ella y sus dispositivos de
// DirectInput (por su import de DirectInput8Create). Se cambian solo las entradas necesarias de sus tablas virtuales:
// fábrica → CreateSwapChain(ForHwnd); cadena → Present y ResizeBuffers(1); DirectInput → CreateDevice, GetDeviceState y
// GetDeviceData. No se crean objetos de prueba ni se toca ExecuteCommandLists ni Present1 (la primera versión lo hacía y
// el juego entraba en bucle con el overlay de Steam). Los recursos de GPU se crean al abrir el panel por primera vez.
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <vector>

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "overlay.h"

namespace overlay {
bool uiWantsFrame();                  // ui.cpp: hay algo que dibujar (el panel está abierto)
void uiSetup(float height);
void uiLoadArt();                     // ui.cpp: arte del juego, después de iniciar el renderizador
void uiInput();
void uiFeedInput(HWND wnd, int width, int height);   // ui.cpp: ratón, teclado y mando a ImGui, antes de cada fotograma
void uiFrame();
void uiWndMessage(UINT msg, WPARAM w, LPARAM l);     // ui.cpp: caracteres y rueda que llegan a la ventana del juego
}  // namespace overlay

using namespace overlay;

// ---------- utilidades ----------
template <class T> static void release(T *&p) {
    if (p) { p->Release(); p = nullptr; }
}

// Cambia una entrada de una tabla virtual (una vez: si ya es nuestra, no hace nada)
static bool patch(void **vt, int idx, void *hook, void **orig) {
    if (vt[idx] == hook) return true;
    DWORD old;
    if (!VirtualProtect(&vt[idx], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) return false;
    *orig = vt[idx];
    vt[idx] = hook;
    VirtualProtect(&vt[idx], sizeof(void *), old, &old);
    return true;
}

// ---------- originales ----------
typedef HRESULT(WINAPI *PresentFn)(IDXGISwapChain *, UINT, UINT);
typedef HRESULT(WINAPI *ResizeFn)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef HRESULT(WINAPI *Resize1Fn)(IDXGISwapChain3 *, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT *, IUnknown *const *);
typedef HRESULT(WINAPI *CreateSCFn)(IDXGIFactory *, IUnknown *, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **);
typedef HRESULT(WINAPI *CreateSCHwndFn)(IDXGIFactory2 *, IUnknown *, HWND, const DXGI_SWAP_CHAIN_DESC1 *,
                                        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *, IDXGIOutput *, IDXGISwapChain1 **);
typedef HRESULT(WINAPI *CreateDevFn)(IDirectInput8W *, REFGUID, LPDIRECTINPUTDEVICE8W *, LPUNKNOWN);
typedef HRESULT(WINAPI *DevStateFn)(IDirectInputDevice8W *, DWORD, LPVOID);
typedef HRESULT(WINAPI *DevDataFn)(IDirectInputDevice8W *, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

static PresentFn oPresent;
static ResizeFn oResize;
static Resize1Fn oResize1;
static CreateSCFn oCreateSC;
static CreateSCHwndFn oCreateSCHwnd;
static CreateDevFn oCreateDevW, oCreateDevA;
static DevStateFn oStateW, oStateA;
static DevDataFn oDataW, oDataA;

static HRESULT WINAPI hkPresent(IDXGISwapChain *s, UINT sync, UINT flags);
static HRESULT WINAPI hkResize(IDXGISwapChain *s, UINT n, UINT w, UINT h, DXGI_FORMAT fmt, UINT fl);
static HRESULT WINAPI hkResize1(IDXGISwapChain3 *s, UINT n, UINT w, UINT h, DXGI_FORMAT fmt, UINT fl, const UINT *m, IUnknown *const *q);

// ---------- cadena de presentación del juego ----------
static ID3D12CommandQueue *swapQueue;    // la cola que el juego pasó al crear su cadena de presentación
static SRWLOCK queueLock = SRWLOCK_INIT;

// ---------- ventana del juego: con el panel abierto el juego no recibe ratón ni teclado ----------
static HWND gameWnd;
static WNDPROC oWndProc;
static LRESULT CALLBACK hkWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (visible) {
        if (m == WM_SETCURSOR && LOWORD(l) == HTCLIENT) {
            SetCursor(nullptr);   // se dibuja el cursor del juego
            return TRUE;
        }
        bool input = (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) || (m >= WM_KEYFIRST && m <= WM_KEYLAST) || m == WM_INPUT;
        if (input) {
            uiWndMessage(m, w, l);
            return m == WM_INPUT ? DefWindowProcW(h, m, w, l) : 0;
        }
    }
    return CallWindowProcW(oWndProc, h, m, w, l);
}
static void hookWindow(HWND w) {
    if (!w || w == gameWnd) return;
    gameWnd = w;
    oWndProc = (WNDPROC)SetWindowLongPtrW(w, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    log(oWndProc ? "Game window hooked" : "Game window could not be hooked: no mouse in the panel");
}

static void hookSwapChain(IUnknown *device, IUnknown *chainObj) {
    ID3D12CommandQueue *q = nullptr;
    if (!device || FAILED(device->QueryInterface(IID_PPV_ARGS(&q)))) return;   // no es D3D12: nada que hacer
    AcquireSRWLockExclusive(&queueLock);
    release(swapQueue);
    swapQueue = q;
    ReleaseSRWLockExclusive(&queueLock);
    IDXGISwapChain3 *sc = nullptr;
    if (!chainObj || FAILED(chainObj->QueryInterface(IID_PPV_ARGS(&sc)))) return;
    void **vt = *(void ***)sc;
    bool ok = patch(vt, 8, (void *)hkPresent, (void **)&oPresent) && patch(vt, 13, (void *)hkResize, (void **)&oResize) &&
              patch(vt, 39, (void *)hkResize1, (void **)&oResize1);
    DXGI_SWAP_CHAIN_DESC d;
    if (SUCCEEDED(sc->GetDesc(&d))) hookWindow(d.OutputWindow);
    sc->Release();
    log(ok ? "Game swap chain attached" : "Swap chain hooks failed: the overlay stays off");
}

static HRESULT WINAPI hkCreateSC(IDXGIFactory *f, IUnknown *dev, DXGI_SWAP_CHAIN_DESC *d, IDXGISwapChain **sc) {
    HRESULT r = oCreateSC(f, dev, d, sc);
    if (SUCCEEDED(r) && sc) hookSwapChain(dev, *sc);
    return r;
}
static HRESULT WINAPI hkCreateSCHwnd(IDXGIFactory2 *f, IUnknown *dev, HWND w, const DXGI_SWAP_CHAIN_DESC1 *d,
                                     const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fs, IDXGIOutput *o, IDXGISwapChain1 **sc) {
    HRESULT r = oCreateSCHwnd(f, dev, w, d, fs, o, sc);
    if (SUCCEEDED(r) && sc) hookSwapChain(dev, *sc);
    return r;
}

// La fábrica que crea el juego (main.cpp la pasa al salir de CreateDXGIFactory*)
void overlay::hookFactory(IUnknown *factory) {
    static volatile LONG done;
    IDXGIFactory2 *f = nullptr;
    if (done || !factory || FAILED(factory->QueryInterface(IID_PPV_ARGS(&f)))) return;
    void **vt = *(void ***)f;
    if (patch(vt, 10, (void *)hkCreateSC, (void **)&oCreateSC) && patch(vt, 15, (void *)hkCreateSCHwnd, (void **)&oCreateSCHwnd)) {
        InterlockedExchange(&done, 1);
        log("DXGI factory hooked");
    }
    f->Release();
}

// ---------- DirectInput: con el panel abierto el juego no recibe teclado ni ratón ----------
static HRESULT WINAPI hkStateW(IDirectInputDevice8W *d, DWORD cb, LPVOID data) {
    HRESULT r = oStateW(d, cb, data);
    if (visible && SUCCEEDED(r) && data) ZeroMemory(data, cb);
    return r;
}
static HRESULT WINAPI hkStateA(IDirectInputDevice8W *d, DWORD cb, LPVOID data) {
    HRESULT r = oStateA(d, cb, data);
    if (visible && SUCCEEDED(r) && data) ZeroMemory(data, cb);
    return r;
}
static HRESULT WINAPI hkDataW(IDirectInputDevice8W *d, DWORD cb, LPDIDEVICEOBJECTDATA od, LPDWORD n, DWORD fl) {
    HRESULT r = oDataW(d, cb, od, n, fl);
    if (visible && SUCCEEDED(r) && n) *n = 0;
    return r;
}
static HRESULT WINAPI hkDataA(IDirectInputDevice8W *d, DWORD cb, LPDIDEVICEOBJECTDATA od, LPDWORD n, DWORD fl) {
    HRESULT r = oDataA(d, cb, od, n, fl);
    if (visible && SUCCEEDED(r) && n) *n = 0;
    return r;
}
static HRESULT WINAPI hkCreateDevW(IDirectInput8W *di, REFGUID g, LPDIRECTINPUTDEVICE8W *dev, LPUNKNOWN outer) {
    HRESULT r = oCreateDevW(di, g, dev, outer);
    if (SUCCEEDED(r) && dev && *dev) {
        void **vt = *(void ***)*dev;
        patch(vt, 9, (void *)hkStateW, (void **)&oStateW);
        patch(vt, 10, (void *)hkDataW, (void **)&oDataW);
    }
    return r;
}
static HRESULT WINAPI hkCreateDevA(IDirectInput8W *di, REFGUID g, LPDIRECTINPUTDEVICE8W *dev, LPUNKNOWN outer) {
    HRESULT r = oCreateDevA(di, g, dev, outer);
    if (SUCCEEDED(r) && dev && *dev) {
        void **vt = *(void ***)*dev;
        if (vt[9] != (void *)hkStateW) {
            patch(vt, 9, (void *)hkStateA, (void **)&oStateA);
            patch(vt, 10, (void *)hkDataA, (void **)&oDataA);
        }
    }
    return r;
}

// El objeto que devuelve el DirectInput8Create del juego (main.cpp)
void overlay::hookDirectInput(void *di, bool wide) {
    if (!di) return;
    void **vt = *(void ***)di;
    if (wide ? patch(vt, 3, (void *)hkCreateDevW, (void **)&oCreateDevW) : (vt[3] == (void *)hkCreateDevW || patch(vt, 3, (void *)hkCreateDevA, (void **)&oCreateDevA)))
        log("DirectInput hooked (%s)", wide ? "W" : "A");
}

// ---------- dibujo ----------
struct Frame {
    ID3D12CommandAllocator *alloc = nullptr;
    ID3D12Resource *buffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fence = 0;
};
static IDXGISwapChain3 *chain;           // la cadena para la que están creados los recursos (sin referencia propia)
static ID3D12Device *device;
static ID3D12CommandQueue *queue;
static ID3D12DescriptorHeap *rtvHeap, *srvHeap;
static ID3D12GraphicsCommandList *cmdList;
static ID3D12Fence *fence;
static HANDLE fenceEvent;
static UINT64 fenceValue;
static std::vector<Frame> frames;
static DXGI_FORMAT format;
static bool imguiReady, broken;
static std::vector<int> freeSrv;
static UINT srvSize;
static LARGE_INTEGER lastTime, freq;

static void waitFor(UINT64 v) {
    if (fence && fence->GetCompletedValue() < v) {
        fence->SetEventOnCompletion(v, fenceEvent);
        WaitForSingleObject(fenceEvent, 2000);
    }
}

static void releaseBuffers() {
    if (fence && queue) {
        queue->Signal(fence, ++fenceValue);
        waitFor(fenceValue);
    }
    for (auto &f : frames) { release(f.buffer); release(f.alloc); }
    frames.clear();
    release(rtvHeap);
}

static bool createBuffers(IDXGISwapChain3 *sc) {
    DXGI_SWAP_CHAIN_DESC d;
    if (FAILED(sc->GetDesc(&d)) || !d.BufferCount) return false;
    D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, d.BufferCount, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    if (FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap)))) return false;
    UINT step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    frames.resize(d.BufferCount);
    for (UINT i = 0; i < d.BufferCount; i++) {
        Frame &f = frames[i];
        if (FAILED(sc->GetBuffer(i, IID_PPV_ARGS(&f.buffer))) ||
            FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&f.alloc))))
            return false;
        f.rtv = h;
        device->CreateRenderTargetView(f.buffer, nullptr, h);
        h.ptr += step;
    }
    format = d.BufferDesc.Format;
    return true;
}

static void srvAlloc(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *cpu, D3D12_GPU_DESCRIPTOR_HANDLE *gpu) {
    int i = freeSrv.empty() ? 0 : freeSrv.back();
    if (!freeSrv.empty()) freeSrv.pop_back();
    *cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    *gpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
    cpu->ptr += (SIZE_T)i * srvSize;
    gpu->ptr += (UINT64)i * srvSize;
}
static void srvFree(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
    freeSrv.push_back((int)((cpu.ptr - srvHeap->GetCPUDescriptorHandleForHeapStart().ptr) / srvSize));
}

static std::vector<ID3D12Resource *> textures;   // se quedan mientras dura el juego

// Sube una imagen RGBA a la GPU y devuelve su descriptor para ImGui (0 si falla). Espera a que termine la copia.
unsigned long long overlay::uploadTexture(const void *rgba, int w, int h) {
    if (!device || !queue || !srvHeap || !fence || w <= 0 || h <= 0) return 0;
    D3D12_HEAP_PROPERTIES heap = {D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = w;
    rd.Height = h;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rd.SampleDesc.Count = 1;
    ID3D12Resource *tex = nullptr, *up = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex))))
        return 0;
    UINT pitch = (w * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    D3D12_HEAP_PROPERTIES uheap = {D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC bd = {};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = (UINT64)pitch * h;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    void *mapped = nullptr;
    if (FAILED(device->CreateCommittedResource(&uheap, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&up))) ||
        FAILED(up->Map(0, nullptr, &mapped))) {
        release(up);
        release(tex);
        return 0;
    }
    for (int y = 0; y < h; y++) memcpy((char *)mapped + (size_t)y * pitch, (const char *)rgba + (size_t)y * w * 4, (size_t)w * 4);
    up->Unmap(0, nullptr);
    ID3D12CommandAllocator *alloc = nullptr;
    ID3D12GraphicsCommandList *list = nullptr;
    bool ok = SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))) &&
              SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list)));
    if (ok) {
        D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};
        dst.pResource = tex;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.pResource = up;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, (UINT)w, (UINT)h, 1, pitch};
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = tex;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        list->ResourceBarrier(1, &b);
        list->Close();
        ID3D12CommandList *lists[] = {list};
        queue->ExecuteCommandLists(1, lists);
        queue->Signal(fence, ++fenceValue);
        waitFor(fenceValue);
    }
    release(list);
    release(alloc);
    release(up);
    if (!ok) {
        release(tex);
        return 0;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
    srvAlloc(nullptr, &cpu, &gpu);
    device->CreateShaderResourceView(tex, &sv, cpu);
    textures.push_back(tex);
    return gpu.ptr;
}

static bool setup(IDXGISwapChain3 *sc) {
    if (!device && FAILED(sc->GetDevice(IID_PPV_ARGS(&device)))) {
        log("The swap chain is not D3D12: overlay disabled");
        broken = true;
        return false;
    }
    AcquireSRWLockShared(&queueLock);
    queue = swapQueue;
    if (queue) queue->AddRef();
    ReleaseSRWLockShared(&queueLock);
    if (!queue) { broken = true; log("The game's command queue is unknown: overlay disabled"); return false; }
    D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
    if (FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&srvHeap))) ||
        FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) || !createBuffers(sc))
        return false;
    srvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for (int i = 127; i >= 0; i--) freeSrv.push_back(i);
    fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[0].alloc, nullptr, IID_PPV_ARGS(&cmdList))))
        return false;
    cmdList->Close();

    DXGI_SWAP_CHAIN_DESC d;
    sc->GetDesc(&d);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    uiSetup((float)d.BufferDesc.Height);
    ImGui_ImplDX12_InitInfo info;
    info.Device = device;
    queue->AddRef();             // el backend la suelta al cerrarse
    info.CommandQueue = queue;
    info.NumFramesInFlight = (int)frames.size();
    info.RTVFormat = format;
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.SrvDescriptorHeap = srvHeap;
    info.SrvDescriptorAllocFn = srvAlloc;
    info.SrvDescriptorFreeFn = srvFree;
    if (!ImGui_ImplDX12_Init(&info)) return false;
    uiLoadArt();
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);
    log("Renderer ready: %u buffers, %ux%u, format %d", (unsigned)frames.size(), d.BufferDesc.Width, d.BufferDesc.Height, (int)format);
    return true;
}

static int drawn;                        // fotogramas dibujados (los primeros se apuntan en el registro)
static void step(const char *what) {
    stage = what;
    if (drawn < 3) log("  frame %d: %s", drawn, what);
}

static void render(IDXGISwapChain *s) {
    IDXGISwapChain3 *sc = nullptr;
    if (FAILED(s->QueryInterface(IID_PPV_ARGS(&sc)))) return;
    sc->Release();   // la cadena sigue viva mientras el juego presenta; no hace falta una referencia propia
    if (!imguiReady) {
        step("setup");
        if (!setup(sc)) {
            if (device && queue) broken = true;   // algo falló con todo disponible: no reintentar cada fotograma
            return;
        }
        imguiReady = true;
        chain = sc;
    }
    if (sc != chain) { releaseBuffers(); chain = sc; }   // el juego creó otra cadena
    if (frames.empty() && !createBuffers(sc)) return;

    step("begin");
    UINT idx = sc->GetCurrentBackBufferIndex();
    if (idx >= frames.size()) return;
    Frame &f = frames[idx];
    waitFor(f.fence);
    f.alloc->Reset();
    cmdList->Reset(f.alloc, nullptr);

    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = f.buffer;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmdList->ResourceBarrier(1, &b);
    cmdList->OMSetRenderTargets(1, &f.rtv, FALSE, nullptr);
    cmdList->SetDescriptorHeaps(1, &srvHeap);

    DXGI_SWAP_CHAIN_DESC d;
    sc->GetDesc(&d);
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)d.BufferDesc.Width, (float)d.BufferDesc.Height);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    io.DeltaTime = (float)(now.QuadPart - lastTime.QuadPart) / freq.QuadPart;
    if (io.DeltaTime <= 0 || io.DeltaTime > 0.5f) io.DeltaTime = 1.0f / 60;
    lastTime = now;

    step("ImGui new frame");
    uiFeedInput(gameWnd ? gameWnd : d.OutputWindow, (int)d.BufferDesc.Width, (int)d.BufferDesc.Height);
    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();
    step("panel");
    uiFrame();
    step("ImGui render");
    ImGui::Render();
    step("draw data (and font texture upload)");
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &b);
    cmdList->Close();
    step("execute");
    ID3D12CommandList *lists[] = {cmdList};
    queue->ExecuteCommandLists(1, lists);
    queue->Signal(fence, ++fenceValue);
    f.fence = fenceValue;
    if (drawn < 3) {
        HRESULT removed = device->GetDeviceRemovedReason();
        log("  frame %d done, back buffer %u of %u, device %s (0x%08lX)", drawn, idx, (unsigned)frames.size(), removed == S_OK ? "OK" : "REMOVED", (unsigned long)removed);
    }
    drawn++;
    stage = "present";
}

static void onPresent(IDXGISwapChain *s, UINT flags) {
    static bool first = true;
    if (first) { first = false; log("First frame presented"); }
    if (flags & DXGI_PRESENT_TEST || broken) return;
    uiInput();
    if (uiWantsFrame()) render(s);
}

static HRESULT WINAPI hkPresent(IDXGISwapChain *s, UINT sync, UINT flags) {
    onPresent(s, flags);
    return oPresent(s, sync, flags);
}
static HRESULT WINAPI hkResize(IDXGISwapChain *s, UINT n, UINT w, UINT h, DXGI_FORMAT fmt, UINT fl) {
    releaseBuffers();
    return oResize(s, n, w, h, fmt, fl);
}
static HRESULT WINAPI hkResize1(IDXGISwapChain3 *s, UINT n, UINT w, UINT h, DXGI_FORMAT fmt, UINT fl, const UINT *m, IUnknown *const *q) {
    releaseBuffers();
    return oResize1(s, n, w, h, fmt, fl, m, q);
}
