public get_cs
public get_ss
public get_ds
public get_es
public get_fs
public get_gs
public get_flags
public get_ldtr
public get_tr
public get_gdt_base
public get_idt_base
public get_gdt_limit
public get_idt_limit
public vmx_exit_handler

public restore_state
public save_and_virtualize



extern g_guest_rsp:qword
extern g_guest_rip:qword
extern main_vmx_exit_handler:proc
extern vmx_resume_instruction:proc

extern virtualize_cpu:proc
extern read_guest_rsp:proc
extern read_guest_rip:proc
extern leave_vmx:proc


.code
save_and_virtualize proc
push rax
push rbx
push rcx
push rdx
push rbp
push rsi
push rdi
push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15

sub rsp, 28h
mov rdx, rsp
call virtualize_cpu
add rsp, 28h;理论上后面不可能被执行到
;执行virtualize_cpu后就vm entry 切换到guest
;guest采用另一个函数restore_state来恢复执行
;vm exit则用 exit_handler 里面通过读vmcs获取guest exit
;时的rsp rip,来继续执行下一条
;来resume,要vmoff的时候，也用改rip rsp恢复
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rbp
pop rdx
pop rcx
pop rbx
pop rax
ret
save_and_virtualize endp

restore_state proc
add rsp,28h
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rbp
pop rdx
pop rcx
pop rbx
pop rax
ret
restore_state endp

get_flags proc
pushfq
pop rax
ret
get_flags endp


vmx_exit_handler proc
push rax
push rbx
push rcx
push rdx
push rbp
push rsi
push rdi
push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15
mov rcx, rsp;传递参数
sub rsp, 28h;这里提前分配40个字节是因为_ccall中 虽然用寄存器传递，但是被调用函数会
;将寄存器又放回栈，而使用的空间，就是调用者提前分配的。
call main_vmx_exit_handler
add rsp, 28h
cmp al,1
je vmx_off_handler
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rbp
pop rdx
pop rcx
pop rbx
pop rax
;sub 100h单纯为了将栈离远点，防止干扰
sub rsp, 0100h;每次exit时host的 rsp 和rip都是固定的，因此不会一直减少越界
jmp vmx_resume_instruction
vmx_exit_handler endp


vmx_off_handler proc
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rbp
pop rdx
pop rcx
pop rbx
pop rax
mov rsp,g_guest_rsp;
push rax
push rbx
push rcx
push rdx
push rbp
push rsi
push rdi
push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15
sub rsp,100h
call leave_vmx
add rsp,100h
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rbp
pop rdx
pop rcx
pop rbx
pop rax
jmp g_guest_rip;这里jump跳到的目标地址不能是是r3的，因为VMM本身是r0的
vmx_off_handler endp


get_cs proc
mov rax, cs
ret
get_cs endp

get_ss proc
mov rax, ss
ret
get_ss endp

get_ds proc
mov rax, ds
ret
get_ds endp

get_es proc
mov rax, es
ret
get_es endp

get_fs proc
mov rax, fs
ret
get_fs endp

get_gs proc
mov rax, gs
ret
get_gs endp

get_ldtr proc
sldt ax
ret
get_ldtr endp

get_tr proc
str ax
ret
get_tr endp

get_gdt_base proc
local gdtr[10]:byte
sgdt gdtr
mov rax, qword ptr gdtr[2]
ret
get_gdt_base endp

get_idt_base proc
local idtr[10]:byte
sidt idtr
mov rax, qword ptr idtr[2]
ret
get_idt_base endp


get_gdt_limit proc
local gdtr[10]:byte
sgdt gdtr
mov ax, word ptr gdtr[0]
ret
get_gdt_limit endp

get_idt_limit proc
local idtr[10]:byte
sidt idtr
mov ax, word ptr idtr[0]
ret
get_idt_limit endp
end