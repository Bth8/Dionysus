; boot.s - initial start code. Sets up multiboot options, jumps to higher
; half and loads kmain

; Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
;
;  This file is part of Dionysus.
;
;  Dionysus is free software: you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, either version 3 of the License, or
;  (at your option) any later version.
;
;  Dionysus is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with Dionysus.  If not, see <http://www.gnu.org/licenses/>

; Multiboot header stuff
MBOOT_MAGIC_HEADER	equ 0x1BADB002	; Kernel magic
MBOOT_PAGE_ALIGN	equ 1			; Load on a page boundary
MBOOT_MEM_INFO		equ 1<<1		; Returns memory info
MBOOT_VIDEO_MODE	equ 1<<2		; Returns video mode
MBOOT_AOUT_KLUDGE	equ	1<<16		; Use to return symbol table so aout works

MBOOT_FLAGS			equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM		equ -(MBOOT_MAGIC_HEADER + MBOOT_FLAGS)

; Other constants
STACKSIZE			equ 0x0400		; Temporary stack until we set up
									; multitasking
KERNEL_VIRTUAL_BASE	equ 0xC0000000
KERNEL_PAGE_NUMBER	equ (KERNEL_VIRTUAL_BASE >> 22)

[BITS 32]

section .data

; Used to set us up in the higher half. Temporary.
; Real paging implemented later
align 4096
global boot_dir
boot_dir:
	; Identity map first 4MiB. Required for now; okay to unmap later
	dd 0x00000083
	times (KERNEL_PAGE_NUMBER - 1) dd 0
	; Page directory containing kernel mapped
	dd 0x00000083
	times (1024 - KERNEL_PAGE_NUMBER - 2) dd 0

section .text
dd MBOOT_MAGIC_HEADER
dd MBOOT_FLAGS
dd MBOOT_CHECKSUM

global start
extern kmain

start:
	; Disable interrupts until we get the IDT going
	cli
	; Page dir addresses must be physical
	mov ecx, (boot_dir - KERNEL_VIRTUAL_BASE)
	mov cr3, ecx

	; Allow 4MiB pages
	mov ecx, cr4
	bts ecx, 4
	mov cr4, ecx

	; Enable paging and turn off write protection
	mov ecx, cr0
	bts ecx, 31
	btr ecx, 16
	mov cr0, ecx

	; Allow global pages
	mov ecx, cr4
	bts ecx, 7
	mov cr4, ecx

	; Jump to kernel space
	lea ecx, [higher_half]
	jmp ecx

higher_half:
	mov esp, stack_top
	mov ebp, stack_top
	push ebp
	push ebx
	push eax

	call kmain

	; C code should never return, but if it does, halt
	hlt

section .bss

; reserve memory for stack
stack_bottom:
	resb STACKSIZE
stack_top:
