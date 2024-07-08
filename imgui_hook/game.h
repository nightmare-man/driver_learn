#pragma once
#include <windows.h>
#define OFFSET_CLIENT_ENTIRY_LIST 0x1831540 
#define OFFSET_ENTITY_HEALTH 804
#define OFFSET_ENTITY_POS_LEVEL1 776
#define OFFSET_ENTITY_POS_LEVEL2 208
#define OFFSET_ENTITY_COLLISION_LEVEL1 792
#define OFFSET_ENTITY_COLLISION_MAX_LEVEL2 76
#define OFFSET_ENTITY_COLLISION_MIN_LEVEL2 64
#define OFFSET_ENTITY_TEAMNUM 963
#define OFFSET_VIEW_MATRIX 0x1A1FCD0
LPVOID get_interface(LPCWSTR module_name, LPCSTR interface_name);
BOOLEAN get_entity_list( LPVOID* list_buffer, UINT32 buffer_size, UINT32* readed_size);
BOOLEAN is_player_alive(LPVOID player);
VOID get_player_box(int* screen_solu, LPVOID player, int* x_min, int* y_min, int* x_max, int* y_max);