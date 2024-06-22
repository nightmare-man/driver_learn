PUBLIC save_state_and_virtualize
PUBLIC vmm_exit_handler
PUBLIC restore_state
PUBLIC get_es
PUBLIC get_cs
PUBLIC get_ss
PUBLIC get_ds
PUBLIC get_fs
PUBLIC get_gs
PUBLIC get_ldtr
PUBLIC get_tr
PUBLIC get_gdt_base
PUBLIC get_idt_base
PUBLIC get_gdt_limit
PUBLIC get_idt_limit
PUBLIC get_flags
extern virtualize_cpu:proc
extern main_exit_handler:proc
extern resume_guest:proc
.CODE
get_flags proc
pushfq
pop rax
ret
get_flags endp

get_es proc
mov ax, es
ret
get_es endp

get_cs proc
mov ax, cs
ret
get_cs endp

get_ss proc
mov ax, ss
ret
get_ss endp

get_ds proc
mov ax, ds
ret
get_ds endp

get_fs proc
mov ax, fs
ret
get_fs endp

get_gs proc
mov ax, gs
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
mov rax,qword ptr gdtr[2]
ret
get_gdt_base endp

get_idt_base proc
local idtr[10]:byte
sidt idtr
mov rax,qword ptr idtr[2]
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

save_state_and_virtualize proc
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
sub rsp,28h
mov rdx,rsp
call virtualize_cpu;正常来说不反回
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
mov rax,0
ret
save_state_and_virtualize endp

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
mov rax, 1
ret
restore_state endp

vmm_exit_handler proc
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
mov rcx,rsp
sub rsp,28h
call main_exit_handler
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

sub rsp,100h
jmp resume_guest
vmm_exit_handler endp
END