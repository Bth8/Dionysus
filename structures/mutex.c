/* mutex.c - Mutual exclusion */

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
#include <structures/mutex.h>
#include <kmalloc.h>

mutex_t *create_mutex(uint32_t locked) {
	mutex_t *mutex = (mutex_t *)kmalloc(sizeof(mutex_t));
	if (!mutex)
		return NULL;

	mutex->mutex = locked ? 1 : 0;
	mutex->wq = create_waitqueue();
	if (!mutex->wq) {
		kfree(mutex);
		return NULL;
	}

	return mutex;
}

void destroy_mutex(mutex_t *mutex) {
	destroy_waitqueue(mutex->wq);
	kfree(mutex);
}

void acquire_mutex(volatile mutex_t *mutex) {
	wait_event_interruptable(mutex->wq,
		__sync_lock_test_and_set(&mutex->mutex, 1) == 1);

}

void release_mutex(volatile mutex_t *mutex) {
	__sync_lock_release(&mutex->mutex);
	wake_queue(mutex->wq);
}

sem_t *create_semaphore(uint32_t max) {
	ASSERT(max > 0);

	sem_t *sem = (sem_t *)kmalloc(sizeof(sem_t));
	if (!sem)
		return NULL;

	sem->semaphore = 0;
	sem->max = max;
	sem->wq = create_waitqueue();
	if (!sem->wq) {
		kfree(sem);
		return NULL;
	}

	return sem;
}

void destroy_semaphore(sem_t *sem) {
	destroy_waitqueue(sem->wq);
	kfree(sem);
}

void acquire_semaphore(volatile sem_t *sem) {
	while (__sync_add_and_fetch(&sem->semaphore, 1) > sem->max) {
		// This probably doesn't need to be atomic
		sem->semaphore--;
		sleep_thread(sem->wq, SLEEP_INTERRUPTABLE);
	}
}

void release_semaphore(volatile sem_t *sem) {
	__sync_sub_and_fetch(&sem->semaphore, 1);
	wake_queue(sem->wq);
}

rw_sem_t *create_rw_semaphore(uint32_t max) {
	rw_sem_t *sem = (rw_sem_t *)kmalloc(sizeof(rw_sem_t));

	sem->write = create_mutex(0);
	if (!sem->write) {
		kfree(sem);
		return 0;
	}

	sem->semaphore = create_semaphore(max);
	if (!sem->semaphore) {
		destroy_mutex(sem->write);
		kfree(sem);
	}

	return sem;
}

void destroy_rw_semaphore(rw_sem_t *sem) {
	destroy_mutex(sem->write);
	destroy_semaphore(sem->semaphore);
	kfree(sem);
}

void acquire_semaphore_read(volatile rw_sem_t *sem) {
	acquire_semaphore(sem->semaphore);
}

void release_semaphore_read(volatile rw_sem_t *sem) {
	release_semaphore(sem->semaphore);
}

void acquire_semaphore_write(volatile rw_sem_t *sem) {
	acquire_mutex(sem->write);
		uint32_t i;
		for (i = 0; i < sem->semaphore->max; i++)
			acquire_semaphore(sem->semaphore);

	release_mutex(sem->write);
}

void release_semaphore_write(volatile rw_sem_t *sem) {
	sem->semaphore -= sem->semaphore->max;
	wake_queue(sem->semaphore->wq);
}