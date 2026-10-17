/*
 * Sch1Mac D3D11 probe (MIT). Creates a window, a D3D11 device and a swap chain, clears and presents
 * a few frames, then exits 0. Run with WINEDEBUG=+loaddll to see which d3d11/dxgi/winemetal load.
 * Exit codes: 0 ok, 2 window, 3 device/swapchain, 4 present.
 */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

int main(void)
{
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"sch1mac_probe";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(L"sch1mac_probe", L"Sch1Mac D3D11 probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              100, 100, 320, 240, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 2;

    DXGI_SWAP_CHAIN_DESC sd = {0};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 320;
    sd.BufferDesc.Height = 240;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0, got;
    IDXGISwapChain *sc = NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &fl, 1, D3D11_SDK_VERSION,
                                               &sd, &sc, &dev, &got, &ctx);
    if (FAILED(hr)) { printf("PROBE_FAIL create hr=0x%08lx\n", (unsigned long)hr); return 3; }

    ID3D11Texture2D *bb = NULL;
    ID3D11RenderTargetView *rtv = NULL;
    IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
    const float color[4] = {0.1f, 0.6f, 0.3f, 1.0f};
    for (int i = 0; i < 30; i++) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, color);
        hr = IDXGISwapChain_Present(sc, 1, 0);
        if (FAILED(hr)) { printf("PROBE_FAIL present hr=0x%08lx\n", (unsigned long)hr); return 4; }
    }
    printf("PROBE_OK feature_level=0x%x\n", (unsigned)got);
    return 0;
}
