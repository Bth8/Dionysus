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
#include <monitor.h>

u32int tick = 0;

static void timer_callback(registers_t regs) {
	monitor_write("Tick\n");
	tick++;
	if (tick > 3) {
		tick = 0;
		switch_task();
	}
	regs.eax = regs.eax;
}

void init_timer(u32int freq) {
	register_interrupt_handler(IRQ0, &timer_callback);

	u32int divisor = 1193180 / freq;

	// Command byte
	outb(0x43, 0x36);

	outb(0x40, divisor & 0xFF);
	outb(0x40, divisor >> 8);
}
