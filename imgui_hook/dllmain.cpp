#include <stdio.h>
#include <windows.h>
#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "hook.h"
#pragma comment(lib, "d3d11.lib")
typedef HRESULT(__stdcall* PRESENT_FUNC)(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flag);

PRESENT_FUNC ret_func = NULL;
BOOLEAN has_init_d3d = FALSE;

IDXGISwapChain* g_swap_chain;
ID3D11Device* g_device;
ID3D11DeviceContext* g_context;
DXGI_SWAP_CHAIN_DESC g_swap_desc;
ID3D11RenderTargetView* g_render_target;
BOOLEAN init_d3d(IDXGISwapChain* swap) {
    g_swap_chain = swap;
    if (g_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device)>0) {
        return FALSE;
    }
    g_device->GetImmediateContext(&g_context);
    g_swap_chain->GetDesc(&g_swap_desc);
    ImGui::CreateContext();
    ID3D11Texture2D* swap_buffer=NULL;
    g_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)& swap_buffer);
    if (!swap_buffer) {
        return FALSE;
    }
    g_device->CreateRenderTargetView(swap_buffer, NULL, &g_render_target);
    swap_buffer->Release();
    ImGui_ImplDX11_Init(g_device, g_context);
    ImGui_ImplWin32_Init((VOID*)g_swap_desc.OutputWindow);
    has_init_d3d = TRUE;
    return TRUE;
}
void render_frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    ImGui::Text("nihao");
  
    ImGui::Render();
    g_context->OMSetRenderTargets(1, &g_render_target, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
HRESULT __stdcall my_present(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flag) {
    if (!has_init_d3d) {
        if (!init_d3d(swap_chain)) {
            OutputDebugString(L"init d3d fail\n");
        }
        else {
            L"init d3d success\n";
        }
    }
    render_frame();
    return ret_func(swap_chain, sync_interval, flag);
}
DWORD WINAPI thread_start(LPVOID param) {
    OutputDebugString(L"dll thread start\n");

    DXGI_SWAP_CHAIN_DESC scd = {};
    ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 1;//1个后台缓冲
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.Width = 100;
    scd.BufferDesc.Height = 100;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = GetForegroundWindow();
    scd.SampleDesc.Count = 4;
    scd.Windowed = TRUE;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    ID3D11Device* tmp_device=NULL;
    ID3D11DeviceContext* tmp_context=NULL;
    IDXGISwapChain* tmp_swap_chain=NULL;
    D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        NULL,
        NULL,
        NULL,
        D3D11_SDK_VERSION,
        &scd,
        &tmp_swap_chain,
        &tmp_device,
        NULL,
        &tmp_context
    );
    if (!tmp_device) {
        OutputDebugString(L"create device fail\n");
    }
    LPVOID virtual_table_addr = *((LPVOID*)tmp_swap_chain);
    LPVOID old_present_func = *(LPVOID*)((UINT_PTR)virtual_table_addr + 64);
    WCHAR buffer[30] = {0};
    swprintf_s(buffer, 30, L"swap 0x%p\n", old_present_func);
    OutputDebugString(buffer);
    tmp_device->Release();
    tmp_context->Release();
    tmp_swap_chain->Release();

    ret_func = (PRESENT_FUNC)hook_func(old_present_func, my_present);
    if (ret_func == NULL) {
        OutputDebugString(L"hook fail\n");
    }
    else {
        OutputDebugString(L"hook success\n");
    }
    OutputDebugString(L"dll thread end\n");
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        CreateThread(NULL, 4096, thread_start, NULL, NULL, NULL);
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

