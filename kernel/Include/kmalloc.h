/* kmalloc.h - kernel heap allocation/management 
 * This is an adaptation of liballoc, written by Durand, released to the public
 * domain. Still, I feel it bears repeating: I did not write this. I only
 * adapted
 */

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

#ifndef KMALLOC_H
#define KMALLOC_H

#include <common.h>

//This lets you prefix malloc and friends
#define PREFIX(func)		k ## func
#define KHEAP_START 0xD0000000
#define KHEAP_MAX 0xDFFFFFFF

void *PREFIX(memalign)(size_t, size_t);		///< The standard function.
void *PREFIX(valloc)(size_t);				///< The standard function.
void *PREFIX(malloc)(size_t);				///< The standard function.
void *PREFIX(realloc)(void *, size_t);		///< The standard function.
void *PREFIX(calloc)(size_t, size_t);		///< The standard function.
void PREFIX(free)(void *);					///< The standard function.

#endif



