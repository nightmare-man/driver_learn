#include "game.h"
#include "tool.h"

extern LPVOID g_module_client_addr;
LPVOID get_interface(LPCWSTR module_name, LPCSTR interface_name) {
	HMODULE target_module = GetModuleHandleW(module_name);
	if (!target_module) return NULL;
	LPVOID func_addr = GetProcAddress(target_module, "CreateInterface");

	Log(L"func addr is 0x%p\n", func_addr);

	typedef LPVOID(*gen_object_fn)();
	typedef struct _interface_register_node {
		gen_object_fn gen_object;//Õâ¸öcallbackµÄ
		const char* interface_name;
		struct _interface_register_node* next;
	} interface_register_node;
	interface_register_node* node = *(interface_register_node**)resolve_addr_in_offset_mem_instruction(func_addr, 7, 3);

	LPVOID ret = NULL;
	while (1) {
		LogA("node [%s,0x%p] \n", node->interface_name, node);
		if (!node) {
			break;
		}
		if (strcmp(node->interface_name, interface_name) == 0) {
			ret = node->gen_object();
			break;
		}
		node = node->next;

	}
	return ret;
}

BOOLEAN get_entity_list( LPVOID* list_buffer, UINT32 buffer_size, UINT32* readed_size) {
	if (!g_module_client_addr) return FALSE;
	if (!list_buffer || !buffer_size) return FALSE;
	UINT32 idx = 0;
	while (1) {
		UINT_PTR target_addr = (UINT_PTR)g_module_client_addr + OFFSET_CLIENT_ENTIRY_LIST + idx * sizeof(LPVOID);
		LPVOID target_value = *(LPVOID*)target_addr;
		if (target_value == NULL) {
			*readed_size = idx * sizeof(LPVOID);
			break;
		}
		UINT_PTR write_addr = (UINT_PTR)list_buffer + idx * sizeof(LPVOID);
		idx++;
		if (idx * sizeof(LPVOID) > buffer_size) {
			return FALSE;
		}
		*(LPVOID*)write_addr = target_value;
	}
	return TRUE;
}

BOOLEAN is_player_alive(LPVOID player) {
	return (*(UINT16*)((UINT_PTR)player + OFFSET_ENTITY_HEALTH)) > 0;
}


void world_to_screen(float world_position[3], float* vp_matrix, int screen_resolution[2], int* screen_x, int* screen_y) {
	// Homogeneous coordinates transformation
	float pos_homogeneous[4] = { world_position[0], world_position[1], world_position[2], 1.0f };

	// Apply view matrix (assuming view matrix is already in homogeneous coordinates)
	float view_position[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			view_position[i] += vp_matrix[i*4+j] * pos_homogeneous[j];
		}
	}

	// Perspective divide
	float clip_space_position[3];
	for (int i = 0; i < 3; ++i) {
		clip_space_position[i] = view_position[i] / view_position[3];
	}

	// Convert to screen coordinates
	*screen_x = (int)((clip_space_position[0] + 1) * 0.5f * screen_resolution[0]);
	*screen_y = (int)((1 - clip_space_position[1]) * 0.5f * screen_resolution[1]);
}

VOID get_player_box(int* screen_solu, LPVOID player, int* x_min, int* y_min, int* x_max, int* y_max) {
	UINT_PTR game_sence_node_addr = *(UINT_PTR*)((UINT_PTR)player + OFFSET_ENTITY_POS_LEVEL1);
	FLOAT* player_pos_addr =(FLOAT*) (game_sence_node_addr + OFFSET_ENTITY_POS_LEVEL2);
	UINT_PTR player_collision_addr = *(UINT_PTR*)((UINT_PTR)player + OFFSET_ENTITY_COLLISION_LEVEL1);
	FLOAT* player_collision_max = (FLOAT*)(player_collision_addr + OFFSET_ENTITY_COLLISION_MAX_LEVEL2);
	FLOAT* player_collision_min = (FLOAT*)(player_collision_addr + OFFSET_ENTITY_COLLISION_MIN_LEVEL2);
	FLOAT* vp_matrix = (FLOAT*)((UINT_PTR)g_module_client_addr + OFFSET_VIEW_MATRIX);
	*x_min = 88888;
	*y_min = 88888;
	*x_max = -88888;
	*y_max = -88888;
	for (UINT32 i = 0; i < 8; i++) {
		float tmp[3] = { 0 };
		for (UINT32 j = 0; j < 3; j++) {
			if (i & (01ULL << j)) {
				tmp[j] = player_pos_addr[j] + player_collision_max[j];
			}
			else {
				tmp[j] = player_pos_addr[j] + player_collision_min[j];
			}
		}
		int tmp_x = 0;
		int tmp_y = 0;
		world_to_screen(tmp, vp_matrix, screen_solu, &tmp_x, &tmp_y);
		if (tmp_x < *x_min) *x_min = tmp_x;
		if (tmp_x > *x_max) *x_max = tmp_x;
		if (tmp_y < *y_min) *y_min = tmp_y;
		if (tmp_y > *y_max) *y_max = tmp_y;
	}
}
