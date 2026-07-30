#include "render.h"
#include "framework.h"
#include "..\minhook\include\MinHook.h"
#include "resolve_log.h"

#include <d3d11.h>
#include <dxgi.h>

#include "..\third_party\imgui\imgui.h"
#include "..\third_party\imgui\imgui_impl_dx11.h"
#include "..\third_party\imgui\imgui_impl_win32.h"
#include "feature.h"
// Forward declare — header has it behind #if 0
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- Function pointer types ---
typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(IDXGISwapChain*, UINT SyncInterval, UINT Flags);
typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffersFn)(
    IDXGISwapChain*, UINT BufferCount, UINT Width, UINT Height,
    DXGI_FORMAT NewFormat, UINT SwapChainFlags
);

// --- Originals ---
static PresentFn OriginalPresent = nullptr;
static ResizeBuffersFn OriginalResizeBuffers = nullptr;

// --- Vtable addresses (kept for EnableHook / RemoveHook) ---
static void* g_presentAddr = nullptr;
static void* g_resizeAddr = nullptr;

// --- Input passthrough ---
static WNDPROC OriginalWndProc = nullptr;

// --- Overlay toggle ---
static bool g_overlayVisible = true;
static bool g_insertWasDown = false;
static bool g_endWasDown = false;

// --- ImGui state ---
static bool g_imguiInitialized = false;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static ID3D11RenderTargetView* g_mainRTV = nullptr;

// ============================================================
// CreateMainRTV — bind target for ImGui draw data.
// Unity leaves no RTV bound at Present time, so draw calls go
// nowhere unless we bind the swapchain backbuffer ourselves.
// Recreated lazily after ResizeBuffers releases it.
// ============================================================
static bool CreateMainRTV(IDXGISwapChain* pSwapChain) {
    if (g_mainRTV) return true;
    if (!g_pd3dDevice) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr) || !backBuffer) {
        LOG("[P1] CreateMainRTV: GetBuffer failed hr=0x%08X", hr);
        return false;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRTV);
    backBuffer->Release();

    if (FAILED(hr)) {
        LOG("[P1] CreateMainRTV: CreateRenderTargetView failed hr=0x%08X", hr);
        g_mainRTV = nullptr;
        return false;
    }

    LOG("[P1] CreateMainRTV: RTV=%p", g_mainRTV);
    return true;
}

// ============================================================
// InitImGui — one-time setup of ImGui context + backends
// ============================================================
static bool InitImGui(IDXGISwapChain* pSwapChain) {
    // Get D3D11 device from swapchain
    HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
    if (FAILED(hr)) {
        LOG("[P1] InitImGui: GetDevice failed hr=0x%08X", hr);
        return false;
    }
    g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);

    // Find game window
    HWND hWnd = FindWindowA("UnityWndClass", NULL);
    if (!hWnd) {
        LOG("[P1] InitImGui: FindWindowA failed — using desktop");
        hWnd = GetDesktopWindow();
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Init Win32 backend
    if (!ImGui_ImplWin32_Init(hWnd)) {
        LOG("[P1] InitImGui: ImGui_ImplWin32_Init failed");
        ImGui::DestroyContext();
        g_pd3dDevice->Release(); g_pd3dDevice = nullptr;
        return false;
    }

    // Init DX11 backend
    if (!ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext)) {
        LOG("[P1] InitImGui: ImGui_ImplDX11_Init failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_pd3dDevice->Release(); g_pd3dDevice = nullptr;
        return false;
    }

    LOG("[P1] ImGui initialized");
    return true;
}

// ============================================================
// WndProc hook — handles ImGui input, passes through to game
// ============================================================
LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui::GetCurrentContext() != nullptr) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return 1; // ImGui consumed
    }
    return CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
}

// ============================================================
// Present hook stub (vtable index 8)
// ============================================================
HRESULT STDMETHODCALLTYPE PresentHook(
    IDXGISwapChain* pSwapChain,
    UINT SyncInterval,
    UINT Flags
) {
    // One-time ImGui init on first successful Present
    if (!g_imguiInitialized) {
        g_imguiInitialized = InitImGui(pSwapChain);
        if (g_imguiInitialized)
            LOG("[P1] First Present(SyncInterval=%u, Flags=%u)", SyncInterval, Flags);
    }

    // Draw debug overlay
    if (g_imguiInitialized && CreateMainRTV(pSwapChain)) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Overlay toggle: INSERT key falling edge
        {
            bool insertDown = ImGui::IsKeyDown(ImGuiKey_Insert);
            if (insertDown && !g_insertWasDown) {
                g_overlayVisible = !g_overlayVisible;
                LOG("[P1] Overlay %s", g_overlayVisible ? "shown" : "hidden");
            }
            g_insertWasDown = insertDown;
        }

        // Panic key: END disables every feature. Flipping `enabled` (instead of
        // MH_DisableHook) avoids unpatching a trampoline while a game thread is
        // executing inside it; each feature hook already passthroughs when off.
        {
            bool endDown = ImGui::IsKeyDown(ImGuiKey_End);
            if (endDown && !g_endWasDown && g_featuresReady) {
                for (auto* f : g_features) f->enabled = false;
                LOG("[P1] PANIC: all features disabled (END)");
            }
            g_endWasDown = endDown;
        }

        if (g_overlayVisible) {
            ImGui::Begin("EL_Native Debug");
            ImGui::Text("EL_Native v0.1");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            // --- Resolve log ---
            ImGui::SeparatorText("Resolved Methods");
            int count = GetResolveLogCount();
            if (count == 0) {
                ImGui::Text("No resolves yet");
            } else {
                static const char* labels[] = { "API", "fallback", "FAIL", "cache" };
                ImGui::Columns(3, "resolve_cols", false);
                ImGui::Text("Method"); ImGui::NextColumn();
                ImGui::Text("Ptr");    ImGui::NextColumn();
                ImGui::Text("Src");    ImGui::NextColumn();
                ImGui::Separator();
                for (int i = 0; i < count; i++) {
                    const ResolveEntry* e = GetResolveLogEntry(i);
                    if (!e) continue;
                    ImGui::Text("%s", e->name);   ImGui::NextColumn();
                    ImGui::Text("0x%llX", e->ptr); ImGui::NextColumn();
                    int src = e->source;
                    if (src >= 0 && src < 4)
                        ImGui::Text("%s", labels[src]);
                    else
                        ImGui::Text("?");
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }

            // Feature list section
            if (g_featuresReady && g_features.size() > 0) {
                ImGui::Separator();
                ImGui::Text("Features");
                for (auto* f : g_features) {
                    if (ImGui::Checkbox(f->name, &f->enabled)) {
                        ConfigSave();
                    }
                    ImGui::SameLine();
                    f->OnMenu();
                }
            }

            ImGui::End();
        }

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    // Fire feature OnUpdate each frame (features use one-shot flags internally)
    if (g_featuresReady) {
        for (auto* f : g_features) {
            f->OnUpdate();
        }
    }

    return OriginalPresent(pSwapChain, SyncInterval, Flags);
}

// ============================================================
// ResizeBuffers hook stub (vtable index 13)
// ============================================================
HRESULT STDMETHODCALLTYPE ResizeBuffersHook(
    IDXGISwapChain* pSwapChain,
    UINT BufferCount,
    UINT Width,
    UINT Height,
    DXGI_FORMAT NewFormat,
    UINT SwapChainFlags
) {
    LOG("[P1] ResizeBuffers(%ux%u, fmt=%u, count=%u)", Width, Height, NewFormat, BufferCount);

    // Backbuffer references must all be released before ResizeBuffers or it fails
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }

    if (g_imguiInitialized)
        ImGui_ImplDX11_InvalidateDeviceObjects();

    HRESULT hr = OriginalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (SUCCEEDED(hr) && g_imguiInitialized) {
        if (!ImGui_ImplDX11_CreateDeviceObjects())
            LOG("[P1] ImGui_ImplDX11_CreateDeviceObjects() failed after resize");
    }

    return hr;
}

// ============================================================
// Render_Init — acquire D3D11 vtable via temp swapchain,
// install MinHook on Present and ResizeBuffers.
//
// Called after MH_Initialize() — assumes MinHook is ready.
// Returns false on any failure (soft-fail, game keeps running).
// ============================================================
bool Render_Init() {
    LOG("[P1] Render_Init starting");

    // Find game window to associate temp swapchain
    // (Unity games register class "UnityWndClass")
    HWND hWnd = FindWindowA("UnityWndClass", NULL);
    if (!hWnd) {
        LOG("[P1] FindWindowA(UnityWndClass) failed — using desktop window");
        hWnd = GetDesktopWindow();
    }

    // Subclass game window for ImGui input passthrough
    OriginalWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
    LOG("[P1] Window subclass installed (original=0x%llX)", (uintptr_t)OriginalWndProc);

    // --- Create temp D3D11 device + swapchain to read vtable ---
    D3D_FEATURE_LEVEL featureLevel;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = 1;
    scd.BufferDesc.Height = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                    // software module
        0,                          // flags
        nullptr,                    // feature levels (null = use defaults)
        0,                          // feature level count
        D3D11_SDK_VERSION,
        &scd,
        &pSwapChain,
        &pDevice,
        &featureLevel,
        &pContext
    );

    if (FAILED(hr)) {
        LOG("[P1] D3D11CreateDeviceAndSwapChain failed: hr=0x%08X", hr);
        return false;
    }

    // Read vtable
    uintptr_t* vtable = *(uintptr_t**)pSwapChain;
    g_presentAddr = (void*)vtable[8];
    g_resizeAddr  = (void*)vtable[13];

    LOG("[P1] swapchain=0x%llX vtable=0x%llX Present=0x%llX Resize=0x%llX",
        (uintptr_t)pSwapChain, (uintptr_t)vtable,
        (uintptr_t)g_presentAddr, (uintptr_t)g_resizeAddr);

    // Release temp resources — vtable addresses stay valid
    // (COM interface vtables live in dxgi.dll, not per-instance)
    pContext->Release();
    pDevice->Release();
    pSwapChain->Release();
    LOG("[P1] Temp D3D11 resources released");

    // --- Install Present hook ---
    MH_STATUS status = MH_CreateHook(
        g_presentAddr, &PresentHook, (LPVOID*)&OriginalPresent
    );
    LOG("[P1] MH_CreateHook(Present) = %d", status);
    if (status != MH_OK) {
        LOG("[P1] FAILED: Present hook create");
        return false;
    }

    // --- Install ResizeBuffers hook ---
    status = MH_CreateHook(
        g_resizeAddr, &ResizeBuffersHook, (LPVOID*)&OriginalResizeBuffers
    );
    LOG("[P1] MH_CreateHook(ResizeBuffers) = %d", status);
    if (status != MH_OK) {
        LOG("[P1] FAILED: ResizeBuffers hook create — rolling back Present");
        MH_RemoveHook(g_presentAddr);
        return false;
    }

    // Enable both
    status = MH_EnableHook(g_presentAddr);
    LOG("[P1] MH_EnableHook(Present) = %d", status);

    status = MH_EnableHook(g_resizeAddr);
    LOG("[P1] MH_EnableHook(ResizeBuffers) = %d", status);

    LOG("[P1] Present+ResizeBuffers hooks installed successfully");
    return true;
}
