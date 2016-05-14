/* string.h - emulates glibc string.h */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
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

uint32_t strlen(const char *buf);
void memset(void *buf, uint8_t value, uint32_t num);
void memcpy(void *dest, const void *src, uint32_t size);
void *memmove(void *dest, const void *src, uint32_t size);
int32_t memcmp(const void *buf1, const void *buf2, uint32_t size);
void strcpy(char *dest, const char *src);
void strncpy(char *dest, const char *src, uint32_t len);
int32_t strcmp(const char *str1, const char *str2);
int32_t strncmp(const char *str1, const char *str2, uint32_t len);

#endif /* STRING_H */
