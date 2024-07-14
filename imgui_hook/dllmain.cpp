#include <stdio.h>
#include <windows.h>
#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "hook.h"
#include "tool.h"
#include "game.h"
#include <vector>
#include "render.h"


#pragma comment(lib, "d3d11.lib")
typedef HRESULT(__stdcall* PRESENT_FUNC)(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flag);
typedef LRESULT(__stdcall* WIND_FUNC)(HWND hwnd, std::uint32_t message, WPARAM wparam, LPARAM lparam);

#define BOX_COLOR 0xff0000ff
#define MENU_COLOR 0xff00ff00
#define MENU_FRONT_SIZE 30.0f
#define ENABLE_INFO "开启"
#define DISABLE_INFO "关闭"


WIND_FUNC origin_window_func = NULL;
PRESENT_FUNC g_ret_func = NULL;
LPVOID g_old_present_func = NULL;
IDXGISwapChain* g_swap_chain;
ID3D11Device* g_device;
ID3D11DeviceContext* g_context;
ID3D11RenderTargetView* g_render_target;
BOOLEAN has_init_d3d = FALSE;


LPVOID g_module_client_addr = NULL;
LPVOID g_module_steam_render_addr = NULL;


UINT32 g_frame_count = 0;

extern std::vector<PLAYER_INFO> g_player_list;
VEC2 g_screen_solu;


bool enable_skeleton = TRUE;
bool enable_esp = TRUE;
bool enable_aim1 = FALSE;
bool enable_menu = TRUE;

float g_aim_radius = 40.0f;

int g_bone_count = 30;
VEC2 g_bone_buffer[60];

VEC2 g_body_line[] = {
    {head,neck},
    {neck,spine},
    {spine, cock},
    {spine, left_shoulder},
    {left_shoulder,left_arm},
    {left_arm,left_hand},
    {spine,right_shoulder},
    {right_shoulder,right_arm},
    {right_arm,right_hand},
    {spine,spine_1},
    {cock,left_hip},
    {left_hip,left_knee},
    {left_knee,left_feet},
    {cock,right_hip},
    {right_hip,right_knee},
    {right_knee,right_feet}
};

const char* g_body_name[] = {
    u8"头",
    u8"胸",
    u8"腰"
};
body_node g_target_body[] = {
    head,
    spine,
    cock,
};
UINT32 g_select_body = 0;
int g_body_line_count = sizeof(g_body_line) / sizeof(VEC2);


LRESULT __stdcall my_window_func(HWND hwnd, std::uint32_t message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_MOUSEWHEEL) {
        int wheelDelta = GET_WHEEL_DELTA_WPARAM(wparam);
        if (wheelDelta > 0) {
            enable_aim1 = TRUE;
        }
        else if(wheelDelta<0) {
            enable_aim1 = FALSE;
        }
    }
    if (message == WM_KEYDOWN) {
        switch (LOWORD(wparam))
        {
    
        case VK_F5: {
            enable_esp = !enable_esp;
            break;
        }
        case VK_F6: {
            enable_skeleton =! enable_skeleton;
            break;
        }
        case VK_F7: {
            enable_menu = !enable_menu;
            break;
        }
        case VK_UP: {
            g_aim_radius++;
            if (g_aim_radius > g_screen_solu.y / 2) {
                g_aim_radius = g_screen_solu.y / 2;
            }
            break;
        }
        case VK_DOWN: {
            g_aim_radius--;
            if (g_aim_radius <5) {
                g_aim_radius = 5;
            }
            break;
        }
        case VK_LEFT: {
            if (g_select_body == 0) {
                g_select_body = 2;
            }
            else {
                g_select_body--;
            }
            break;
        }
        case VK_RIGHT: {
            if (g_select_body == 2) {
                g_select_body = 0;
            }
            else {
                g_select_body++;
            }
            
            break;
            
        }
        default:
            break;
        }
    }
     
    return CallWindowProc(origin_window_func, hwnd, message, wparam, lparam);
}


BOOLEAN init_d3d(IDXGISwapChain* swap) {
    g_swap_chain = swap;
    if (g_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device)>0) {
        return FALSE;
    }
    g_device->GetImmediateContext(&g_context);

    DXGI_SWAP_CHAIN_DESC swap_desc;
    g_swap_chain->GetDesc(&swap_desc);
    HWND wind = swap_desc.OutputWindow;
    origin_window_func= (WIND_FUNC)SetWindowLongPtr(wind, GWLP_WNDPROC, (LONG_PTR)my_window_func);
    g_screen_solu.x = swap_desc.BufferDesc.Width;
    g_screen_solu.y = swap_desc.BufferDesc.Height;
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("c:/windows/Fonts/simhei.ttf", MENU_FRONT_SIZE, NULL, io.Fonts->GetGlyphRangesChineseFull());
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    ID3D11Texture2D* swap_buffer=NULL;
    g_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)& swap_buffer);
    if (!swap_buffer) {
        return FALSE;
    }
    g_device->CreateRenderTargetView(swap_buffer, NULL, &g_render_target);
    swap_buffer->Release();
    ImGui_ImplDX11_Init(g_device, g_context);
    ImGui_ImplWin32_Init((VOID*)swap_desc.OutputWindow);
    return TRUE;
}
BOOLEAN release_d3d() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_render_target->Release();
    return TRUE;
}

void render_menu() {
    
    
    render_text(VEC2{ 100,300 }, MENU_COLOR, 30.0f, "透视F5：%s", enable_esp ? ENABLE_INFO : DISABLE_INFO);
    render_text(VEC2{ 100,350 }, MENU_COLOR, 30.0f, "骨骼F6：%s", enable_skeleton ? ENABLE_INFO : DISABLE_INFO);
    render_text(VEC2{ 100,400 }, MENU_COLOR, 30.0f, "自瞄鼠标滚轮上下滑动：%s", enable_aim1 ? ENABLE_INFO : DISABLE_INFO);
    render_text(VEC2{ 100,450 }, MENU_COLOR, 30.0f, "菜单F7");
    if (enable_aim1) {
        render_text(VEC2{ 100,500 }, MENU_COLOR, 30.0f, u8"自瞄范围(上下键)：%d", (int)g_aim_radius);
        render_text(VEC2{ 100,550 }, MENU_COLOR, 30.0f, u8"自瞄部位(左右键)：%s", g_body_name[g_select_body]);
        render_circle(VEC2{ g_screen_solu.x / 2, g_screen_solu.y / 2 }, g_aim_radius, MENU_COLOR, 1.0f);
    }
}

void render_my_tool() {
    if (enable_menu) {
        render_menu();
    }
    refresh_data();
    UINT_PTR local = get_local_player();
    
    if (!local || !is_player_alive(local)) {
        return;
    }
   
    
    if (enable_aim1) {
      
       set_new_oritation(g_aim_radius,g_target_body[g_select_body]);
    }
    if (enable_esp) {
        PLAYER_BOX box;
        for (auto x : g_player_list) {
            
            if (x.is_enemy && x.is_alive) {
                if (get_player_box(x.player_addr, &box)) {
                    render_box(box.xmin, box.ymin, box.xmax, box.ymax, BOX_COLOR);
                }
            }
        }
    }

    if (enable_skeleton) {
        for (auto x : g_player_list) {
            if (x.is_enemy && x.is_alive) {

                if (get_player_skeleton(x.player_addr, g_bone_buffer, g_bone_count)) {
                    for (int i = 0; i < g_body_line_count; i++) {
                        VEC2 start = g_bone_buffer[g_body_line[i].x];

                        VEC2 end = g_bone_buffer[g_body_line[i].y];
     
                        render_line(start, end, BOX_COLOR);
                    }
                }
            }
            
            
        }
    }
    
}




void render_frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
   
   
    render_my_tool();
    

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
    return g_ret_func(swap_chain, sync_interval, flag);
}

BOOLEAN init() {
    while (!g_module_steam_render_addr) {
        g_module_steam_render_addr = get_module_base_addr(L"gameoverlayrenderer64.dll");
    }
    while (!g_module_client_addr) {
        g_module_client_addr = get_module_base_addr(L"client.dll");
    }
  
    g_old_present_func = *(LPVOID*)((UINT_PTR)g_module_steam_render_addr + OFFSET_GAME_RENDER_OFFSET);
    if (!g_module_client_addr || !g_module_steam_render_addr) {
        return FALSE;
    }
    init_game_addr();
    return TRUE;
}

DWORD WINAPI on_attach(LPVOID p) {
    
    if (!init()) {
        Log(L"init fail");
        return -1;
    }
    Log(L"init success");
  
    g_ret_func = (PRESENT_FUNC)hook_func( g_old_present_func, my_present);
    if (!g_ret_func) {
        Log(L"set present hook fail");
    }
    Log(L"set present hook success");
    return 0;
}

void on_detach() {
    if (has_init_d3d) release_d3d();
    reset_func(g_old_present_func);
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {

        CreateThread(NULL, 1024 * 1024, on_attach, NULL, NULL, NULL);
        
        break;
    }
    case DLL_THREAD_ATTACH: 
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH: {
        break;
    }
    }
    return TRUE;
}

