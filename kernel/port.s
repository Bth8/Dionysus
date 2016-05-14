; port.s - functions for reading from ports

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

global inb
inb:
	mov edx, [esp + 4]
	in al, dx

	ret

global inw
inw:
	mov edx, [esp + 4]
	in ax, dx

	ret

global inl
inl:
	mov edx, [esp + 4]
	in eax, dx

	ret

global insw
insw:
	push edi

	mov edx, [esp + 8]
	mov edi, [esp + 12]
	mov ecx, [esp + 16]

	rep insw

	pop edi
	ret

global outsw
outsw:
	push esi

	mov edx, [esp + 8]
	mov esi, [esp + 12]
	mov ecx, [esp + 16]

	rep outsw

	pop esi
	ret
