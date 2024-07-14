#include "game.h"
#include "tool.h"
#include <stdio.h>
#include <math.h>

#define TARGET_ENTITY_NAME "c_cs_player_for_precache"
#define VIEW_MODEL_NAME "csgo_viewmodel"
#define MY_PI 3.1415926535897932
static LPVOID g_module_client_addr=NULL;
std::vector<PLAYER_INFO> g_player_list;
UINT_PTR g_local_player;
UINT_PTR g_handle_list_addr;
UINT_PTR g_input_object;
FLOAT* g_view_matrix = NULL;

typedef UINT_PTR(*FUNC_GET_ENTITY_BY_ID)(UINT_PTR,UINT32);
typedef VOID(*FUNC_SET_ORITATION)(UINT_PTR, INT32 zero, INT64*);

extern VEC2 g_screen_solu;


FUNC_GET_ENTITY_BY_ID g_get_entity_by_id;
FUNC_SET_ORITATION g_set_oritation;
UINT_PTR g_view_model = 0;

VOID init_game_addr() {
	while (!g_module_client_addr) {
		g_module_client_addr = get_module_base_addr(L"client.dll");
	}

	UINT_PTR tmp1 = pattern_scan(L"client.dll", PATTERN_FIND_HANDLE_LIST);
	LogA("tmp1 0x%p", tmp1);
	UINT_PTR tmp2 = resolve_addr_in_offset_mem_instruction(tmp1 + 6, 7, 3);
	LogA("tmp2 0x%p", tmp2);
	g_handle_list_addr = *(UINT_PTR*) tmp2;
	LogA("g_handle_list_addr 0x%p", g_handle_list_addr);


	g_get_entity_by_id = (FUNC_GET_ENTITY_BY_ID)pattern_scan(L"client.dll", PATTERN_GET_ENTITY_BY_ID);
	g_set_oritation = (FUNC_SET_ORITATION)pattern_scan(L"client.dll", PATTERN_SET_ORITATION);
	g_input_object =((UINT_PTR)g_module_client_addr + OFFSET_INPUT_OBJECT);
	LogA("input obejct is 0x%p", g_input_object);
	g_local_player = *(UINT_PTR*)((UINT_PTR)g_module_client_addr + OFFSET_LOCAL_PLAYER);
	
}

BOOLEAN is_in_game() {
	return g_local_player != 0;
}

UINT_PTR get_entity_by_idx(UINT32 idx) {
	return g_get_entity_by_id(g_handle_list_addr, idx);
}
UINT_PTR player_to_controller(UINT_PTR player) {
	if (!player) return NULL;
	UINT32 handle = *(UINT32*)(player + 5208);
	if (!handle) return NULL;
	handle = handle & 0x7fff;
	return get_entity_by_idx(handle);
}

UINT_PTR controller_to_player(UINT_PTR controller) {
	if (!controller) return NULL;
	UINT32 handle = 0xffff;
	UINT_PTR ret = NULL;
	__try {
		handle = *(UINT32*)(controller + 1532);
		handle = handle & 0x7fff;
		ret=get_entity_by_idx(handle);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		
		ret = NULL;
	}
	
	return  NULL;
}

BOOLEAN is_local_player(UINT_PTR player) {
	if (!player) return FALSE;
	return player == g_local_player;
}

BOOLEAN is_player_alive(UINT_PTR player) {
	if (!player) return FALSE;
	BOOLEAN ret = FALSE;
	__try {
		ret = (*(UINT16*)(player + OFFSET_ENTITY_HEALTH)) > 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		ret = FALSE;
	}
	return ret;
}

SHORT get_player_hp(UINT_PTR player) {
	if (!player) return 0;
	USHORT ret = 0;
	__try {
		ret = *(SHORT*)(player + OFFSET_ENTITY_HEALTH);
	}
	__except(EXCEPTION_EXECUTE_HANDLER){
		ret = 0;
	}
	return ret;
}


UINT_PTR get_local_player() {
	return g_local_player;
}

VOID get_player_pos(UINT_PTR player, VEC3f* vec) {
	if (!player) return;

	__try {
		UINT_PTR game_sence_node_addr = *(UINT_PTR*)(player + OFFSET_ENTITY_SCENE_NODE);
		
		FLOAT* player_pos_addr = (FLOAT*)(game_sence_node_addr + OFFSET_SCENE_NODE_POS);
	
		memcpy_s((void*)vec, sizeof(VEC3f), player_pos_addr, sizeof(VEC3f));
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return;
	}
	
}


const char* get_player_name(UINT_PTR player) {
	if (!player) return NULL;
	const char* ret = NULL;
	__try {
		UINT_PTR controller = player_to_controller(player);
		const char* ret = *(const char**)(controller + OFFSET_ENTITY_NAME);
	}

	__except (EXCEPTION_EXECUTE_HANDLER) {
		ret = NULL;
	}
	
	return ret;
}
CHAR get_player_team(UINT_PTR player) {

	if (!player) return NULL;
	CHAR ret = 0;
	
	__try {
		ret = *(CHAR*)(player + OFFSET_ENTITY_TEAMNUM);
	}

	__except (EXCEPTION_EXECUTE_HANDLER) {
		ret = 0;
	}

	return ret;
}
BOOLEAN is_enemy(UINT_PTR player) {
	if (!player) return FALSE;
	CHAR my = get_player_team(g_local_player);
	return my != get_player_team(player);
}
VOID set_player_info(UINT_PTR player, PLAYER_INFO* info) {
	if (!player) return;
	info->player_addr = player;
	info->hp = get_player_hp(player);
	info->is_alive = is_player_alive(player);
	info->is_enemy = is_enemy(player);
	info->is_self = player == g_local_player;
}

BOOLEAN refresh_data() {
	g_local_player = *(UINT_PTR*)((UINT_PTR)g_module_client_addr + OFFSET_LOCAL_PLAYER);
	g_view_matrix = (FLOAT*)((UINT_PTR)g_module_client_addr + OFFSET_VIEW_MATRIX);
	g_player_list.clear();
	UINT_PTR entity_system_object = *(UINT_PTR*)((UINT_PTR)g_module_client_addr + OFFSET_ENTITY_SYSTEM);
	UINT_PTR entity = *(UINT_PTR*)(entity_system_object + 528);
	while (entity) {
		UINT_PTR player = *(UINT_PTR*)(entity);
		UINT_PTR entity_identity =*(UINT_PTR*)( player + 16);
		const char* entity_name = *(const char**)(entity_identity + 32);
		if (entity_name && strncmp(entity_name,TARGET_ENTITY_NAME,40)==0) {
			PLAYER_INFO info;
			set_player_info(player, &info);
			g_player_list.push_back(info);
		}
		if (entity_name && strncmp(entity_name, VIEW_MODEL_NAME, 25) == 0) {
			g_view_model = player;
		}
		entity = *(UINT_PTR*)(entity + 96);
	}
	
	return TRUE;
}


void camera_world_position(float view_matrix[4][4], VEC3f* vec) {
	// Calculate inverse of view matrix to get camera position in world coordinates
	float det = 0.0;
	float inv[4][4];

	// Calculate determinant using first row elements
	det = view_matrix[0][0] * (view_matrix[1][1] * view_matrix[2][2] - view_matrix[1][2] * view_matrix[2][1])
		- view_matrix[0][1] * (view_matrix[1][0] * view_matrix[2][2] - view_matrix[1][2] * view_matrix[2][0])
		+ view_matrix[0][2] * (view_matrix[1][0] * view_matrix[2][1] - view_matrix[1][1] * view_matrix[2][0]);

	// Calculate inverse matrix
	inv[0][0] = (view_matrix[1][1] * view_matrix[2][2] - view_matrix[1][2] * view_matrix[2][1]) / det;
	inv[0][1] = (view_matrix[0][2] * view_matrix[2][1] - view_matrix[0][1] * view_matrix[2][2]) / det;
	inv[0][2] = (view_matrix[0][1] * view_matrix[1][2] - view_matrix[0][2] * view_matrix[1][1]) / det;
	inv[0][3] = 0;
	inv[1][0] = (view_matrix[1][2] * view_matrix[2][0] - view_matrix[1][0] * view_matrix[2][2]) / det;
	inv[1][1] = (view_matrix[0][0] * view_matrix[2][2] - view_matrix[0][2] * view_matrix[2][0]) / det;
	inv[1][2] = (view_matrix[0][2] * view_matrix[1][0] - view_matrix[0][0] * view_matrix[1][2]) / det;
	inv[1][3] = 0;
	inv[2][0] = (view_matrix[1][0] * view_matrix[2][1] - view_matrix[1][1] * view_matrix[2][0]) / det;
	inv[2][1] = (view_matrix[0][1] * view_matrix[2][0] - view_matrix[0][0] * view_matrix[2][1]) / det;
	inv[2][2] = (view_matrix[0][0] * view_matrix[1][1] - view_matrix[0][1] * view_matrix[1][0]) / det;
	inv[2][3] = 0;
	inv[3][0] = 0;
	inv[3][1] = 0;
	inv[3][2] = 0;
	inv[3][3] = 1;

	// Calculate camera world position
	vec->x = -(inv[0][0] * view_matrix[0][3] + inv[0][1] * view_matrix[1][3] + inv[0][2] * view_matrix[2][3]);
	vec->y = -(inv[1][0] * view_matrix[0][3] + inv[1][1] * view_matrix[1][3] + inv[1][2] * view_matrix[2][3]);
	vec->z = -(inv[2][0] * view_matrix[0][3] + inv[2][1] * view_matrix[1][3] + inv[2][2] * view_matrix[2][3]);
}
void get_cam_pos(VEC3f* vec) {
	float(*ptr)[4] = (float(*)[4])g_view_matrix;
	camera_world_position(ptr, vec);
}

BOOLEAN world_to_screen(float world_position[3], VEC2* cord) {
	// Homogeneous coordinates transformation
	float pos_homogeneous[4] = { world_position[0], world_position[1], world_position[2], 1.0f };

	// Apply view matrix (assuming view matrix is already in homogeneous coordinates)
	float view_position[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			view_position[i] += g_view_matrix[i*4+j] * pos_homogeneous[j];
		}
	}
	
	// Perspective divide
	float clip_space_position[3];
	for (int i = 0; i < 3; ++i) {
		clip_space_position[i] = view_position[i] / view_position[3];
		if (clip_space_position[i] > 1.0f) {
			clip_space_position[i] = 1.0f;
		}
		else if (clip_space_position[i] < -1.0f) {
			clip_space_position[i] = -1.0f;
		}
	}
	if (clip_space_position[2] <= -1.0 || clip_space_position[2] >= 1.0f) {
		return FALSE;
	}
	cord->x = (int)((clip_space_position[0] + 1) * 0.5f * g_screen_solu.x);
	
	cord->y = (int)((1 - clip_space_position[1]) * 0.5f * g_screen_solu.y);
	
	return TRUE;
}

VOID get_player_body_world_pos(UINT_PTR  player, VEC3f* world_pos, int node) {
	if (!player) return;
	if (node > 128) return;
	__try {
		UINT_PTR game_sence_node_addr = *(UINT_PTR*)(player + OFFSET_ENTITY_SCENE_NODE);
	

		FLOAT* bone_array = *(FLOAT**)(game_sence_node_addr + OFFSET_SCENE_NODE_SKELETON);
	
		UINT_PTR target_pone = (UINT_PTR)bone_array + node * 0x20;

		memcpy_s(world_pos, sizeof(VEC3f), (VOID*)target_pone, sizeof(VEC3f));
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		ZeroMemory(world_pos, sizeof(VEC3f));
	}
	
}
VOID get_player_body_screen_pos(UINT_PTR player, VEC2* screen_pos, int node) {


	VEC3f tmp_world_pos = { 0 };
	get_player_body_world_pos(player, &tmp_world_pos, node);
	world_to_screen((float*)&tmp_world_pos, screen_pos);
}
BOOLEAN get_player_skeleton( UINT_PTR player, VEC2* buffer, int max_bone) {
	VEC3f tmp_world_pos = { 0 };
	for (UINT32 i = 0; i < max_bone; i++) {
		get_player_body_screen_pos(player, buffer + i, i);
	}
	return TRUE;
}





// 从视图矩阵中提取摄像机位置
void extract_camera_position( VEC3f* vec3) {
	// 视图矩阵的前三列表示摄像机坐标系的三个轴的方向
	// 第四列表示平移向量 (负的摄像机位置)
	// 提取摄像机位置
	vec3->x = -(g_view_matrix[0] * g_view_matrix[12] + g_view_matrix[1] * g_view_matrix[13] + g_view_matrix[2] * g_view_matrix[14]);
	vec3->y= -(g_view_matrix[4] * g_view_matrix[12] + g_view_matrix[5] * g_view_matrix[13] + g_view_matrix[6] * g_view_matrix[14]);
	vec3->z= -(g_view_matrix[8] * g_view_matrix[12] + g_view_matrix[9] * g_view_matrix[13] + g_view_matrix[10] * g_view_matrix[14]);

}

VEC3f sub_vec(VEC3f a, VEC3f b){
	return VEC3f{ a.x - b.x,a.y - b.y,a.z - b.z };
}
VEC3f normalize_vec(VEC3f a) {
	float len= sqrtf(powf(a.x,2) + powf(a.y,2) + powf(a.z,2));
	return VEC3f{ a.x / len, a.y / len,a.z / len };
}
// 计算新的欧拉角
void cac_new_oritation_internal(VEC3f cam_pos, VEC3f target_pos, VEC2f* oritation) {
	VEC3f direction = normalize_vec(sub_vec(target_pos, cam_pos));
	//x 是pitch 俯仰
	oritation->x = -atan2f(direction.z, sqrtf(direction.x * direction.x + direction.y * direction.y));
	//y是 yaw 自身旋转
	oritation->y = atan2f(direction.y, direction.x);
	// Calculate the yaw (rotation around y-axis)

	oritation->y = oritation->y * 180 / MY_PI;
	// 将弧度转换为角度
	oritation->x = oritation->x * 180 / MY_PI;
}


VOID cac_new_oritation(VEC3f target_pos, VEC2f* oritation) {
	VEC3f cam_pos = { 0 };
	get_cam_pos(&cam_pos);
	cac_new_oritation_internal(cam_pos, target_pos, oritation);
}
VOID set_view_angle(VEC3f* vec3) {
	g_set_oritation(g_input_object, 0, (INT64*)vec3);
}
void set_new_oritation(float max_dist, body_node node) {
	float nearest_dist = 9999.0f;
	VEC3f nearest_pos = { 0 };
	UINT_PTR nearest_player = NULL;
	for (const PLAYER_INFO x : g_player_list) {
		if (x.is_alive && x.is_enemy) {
			VEC2 tmp_screen_pos = { 0 };
			get_player_body_screen_pos(x.player_addr, &tmp_screen_pos, node);
			float dist = sqrtf(powf(tmp_screen_pos.x - g_screen_solu.x/2, 2) + powf(tmp_screen_pos.y - g_screen_solu.y/2, 2));
			if (dist < max_dist && dist < nearest_dist) {
				nearest_dist = dist;
				nearest_player = x.player_addr;
				
			}
		}
	}
	if (nearest_player) {
		VEC3f tmp_world_pos = { 0 };
		VEC3f new_oritation = { 0 };
		get_player_body_world_pos(nearest_player, &tmp_world_pos, node);
		
		cac_new_oritation(tmp_world_pos, (VEC2f*) ( & new_oritation));
		set_view_angle(&new_oritation);
	}
}

BOOLEAN get_player_box( UINT_PTR player, PLAYER_BOX* box) {
	if (!player) return FALSE;
	box->xmin = 9999;
	box->ymin = 9999;
	box->xmax = -9999;
	box->ymax = -9999;
	BOOLEAN ret = FALSE;
	__try {
		UINT_PTR game_sence_node_addr = *(UINT_PTR*)(player + OFFSET_ENTITY_SCENE_NODE);
	
		FLOAT* player_pos_addr = (FLOAT*)(game_sence_node_addr + OFFSET_SCENE_NODE_POS);
		
		UINT_PTR player_collision_addr = *(UINT_PTR*)((UINT_PTR)player + OFFSET_ENTITY_COLLISION);
	
		FLOAT* player_collision_max = (FLOAT*)(player_collision_addr + OFFSET_COLLISION_POS_MAX);
		
		FLOAT* player_collision_min = (FLOAT*)(player_collision_addr + OFFSET_COLLISION_POS_MIN);
		



		
		for (UINT32 i = 0; i < 8; i++) {
			float world_cord[3] = { 0 };
			for (UINT32 j = 0; j < 3; j++) {
				if (i & (01ULL << j)) {
					world_cord[j] = player_pos_addr[j] + player_collision_max[j];
				}
				else {
					world_cord[j] = player_pos_addr[j] + player_collision_min[j];
				}
			}
			VEC2 tmp = { 0 };
			if (!world_to_screen(world_cord, &tmp)) {
				return FALSE;
			}
			if (tmp.x < box->xmin) box->xmin = tmp.x;
			if (tmp.x > box->xmax) box->xmax = tmp.x;
			if (tmp.y < box->ymin) box->ymin = tmp.y;
			if (tmp.y > box->ymax) box->ymax = tmp.y;
		}
		ret = TRUE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		ret = FALSE;
	}
	if (box->xmin + box->ymin + box->xmax + box->ymax == 0) {
		ret = FALSE;
	}
	return ret;
}
