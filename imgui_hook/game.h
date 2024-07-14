#pragma once
#include <windows.h>
#include <vector>
#include "tool.h"
#define OFFSET_ENTITY_HEALTH 804
#define OFFSET_ENTITY_SCENE_NODE 776
#define OFFSET_ENTITY_TEAMNUM 963
#define OFFSET_ENTITY_NAME 1856

#define OFFSET_SCENE_NODE_POS 208
#define OFFSET_ENTITY_COLLISION 792
#define OFFSET_COLLISION_POS_MAX 76
#define OFFSET_COLLISION_POS_MIN 64
#define OFFSET_SCENE_NODE_SKELETON (0x170+0x80)


#define OFFSET_GAME_RENDER_OFFSET 0x149be0
#define OFFSET_VIEW_MATRIX 27393200
#define OFFSET_LOCAL_PLAYER 25311752
#define OFFSET_INPUT_OBJECT  27426320
#define OFFSET_ENTITY_SYSTEM 28167064
#define OFFSET_VIEW_ANGLE 27447848
//#define OFFSET_HANDLE_RESOLVE 0x1a12f10

//#define OFFSET_INPUT_FUNC 0x76e1c0


#define PATTERN_FIND_HANDLE_LIST "40 57 48 83 EC 30 48 8B ?? ?? ?? ?? ?? 49 8B F8 48 85 C9"
#define PATTERN_FIND_NEXT_ENTITY_FUNC "48 85 D2 74 0A 48 8B 42 10 48 8B 40 60 EB 07 48 8B 81 10 02 00 00"
#define PATTERN_GET_ENTITY_BY_ID "81 FA FE 7F 00 00 77 36 8B C2 C1 F8 09 83 F8 3F 77 2C 48 98 48 8B 4C C1 10 48 85 C9"
#define PATTERN_SET_ORITATION "85 D2 75 3F 48 63 81 B0 54 00 00 F2 41 0F 10 00 85 C0 74 1D"



//controller��������
//����player���������Ѫ��



typedef struct _player_box {
	int xmin;
	int ymin;
	int xmax;
	int ymax;
} PLAYER_BOX;

typedef struct _player_info {
	UINT_PTR player_addr;
	USHORT hp;
	BOOLEAN is_enemy;
	BOOLEAN is_alive;
	BOOLEAN is_self;
} PLAYER_INFO;

enum body_node {
	head = 6,
	neck = 5,
	spine = 4,
	spine_1 = 2,
	left_shoulder = 8,
	left_arm = 9,
	left_hand = 11,
	cock = 0,
	right_shoulder = 13,
	right_arm = 14,
	right_hand = 16,
	left_hip = 22,
	left_knee = 23,
	left_feet = 24,
	right_hip = 25,
	right_knee = 26,
	right_feet=27
};





VOID init_game_addr();
BOOLEAN is_in_game();
BOOLEAN refresh_data();
UINT_PTR get_local_player();
BOOLEAN is_local_player(UINT_PTR player);
BOOLEAN is_player_alive(UINT_PTR player);
BOOLEAN is_enemy(UINT_PTR player);
SHORT get_player_hp(UINT_PTR player);
const char* get_player_name(UINT_PTR player);
BOOLEAN get_player_box(UINT_PTR player, PLAYER_BOX* box);
BOOLEAN get_player_skeleton(UINT_PTR player, VEC2* buffer, int max_bone);
VOID  set_new_oritation(float max_dist,body_node node);
UINT_PTR get_entity_by_idx(UINT32 idx);