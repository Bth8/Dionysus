/* kmalloc.h - function declarations for allocation and freeing */

/* Copyright (C) 2014 Bth8 <bth8fwd@gmail.com>
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

void kfree(void *addr);
void *kmalloc(uint32_t sz);
void *kmalloc_a(uint32_t sz);
void *kmalloc_p(uint32_t sz, uint32_t *phys);
void *kmalloc_ap(uint32_t sz, uint32_t *phys);

#endif /* KMALLOC_H */
