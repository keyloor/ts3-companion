// Programa de prueba que hace de «juego»: importa dxgi.dll y xinput1_4.dll como el juego (así Windows carga el dxgi.dll del
// overlay de su carpeta), presenta con D3D12, lee el mando cada fotograma y guarda capturas de su propia ventana
// (PrintWindow: nunca la pantalla ni teclas simuladas). Se compila como "Toy Story 3 overlay test.exe" (el overlay solo se
// activa en procesos con «Toy Story 3» en el nombre). El panel se abre poniendo antes TS3C_OVERLAY_DEMO=pestaña[,all].
// Uso: "Toy Story 3 overlay test.exe" <carpeta de salida> [game] [late]
//   game = como el juego: 2 búferes B8G8R8A8.  late = crea la cadena de presentación sin esperar a los ganchos
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <xinput.h>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

#include <cmath>
#include <cstdio>
#include <string>

static void capture(HWND w, const std::wstring &path) {
    RECT r;
    GetClientRect(w, &r);
    POINT p = {0, 0};
    ClientToScreen(w, &p);
    int W = r.right, H = r.bottom;
    HDC screen = GetDC(nullptr), mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
    SelectObject(mem, bmp);
    PrintWindow(w, mem, PW_CLIENTONLY | PW_RENDERFULLCONTENT);   // solo esta ventana, aunque esté tapada
    BITMAPINFOHEADER bi = {sizeof bi, W, -H, 1, 24, BI_RGB};
    int stride = (W * 3 + 3) & ~3;
    std::string px(stride * H, 0);
    GetDIBits(mem, bmp, 0, H, &px[0], (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    BITMAPFILEHEADER bf = {0x4D42, (DWORD)(sizeof bf + sizeof bi + px.size()), 0, 0, sizeof bf + sizeof bi};
    if (FILE *f = _wfopen(path.c_str(), L"wb")) {
        fwrite(&bf, sizeof bf, 1, f);
        fwrite(&bi, sizeof bi, 1, f);
        fwrite(px.data(), 1, px.size(), f);
        fclose(f);
    }
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

int wmain(int argc, wchar_t **argv) {
    std::wstring out = argc > 1 ? argv[1] : L".";
    bool late = false, game = false;
    for (int a = 2; a < argc; a++) { late |= std::wstring(argv[a]) == L"late"; game |= std::wstring(argv[a]) == L"game"; }
    const UINT N = game ? 2 : 3;
    if (!late) Sleep(1500);   // el juego tarda en crear su ventana: el hilo del overlay instala los ganchos antes

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TS3OverlayHost";
    RegisterClassW(&wc);
    HWND w = CreateWindowW(wc.lpszClassName, L"Toy Story 3 overlay test", WS_OVERLAPPEDWINDOW, 40, 40, 1600, 939, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(w, SW_SHOWNOACTIVATE);   // visible para PrintWindow, sin quitarle el foco a nadie

    ID3D12Device *dev;
    ID3D12CommandQueue *q;
    IDXGIFactory2 *fac;
    IDXGISwapChain1 *sc1;
    IDXGISwapChain3 *sc;
    D3D12_COMMAND_QUEUE_DESC qd = {D3D12_COMMAND_LIST_TYPE_DIRECT};
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev))) || FAILED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q))) ||
        FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&fac)))) { printf("d3d12 init failed\n"); return 1; }
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Format = game ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = N;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (FAILED(fac->CreateSwapChainForHwnd(q, w, &sd, nullptr, nullptr, &sc1)) || FAILED(sc1->QueryInterface(IID_PPV_ARGS(&sc)))) { printf("swap chain failed\n"); return 1; }

    ID3D12DescriptorHeap *heap;
    D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, N};
    dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap));
    UINT step = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ID3D12Resource *bufs[3];
    ID3D12CommandAllocator *alloc[3];
    for (UINT i = 0; i < N; i++) {
        sc->GetBuffer(i, IID_PPV_ARGS(&bufs[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += i * step;
        dev->CreateRenderTargetView(bufs[i], nullptr, h);
        dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc[i]));
    }
    ID3D12GraphicsCommandList *list;
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[0], nullptr, IID_PPV_ARGS(&list));
    list->Close();
    ID3D12Fence *fence;
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    UINT64 fv = 0, fvs[3] = {};
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    for (int frame = 0; frame < 240; frame++) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
        XINPUT_STATE st;
        XInputGetState(0, &st);   // por la tabla de importaciones, como el juego
        UINT i = sc->GetCurrentBackBufferIndex();
        if (fence->GetCompletedValue() < fvs[i]) { fence->SetEventOnCompletion(fvs[i], ev); WaitForSingleObject(ev, INFINITE); }
        alloc[i]->Reset();
        list->Reset(alloc[i], nullptr);
        D3D12_RESOURCE_BARRIER b = {};
        b.Transition.pResource = bufs[i];
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        list->ResourceBarrier(1, &b);
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += i * step;
        float t = frame / 120.0f, color[4] = {0.35f + 0.1f * sinf(t), 0.55f, 0.75f, 1};
        list->ClearRenderTargetView(h, color, 0, nullptr);
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        list->ResourceBarrier(1, &b);
        list->Close();
        ID3D12CommandList *l[] = {list};
        q->ExecuteCommandLists(1, l);
        sc->Present(1, 0);
        q->Signal(fence, ++fv);
        fvs[i] = fv;

        if (frame == 60) capture(w, out + L"\\early.bmp");
        if (frame == 200) capture(w, out + L"\\shot.bmp");
    }
    q->Signal(fence, ++fv);
    fence->SetEventOnCompletion(fv, ev);
    WaitForSingleObject(ev, INFINITE);
    printf("host done\n");
    return 0;
}
