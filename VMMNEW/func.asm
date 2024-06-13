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
public save_host_state
public restore_host_state

extern g_host_save_rsp:qword
extern g_host_save_rbp:qword
extern main_vmx_exit_handler:proc
extern vmx_resume_instruction:proc
.code

get_flags proc
pushfq
pop rax
ret
get_flags endp

save_host_state proc
mov g_host_save_rsp, rsp
mov g_host_save_rbp, rbp
ret
save_host_state endp

restore_host_state proc
vmxoff
mov rsp, g_host_save_rsp
mov rbp, g_host_save_rbp
add rsp, 8;跳过压入的launch里的返回地址
mov rdx, [rsp+58h+10h]
mov ecx, [rsp+58h+8h]
add rsp, 50h
pop rdi
ret
restore_host_state endp

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
sub rsp, 28h;这里提前分配40个字节是因为_ccall中 虽然用寄存器传递，但是被调用函数会
;将寄存器又放回栈，而使用的空间，就是调用者提前分配的。
call main_vmx_exit_handler
add rsp, 28h
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
sub rsp, 0100h
jmp vmx_resume_instruction
vmx_exit_handler endp

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
sldt rax
ret
get_ldtr endp

get_tr proc
str rax
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