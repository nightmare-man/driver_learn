#pragma once
#include <ntddk.h>
typedef union _PML4E {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reserved : 5;
		ULONG64 access_flag : 1;
		ULONG64 ignored1 : 1;
		ULONG64 user_mode_execute_access : 1;
		ULONG64 ignored2 : 1;
		ULONG64 next_level_table_pa : 40;
		ULONG64 ignored3 : 12;
	} field;
} PML4E,*PML4E_PTR;


//1有关内存缓存的控制机制，一开始只有MTRRs和 最后一级页表项目上的 PCD(是否缓存)和PWT(缓存写方式)
//2后来引入了PAT,目的是为了大范围修改内存缓存属性时不需要大量修改页表项，所以将PCD和PWT变为了 PAT里面保存的
//内存方式的索引
//3再后来有了HYPERVISOR，又引入了EPT memory type

//关于函数返回地址的安全问题，有两种解决方案，一种是stack cookie，在某个地方多放个cookie,返回之前检查下
//如果被改了就表明返回地址不正确，这是由编译器实现的
//另一种就是影子栈了，硬件实现的，比如intel 的cet,在项目属性-链接器-高级里可以找到开启选项
//但需要知道影子栈也是保存在内存里。理论上来说，影子栈只能由硬件写入，硬件读取，能不能手动改是设置的，因此
//这里有个内核级别影子栈是否能够访问的位来控制。具体查看intel cet


typedef union _PML3E {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reserved : 5;
		ULONG64 access_flag : 1;
		ULONG64 ignored1 : 1;
		ULONG64 user_mode_execute_access : 1;
		ULONG64 ignored2 : 1;
		ULONG64 next_level_table_pa : 40;
		ULONG64 ignored3 : 12;
	}field;
} PML3E,* PML3E_PTR;

typedef union _PML3E_PAGE {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 ept_memory_type : 3;
		ULONG64 ignore_pat_memory_type : 1; //两个字段合起来才决定缓存方式，注意，另外有MTRR决定缓存方式，
		//29.3.7.2 Memory Type Used for Translated Guest-Physical Addresses
		ULONG64 must_be_one : 1;
		ULONG64 access_flag : 1;
		ULONG64 dirty_flag : 1;
		ULONG64 user_mode_execute_access : 1;
		ULONG64 ignored1 : 1;
		ULONG64 reversed1 : 18;
		ULONG64 page_pa : 22;
		ULONG64 ignored2 : 5;
		ULONG64 verify_guest_paging : 1;//是不是要启用guest分页结构验证，也就是说把guest pa处看做gust page struct
		//要通过这个entry的这个位来决定是否验证
		ULONG64 paging_write_access : 1;//
		ULONG64 ignored3 : 1;
		ULONG64 supervisor_shadow_stack_access : 1;
		ULONG64 ignored4 : 2;
		ULONG64 suppress_vm_exit : 1;//跟ept violation造成vm exit还是传递virtual exception到guest有关的设置

	}field;
} PML3E_PAGE,* PML3E_PAGE_PTR;

typedef union _PML2E {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reserved : 5;
		ULONG64 access_flag : 1;
		ULONG64 ignored1 : 1;
		ULONG64 user_mode_execute_access : 1;
		ULONG64 ignored2 : 1;
		ULONG64 next_level_table_pa : 40;
		ULONG64 ignored3 : 12;
	}field;
} PML2E,* PML2E_PTR;

typedef union _PML2E_PAGE {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 ept_memory_type : 3;
		ULONG64 ignore_pat_memory_type : 1; //两个字段合起来才决定缓存方式，注意，另外有MTRR决定缓存方式，
		//29.3.7.2 Memory Type Used for Translated Guest-Physical Addresses
		ULONG64 must_be_one : 1;
		ULONG64 access_flag : 1;
		ULONG64 dirty_flag : 1;
		ULONG64 user_mode_execute_access : 1;
		ULONG64 ignored1 : 1;
		ULONG64 reversed1 : 9;
		ULONG64 page_pa : 31;
		ULONG64 ignored2 : 5;
		ULONG64 verify_guest_paging : 1;//是不是要启用guest分页结构验证，也就是说把guest pa处看做gust page struct
		//要通过这个entry的这个位来决定是否验证
		ULONG64 paging_write_access : 1;//
		ULONG64 ignored3 : 1;
		ULONG64 supervisor_shadow_stack_access : 1;
		ULONG64 ignored4 : 2;
		ULONG64 suppress_vm_exit : 1;//跟ept violation造成vm exit还是传递virtual exception到guest有关的设置

	}field;
} PML2E_PAGE,* PML2E_PAGE_PTR;

typedef union _PML1E {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 reserved : 5;
		ULONG64 access_flag : 1;
		ULONG64 ignored1 : 1;
		ULONG64 user_mode_execute_access : 1;
		ULONG64 ignored2 : 1;
		ULONG64 next_level_table_pa : 40;
		ULONG64 ignored3 : 12;
	}field;
} PML1E,* PML1E_PTR;


typedef union _PML1E_PAGE {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;
		ULONG64 write_access : 1;
		ULONG64 execute_access : 1;
		ULONG64 ept_memory_type : 3;
		ULONG64 ignore_pat_memory_type : 1; //两个字段合起来才决定缓存方式，注意，另外有MTRR决定缓存方式，
		//29.3.7.2 Memory Type Used for Translated Guest-Physical Addresses
		ULONG64 must_be_one : 1;
		ULONG64 access_flag : 1;
		ULONG64 dirty_flag : 1;
		ULONG64 user_mode_execute_access : 1;
		ULONG64 ignored1 : 1;
		ULONG64 page_pa : 40;
		ULONG64 ignored2 : 5;
		ULONG64 verify_guest_paging : 1;//是不是要启用guest分页结构验证，也就是说把guest pa处看做gust page struct
		//要通过这个entry的这个位来决定是否验证
		ULONG64 paging_write_access : 1;//
		ULONG64 ignored3 : 1;
		ULONG64 supervisor_shadow_stack_access : 1;
		ULONG64 ignored4 : 2;
		ULONG64 suppress_vm_exit : 1;//跟ept violation造成vm exit还是传递virtual exception到guest有关的设置

	}field;
} PML1E_PAGE,* PML1E_PAGE_PTR;

enum MEM_TYPE {
	MEM_UC = 0,
	MEM_WC = 1,
	MEM_REVERSED1 = 2,
	MEM_REVERSED2 = 3,
	MEM_WRITE_THROUGH = 4,
	MEM_WRITE_PROTECTED = 5,
	MEM_WRITE_BACK=6,
	MEM_REVERSED3 = 7
};
//以下定义为左开右闭，不含end本身
typedef struct _MEM_TYPE_RANGE {
	ULONG64 start;
	ULONG64 end;
	enum MEM_TYPE type;
}MEM_TYPE_RANGE,*P_MEM_TYPE_RANGE;


//详细介绍一下mtrr，mtrr提供一种机制，描述系统内存 的物理地址范围和 缓存方式的对应关系
//从名字可以看出，类似于一种map，映射关系是物理地址范围 -》缓存方式
//实际也是这么做的，提供三种方式，默认，固定和可变的，都是通过msr来配置的（一般bios+windows已经配置好了）
//默认是提供兜底机制的，由某个def msr寄存器来规定没有被其他两种方式映射的物理地址范围是什么缓存方式
//固定范围的是已经提供好范围，只需要写入对应范围的缓存方式即可,一共有若干个寄存器
//而可变的，则首先由某个寄存器给出一共有多少个可变范围，然后对于每个可变范围，提供两个寄存器，一个记录开始地址
//一个记录结束地址，（缓存方式也记录了）


//为什么会用到mtrr?
//没有ept时，有两种机制，mtrr和pat,两个共同决定物理内存的缓存方式，
//有ept时，对于host来说，还是上面这种机制，但是对于guest来说，ept上的memory type 代替了mtrr
//和guest 上的ept共同决定有效的缓存方式（出自intel 文档）。
//因此如果我们要把已经有的host系统降成guest,我们需要把原来的mtrr里的缓存方式都记录下来，用ept
//重新设置一遍，这个过程中，需要读mtrr


BOOLEAN init_ept(ULONG64* eptp_ptr);