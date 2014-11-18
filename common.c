/* common.c - globally-used function definitions */

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

#include <common.h>
#include <printf.h>
#include <timer.h>

void panic(uint32_t line, char *file, char *msg) {
	asm volatile("cli");

	printf("KERNEL PANIC AT LINE %u IN FILE %s: %s\n", line, file, msg);

	halt();
}

void spin_lock(volatile spinlock_t *lock) {
	while (__sync_lock_test_and_set(lock, 1)) {
		halt();
	}
}

void spin_unlock(volatile spinlock_t *lock){
	__sync_lock_release(lock);
}