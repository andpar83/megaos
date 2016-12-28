;;kernel.asm

;nasm directive - 32 bit
bits 32
section .text

code_begin:
        ;multiboot spec
        align 4
        dd 0x1BADB002            ;magic
        dd 0x00                  ;flags
        dd - (0x1BADB002 + 0x00) ;checksum. m+f+c should be zero

global start
global read_port
global write_port
global keyboard_handler
global load_idt

extern kmain	        ;kmain is defined in the c file
extern keyboard_handler_main

; param1 - port number (int32)
read_port:
	mov edx, [esp + 4]
	in al, dx	
	ret

; param1 - port number (int32)
; param2 - byte of data (passed as int32)
write_port:
	mov   edx, [esp + 4]    
	mov   al, [esp + 4 + 4]  
	out   dx, al  
	ret

; param1 - address of IDT descriptor (int32)
load_idt:
	mov edx, [esp + 4]
	lidt [edx]
	sti
	ret

; keyboard interrupt handler
keyboard_handler:                 
	call    keyboard_handler_main
	iretd

start:
  cli 			;block interrupts
  mov eax, code_begin   ;getting address of our program in memory (eip)
  mov esp, stack_space	;set stack pointer

  push stack_space
  push eax
  call kmain
  add esp, 8

loop:
  hlt		 	;halt the CPU
  jmp loop

section .bss
resb 8192		;8KB for stack
stack_space:
