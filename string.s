; string.s - replacement for glibc's string functions. Only implemented when needed, hence the lack of functions
; Copyright (C) 2011-2013 Bth8 <bth8fwd@gmail.com>
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

	mov edi, [esp + 8]		; Address of buf
	mov al, [esp + 12]		; Value
	mov ecx, [esp + 16]		; Number of bytes to fill

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

	rep movsb

	pop esi
	pop edi
	ret

global strcpy
strcpy:
	push edi
	push esi

	mov edi, [esp + 12]
	mov esi, [esp + 16]
	xor al, al
	xor ecx, ecx

	push esi
	call strlen
	pop esi
	inc eax

	push eax
	push esi
	push edi
	call strncpy
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

	rep movsb

	pop esi
	pop edi
	ret

global strcmp
strcmp:
	push edi
	push esi

	mov esi, [esp + 12]
	mov edi, [esp + 16]

	push esi
	call strlen
	pop esi

	push eax
	push edi
	call strlen
	pop edi
	pop ecx

	cmp ecx, eax
	ja .greater
	jb .less

	push ecx
	push edi
	push esi
	call strncmp
	add esp, 12
	pop esi
	pop edi
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
