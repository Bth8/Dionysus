/* timer.c - initializes and handles PIT interrupts */

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
#include <timer.h>
#include <idt.h>
#include <task.h>
#include <time.h>

// Defined in time.c
extern time_t current_time;

uint64_t tick = 0;
int32_t task_tick = 20;
uint32_t rtc_tick = 1024;

static void pit_callback(registers_t *regs) {
	++tick;
	if (--task_tick <= 0)
		task_tick = (20 - switch_task());
}

static void rtc_callback(registers_t *regs) {
	if (--rtc_tick <= 0) {
		current_time++;
		rtc_tick = 1024;
	}
	// Re-enables RTC interrupts
	READ_CMOS(CMOS_RTC_STAT_C);
}

void init_timer(void) {
	asm volatile("cli");
	// PIT
	register_interrupt_handler(IRQ0, &pit_callback);

	uint32_t divisor = 1193182 / HZ;

	// Command byte
	outb(PIT_CMD, PIT_CHAN0 | PIT_LOHI | PIT_MODE3 | PIT_BIN);

	outb(PIT_PORT0, divisor & 0xFF);
	outb(PIT_PORT0, divisor >> 8);

	// RTC
	register_interrupt_handler(IRQ8, &rtc_callback);

	uint8_t prev = READ_CMOS(CMOS_RTC_STAT_B);
	WRITE_CMOS(CMOS_RTC_STAT_B, prev | CMOS_RTC_INT);

	// Enable interrupts
	asm volatile("sti");
}

void wait(uint32_t ms) {
	uint32_t finish = tick + ms;
	while (tick > finish)
		continue;
}

void sleep_thread(void) {
	task_tick = 0;
}
