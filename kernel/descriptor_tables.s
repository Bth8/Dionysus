; descriptor_tables.s - a couple of functions to flush GDT, IDT and TSS

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

[BITS 32]

global gdt_flush

gdt_flush:
	; Move GDT descriptor pointer into EAX and load
	mov eax, [esp+4]
	lgdt [eax]

	; 0x10 is the GDT offset to our data segment. we use ax because we can't
	;  directly modify segment registers
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	; 0x08 is the GDT offset to our code segment: far jump
	jmp 0x08:.flush

.flush:
	ret

global idt_flush

idt_flush:
	mov eax, [esp+4]
	lidt [eax]
	sti
	ret

global tss_flush

tss_flush:
	; TSS structure index in GDT. 5th selector (0x28) with RPL 3
	mov ax, 0x2B
	ltr ax
	ret
