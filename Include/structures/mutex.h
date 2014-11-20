#ifndef MUTEX_H
#define MUTEX_H

#include <common.h>
#include <task.h>

#define SEM_READ	0
#define SEM_WRITE	1

typedef struct {
	uint32_t mutex;
	waitqueue_t *wq;
} mutex_t;

typedef struct {
	uint32_t semaphore;
	uint32_t max;
	waitqueue_t *wq;
} sem_t;

typedef struct {
	mutex_t *write;
	sem_t *semaphore;
} rw_sem_t;

mutex_t *create_mutex(uint32_t locked);
void destroy_mutex(mutex_t *mutex);
void acquire_mutex(volatile mutex_t *mutex);
void release_mutex(volatile mutex_t *mutex);

sem_t *create_semaphore(uint32_t max);
void destroy_semaphore(sem_t *sem);
void acquire_semaphore(volatile sem_t *sem);
void release_semaphore(volatile sem_t *sem);

rw_sem_t *create_rw_semaphore(uint32_t max);
void destroy_rw_semaphore(rw_sem_t *sem);
void acquire_semaphore_read(volatile rw_sem_t *sem);
void acquire_semaphore_write(volatile rw_sem_t *sem);
void release_semaphore_read(volatile rw_sem_t *sem);
void release_semaphore_write(volatile rw_sem_t *sem);

#endif