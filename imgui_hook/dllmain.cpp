#include <stdio.h>
#include <windows.h>
#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "hook.h"
#include "tool.h"


#pragma comment(lib, "d3d11.lib")
typedef HRESULT(__stdcall* PRESENT_FUNC)(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flag);

PRESENT_FUNC ret_func = NULL;
BOOLEAN has_init_d3d = FALSE;

IDXGISwapChain* g_swap_chain;
ID3D11Device* g_device;
ID3D11DeviceContext* g_context;
DXGI_SWAP_CHAIN_DESC g_swap_desc;
ID3D11RenderTargetView* g_render_target;

LPCVOID g_module_client_addr = NULL;
LPCVOID g_module_steam_render_addr = NULL;


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
        init_d3d(swap_chain);
        has_init_d3d = TRUE;
    }
    render_frame();
    return ret_func(swap_chain, sync_interval, flag);
}

BOOLEAN set_module_addr() {
    g_module_steam_render_addr = get_module_base_addr(L"gameoverlayrenderer64.dll");
    g_module_client_addr = get_module_base_addr(L"client.dll");
    if (!g_module_client_addr || !g_module_steam_render_addr) {
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI thread_start(LPVOID param) {

    OutputDebugString(L"dll thread start\n");
    
    if (!set_module_addr()) return -1;
    LPVOID old_present_func = *(LPVOID*)((UINT_PTR)g_module_steam_render_addr + 0x149be0);
    WCHAR buffer[40];
    swprintf_s(buffer, 40, L"present func is 0x%p\n", old_present_func);
    suspend_or_resume_other_threads(TRUE);
    ret_func = (PRESENT_FUNC)hook_func(old_present_func, my_present);
    suspend_or_resume_other_threads(FALSE);
   
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
        
        CreateThread(NULL, 2*1024*1024, thread_start, NULL, NULL, NULL);
        
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

