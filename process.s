; process.s - Assembly functions used for process handling

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

[bits 32]
; Defined in boot.s
extern BootPageDir
; Ensure this is the same as the one in boot.s
KERNEL_VIRTUAL_BASE equ 0xC0000000

global copy_page_physical
copy_page_physical:
	push ebx
	push esi
	push edi
	pushf				; To remember state of interrupts

	cli

	mov esi, [esp + 20]	; Source
	mov edi, [esp + 24]	; Dest

	mov ebx, cr3		; Preserve page dir

	mov eax, (BootPageDir - KERNEL_VIRTUAL_BASE)
	mov cr3, eax		; Change page dir to the one used at boot time

	lea eax, [.continue]	; Get address
	sub eax, KERNEL_VIRTUAL_BASE
	jmp eax

.continue:
	; Disable paging so that we can access physical addresses
	mov edx, cr0
	btr edx, 31
	mov cr0, edx

	; copy page
	mov ecx, 1024
	rep movsd

	; reenable paging
	bts edx, 31
	mov cr0, edx

	lea eax, [.higher]
	jmp eax

.higher:
	; Reinstate original page directory
	mov cr3, ebx

	popf
	pop edi
	pop esi
	pop ebx
	ret

global read_eip
read_eip:
	pop eax
	jmp eax
