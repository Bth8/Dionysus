/* timer.c - initializes and handles PIT interrupts */

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

#include <common.h>
#include <timer.h>
#include <idt.h>
#include <task.h>
#include <time.h>
#include <structures/list.h>
#include <kmalloc.h>

// Defined in time.c
extern time_t current_time;

volatile uint32_t tick = 0;
int32_t task_tick = 200;
uint32_t rtc_tick = 1024;
list_t *timers = NULL;

waitqueue_t *timer_wq = NULL;

static void pit_callback(registers_t *regs) {
	++tick;

	irq_ack(regs->int_no);

	node_t *node;
	foreach(node, timers) {
		struct timer *timer = (struct timer *)node->data;
		if (timer->expires == tick)
			timer->callback();
	}
}

static void rtc_callback(registers_t *regs) {
	if (--rtc_tick <= 0) {
		current_time++;
		rtc_tick = 4096;
	}

	// Re-enables RTC interrupts
	READ_CMOS(CMOS_RTC_STAT_C);
	irq_ack(regs->int_no);

	if (--task_tick <= 0)
		task_tick = 10 * (20 - switch_task(1));
}

void init_timer(void) {
	asm volatile("cli");
	// PIT
	ASSERT(register_interrupt_handler(IRQ0, &pit_callback) == 0);

	uint32_t divisor = 1193182 / HZ;

	timers = list_create();
	ASSERT(timers);
	timer_wq = create_waitqueue();
	ASSERT(timer_wq);

	// Command byte
	outb(PIT_CMD, PIT_CHAN0 | PIT_LOHI | PIT_MODE3 | PIT_BIN);

	outb(PIT_PORT0, divisor & 0xFF);
	outb(PIT_PORT0, divisor >> 8);

	// RTC
	ASSERT(register_interrupt_handler(IRQ8, &rtc_callback) == 0);

	uint8_t prev = READ_CMOS(CMOS_RTC_STAT_A);
	WRITE_CMOS(CMOS_RTC_STAT_A, (prev & 0xF0) | CMOS_RTC_RATE);

	prev = READ_CMOS(CMOS_RTC_STAT_B);
	WRITE_CMOS(CMOS_RTC_STAT_B, prev | CMOS_RTC_INT);

	// Enable interrupts
	asm volatile("sti");
}

int add_timer(struct timer *timer) {
	ASSERT(timer);
	node_t *node = list_insert(timers, timer);
	if (!node)
		return -1;
	return 0;
}

void del_timer(struct timer *timer) {
	ASSERT(timer);
	node_t *node = list_find(timers, timer);
	list_remove(timers, node);
}

void wake_timers() {
	wake_queue(timer_wq);
}

void sleep_until(uint32_t expires) {
	if (expires <= tick)
		return;
	struct timer *timer = (struct timer *)kmalloc(sizeof(struct timer));
	ASSERT(timer);

	timer->callback = wake_timers;
	timer->expires = expires;
	add_timer(timer);

	wait_event(timer_wq, tick >= expires);

	del_timer(timer);
}