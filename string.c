/* string.h - replacement for glibc's string functions. Only implemented when needed, hence the lack of complete functions */
/* Copyright (C) 2011 Bth8 <bth8fwd@gmail.com>
 *
 *  This file is part of Dionysus.
 *
 *  Dionysus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Dionysus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dionysus.  If not, see <http://www.gnu.org/licenses/>
 */

#include <common.h>
#include <string.h>

u32int strlen(char *buf) {
	u32int i;
	for (i = 0; buf[i] != '\0'; i++)
		continue;
	return i;
}

void memset(u8int *buf, u8int value, u32int num) {
	u32int i;
	for (i = 0; i < num; i++)
		buf[i] = value;
}

void memcpy(u8int *dest, u8int *src, u32int size) {
	u32int i;
	for (i = 0; i < size; i++)
		dest[i] = src[i];
}
