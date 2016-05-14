; string.s - replacement for glibc's string functions.
; Only implemented as needed, hence the lack of functions

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

global strlen
strlen:
	push edi

	mov edi, [esp + 8]
	xor al, al
	xor ecx, ecx

	cld

.check:
	scasb
	je .end
	inc ecx
	jmp .check
.end:
	mov eax, ecx
	pop edi
	ret

global memset
memset:
	push edi

	mov edi, [esp + 8] ; Address of buf
	mov al, [esp + 12] ; Value
	mov ecx, [esp + 16] ; Number of bytes to fill

	cld

	rep stosb

	pop edi
	ret

global memcpy
memcpy:
	push edi
	push esi

	mov edi, [esp + 12]
	mov esi, [esp + 16]
	mov ecx, [esp + 20]

	cld

	rep movsb

	pop esi
	pop edi
	ret

global memmove
memmove:
	push edi
	push esi

	mov edi, [esp + 12]
	mov esi, [esp + 16]
	mov ecx, [esp + 20]
	
	cld

	cmp edi, esi
	jnl .move
	std
	add esi, ecx
	dec esi
	add edi, ecx
	dec esi
.move:
	rep movsb

	mov eax, [esp + 12]
	pop esi
	pop edi
	ret

global memcmp
memcmp:
	push edi
	push esi

	mov edi, [esp + 12]
	mov esi, [esp + 16]
	mov ecx, [esp + 20]

	cld

	repe cmpsb

	pop esi
	pop edi
	
	ja .greater
	jb .less
	xor eax, eax
	ret
.greater:
	mov eax, 1
	ret
.less:
	mov eax, -1
	ret

global strcpy
strcpy:
	push edi
	push esi

	mov edi, [esp + 12]
	mov esi, [esp + 16]

	push esi
	call strlen
	add esp, 4
	inc eax

	push eax
	push esi
	push edi
	call memcpy
	add esp, 12

	pop esi
	pop edi
	ret

global strncpy
strncpy:
	push edi
	push esi

	mov edi, [esp + 12]
	mov esi, [esp + 16]
	mov ecx, [esp + 20]

	push esi
	call strlen
	pop esi
	inc eax

	cmp ecx, eax
	jge .greater

	push eax
	push esi
	push edi
	call memcpy
	add esp, 12
	add edi, eax

	sub ecx, eax
	push ecx
	push 0
	push edi
	call memset
	jmp .end

.greater:
	push ecx
	push esi
	push edi
	call memcpy
	add esp, 12

	rep movsb

.end:
	pop esi
	pop edi
	ret

global strcmp
strcmp:
	push edi
	push esi

	mov edi, [esp + 12]
	mov esi, [esp + 16]

	push esi
	call strlen
	add esp, 4

	push eax
	push edi
	call strlen
	add esp, 4
	pop edx

	cmp eax, edx
	ja .greater
	push eax
	jmp .call

.greater:
	push ecx

.call:
	push esi
	push edi
	call memcmp
	add esp, 12

	pop esi
	pop edi
	ret


; Needs to be fixed
global strncmp
strncmp:
	push edi
	push esi

	mov esi, [esp+12]
	mov edi, [esp+16]
	mov ecx, [esp+20]

	repe cmpsb
	ja .greater
	jb .less
	pop esi
	pop edi
	xor eax, eax
	ret
.greater:
	pop esi
	pop edi
	mov eax, 1
	ret
.less:
	pop esi
	pop edi
	mov eax, -1
	ret
