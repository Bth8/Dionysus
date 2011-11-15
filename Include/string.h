/* string.h - emulates glibc string.h */
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

#ifndef STRING_H
#define STRING_H
#include <common.h>

u32int strlen(const char *buf);
void memset(void *buf, u8int value, u32int num);
void memcpy(void *dest, const void *src, u32int size);
void strcpy(char *dest, const char *src);
s32int strcmp(const char *str1, const char *str2);
s32int strncmp(const char *str1, const char *str2, u32int len);

#endif
