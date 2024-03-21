#pragma once

enum ItemType {
	None,
	ProcessCreate,
	ProcessExit
};

struct ItemHeader {
	enum ItemType type;
	unsigned short size;
	LARGE_INTEGER time;
};

struct ProcessExitInfo {
	struct ItemHeader header;
	unsigned long process_id;
};

struct ProcessCreateInfo {
	struct ItemHeader header;
	unsigned long process_id;
	unsigned short cmd_line_len;
	unsigned short cmd_line_offset;
};

