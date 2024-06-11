#pragma once
#include <ntddk.h>
#define DRIVER_TAG 0x123456u
union EXTENDED_PAGE_TABLE_POINTER {
	ULONG64 all;
	struct {
		ULONG64 ept_paging_struct_cache_type : 3;
		ULONG64 ept_page_walk_length : 3;//页表分级长度，4级为3， 5级为4
		ULONG64 ept_dirty_flag_enable : 1;
		ULONG64 reversed1 : 5;
		ULONG64 pml4_pa : 36;
		ULONG64 reversed2 : 16;
	}fields ;
};

union PAGE_MAP_LEVEL_4_ENTRY {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reversed1 : 5;
		ULONG64 dirty_flag : 1;//这个页表项所包含的所有内存是否被访问过
		ULONG64 reversed2 : 1;
		ULONG64 user_mode_execute_access : 1;//跟上面相对的，如果mode based execute control
		//被设置了，就由上面控制内核执行是否允许， 这个控制用户执行是否允许。否则，都用上面的
		//作为执行是否允许的bit
		ULONG64 reversed3 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed4 : 16;//must be zero
	}fields;
};
//第三级实际上有两种定义，一种是用来实现1GB大页面的,只由PML4和PML3
//将虚拟地址的47：30位来找页面，而页面大小是 2^30 1GB大小
//另外一种是标准PML3继续指向PML2
union PAGE_MAP_LEVEL_3_ENTRY_NORMAL {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reversed1 : 5;
		ULONG64 dirty_flag : 1;//控制的1GB内存区域是否被访问，要求EPTP bit6 1
		ULONG64 reversed2 : 1;
		ULONG64 user_mode_execute_access : 1;//同上
		ULONG64 reversed3 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed4 : 16;
	}fields;
};

//同样PML2也是两种，以下是标准版定义
union PAGE_MAP_LEVEL_2_ENTRY_NORMAL {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reversed1 : 5;
		ULONG64 dirty_flag : 1;//控制的2MB内存区域是否被访问，要求EPTP bit6 1
		ULONG64 reversed2 : 1;
		ULONG64 user_mode_execute_access : 1;//同上
		ULONG64 reversed3 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed4 : 16;
	}fields;
};
union PAGE_MAP_LEVEL_1_ENTRY {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 memory_cache_type : 3;//所关联的1KB内存的缓存方式
		ULONG64 ignore_pat_cache_type : 1;
		ULONG64 reversed1 : 1;
		ULONG64 dirty_flag : 1;//是否被访问过，要求EPTP bit 6 1
		ULONG64 write_dirty_flag : 1;//是否被写入过,同要求
		ULONG64 user_mode_execute_access : 1;
		//如果VMCS中 mode-based execute control for ept被设置
		//这个控制用户模式的执行权限， bit2 控制内核,否则 bit2既控制
		//用户执行权限，也控制内核
		ULONG64 reversed2 : 1;
		ULONG64 referenced_pa : 36;
		ULONG64 reversed3 : 1;
	}fields;
};

ULONG64 init_eptp();
