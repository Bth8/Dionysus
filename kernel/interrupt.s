; interrupt.s - Set up interrupts for proper handling

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

%macro ISR_NOERRORCODE 1 ; Macro takes 1 parameter, returns dummy error code
	global isr%1
	isr%1:
		push dword 0
		push dword %1
		jmp isr_common_stub
%endmacro

%macro ISR_ERRORCODE 1 ; Returns an error code
	global isr%1
	isr%1:
		push dword %1
		jmp isr_common_stub
%endmacro

ISR_NOERRORCODE 0
ISR_NOERRORCODE 1
ISR_NOERRORCODE 2
ISR_NOERRORCODE 3
ISR_NOERRORCODE 4
ISR_NOERRORCODE 5
ISR_NOERRORCODE 6
ISR_NOERRORCODE 7
ISR_ERRORCODE 8
ISR_NOERRORCODE 9
ISR_ERRORCODE 10
ISR_ERRORCODE 11
ISR_ERRORCODE 12
ISR_ERRORCODE 13
ISR_ERRORCODE 14
ISR_NOERRORCODE 15
ISR_NOERRORCODE 16
ISR_NOERRORCODE 17
ISR_NOERRORCODE 18
ISR_NOERRORCODE 19
ISR_NOERRORCODE 20
ISR_NOERRORCODE 21
ISR_NOERRORCODE 22
ISR_NOERRORCODE 23
ISR_NOERRORCODE 24
ISR_NOERRORCODE 25
ISR_NOERRORCODE 26
ISR_NOERRORCODE 27
ISR_NOERRORCODE 28
ISR_NOERRORCODE 29
ISR_NOERRORCODE 30
ISR_NOERRORCODE 31
ISR_NOERRORCODE 128


; In idt.c
extern isr_handler

; Saves everything, goes kernel mode, calls fault handler, restores everything
isr_common_stub:
	pushad			; Push eax, ecx, edx, ebx, esp, ebp, esi, edi

	mov ax, ds		; Save segment descriptor
	push eax

	mov ax, 0x10	; Kernel segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	push esp
	call isr_handler
	add esp, 4

	pop eax			; reload segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	popad			; Pop eax, ecx...
	add esp, 8		; Gets rid of pushed error code and ISR number

	iret			; interrupt return

%macro IRQ 2
  global irq%1
  irq%1:
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

extern irq_handler

irq_common_stub:
	pushad

	mov ax, ds
	push eax

	mov ax, 0x10	; Kernel segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	push esp
	call irq_handler
	add esp, 4

	pop eax			; reload segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	popad			; Pop eax, ecx...
	add esp, 8		; Gets rid of pushed error code and ISR number

	iret			; interrupt return
