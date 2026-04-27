MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDINFO  equ  1 << 2 
FLAGS    equ  MBALIGN | MEMINFO | VIDINFO
MAGIC    equ  0x1BADB002        
CHECKSUM equ -(MAGIC + FLAGS)   

section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM
	dd 0, 0, 0, 0, 0 
	dd 0             
	dd 800           
	dd 600           
	dd 32            
align 16
stack_bottom:
resb 16384
stack_top:

section .text
global _start:function (_start.end - _start)
_start:
	mov esp, stack_top

	push ebx
	push eax

	extern kernel_main
	call kernel_main

	cli
.hang:	hlt
	jmp .hang
.end:
