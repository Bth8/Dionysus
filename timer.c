/* timer.c - initializes and handles PIT interrupts */
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
#include <timer.h>
#include <idt.h>
#include <task.h>
#include <time.h>

// Defined in time.c
extern time_t current_time;

u32int tick = 0;
u32int task_tick = 20;
u32int rtc_tick = 1024;

static void pit_callback(registers_t regs) {
	++tick;
	if (--task_tick <= 0)
		task_tick = (20 - switch_task());
	// Compiler complains otherwise
	regs.eax = regs.eax;
}

static void rtc_callback(registers_t regs) {
	if (--rtc_tick <= 0) {
		current_time++;
		rtc_tick = 1024;
	}
	READ_CMOS(0x0C);		// Register C has info on what int just fired. We don't care, trash it
	// Compiler complains otherwise
	regs.eax = regs.eax;
}

void init_timer(u32int freq) {
	asm volatile("cli");
	// PIT
	register_interrupt_handler(IRQ0, &pit_callback);

	u32int divisor = 1193182 / freq;

	// Command byte
	outb(0x43, 0x36);

	outb(0x40, divisor & 0xFF);
	outb(0x40, divisor >> 8);

	// RTC
	register_interrupt_handler(IRQ8, &rtc_callback);
	// Enable interrupts
	u8int prev = READ_CMOS(0x0B);
	WRITE_CMOS(0x0B, prev | 0x40);
	asm volatile("sti");
}

void sleep(u32int ms) {
	u32int finish = tick + ms;
	while (tick > finish)
		continue;
}
