/* idt.c - Code for setting up the IDT and registering ISRs */
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

#include <idt.h>
#include <common.h>
#include <monitor.h>

// Flush IDT. Defined in descriptor_tables.s
extern void idt_flush(u32int);

idt_entry_t idt_entries[256];
idt_ptr_t idt_ptr;

isr_t isr_handlers[256];

void idt_set_gate(u8int num, u32int handler, u16int sel, u8int flags) {
	idt_entries[num].handler_low = handler & 0xFFFF;
	idt_entries[num].handler_high = handler >> 16;

	idt_entries[num].selector = sel;

	idt_entries[num].zero = 0;

	// Must be uncommented when we start running user mode. Drops privs to 3.
	idt_entries[num].flags = flags /* | 0x60 */;
}

void register_interrupt_handler(u8int n, isr_t handler) {
	isr_handlers[n] = handler;
}

void init_idt(void) {
	idt_ptr.offset = (sizeof(idt_entry_t) * 256) - 1;
	idt_ptr.base = (u32int)&idt_entries;

	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);

	idt_set_gate(0, (u32int)isr0, 0x08, 0x8E);
	idt_set_gate(1, (u32int)isr1, 0x08, 0x8E);
	idt_set_gate(2, (u32int)isr2, 0x08, 0x8E);
	idt_set_gate(3, (u32int)isr3, 0x08, 0x8E);
	idt_set_gate(4, (u32int)isr4, 0x08, 0x8E);
	idt_set_gate(5, (u32int)isr5, 0x08, 0x8E);
	idt_set_gate(6, (u32int)isr6, 0x08, 0x8E);
	idt_set_gate(7, (u32int)isr7, 0x08, 0x8E);
	idt_set_gate(8, (u32int)isr8, 0x08, 0x8E);
	idt_set_gate(9, (u32int)isr9, 0x08, 0x8E);
	idt_set_gate(10, (u32int)isr10, 0x08, 0x8E);
	idt_set_gate(11, (u32int)isr11, 0x08, 0x8E);
	idt_set_gate(12, (u32int)isr12, 0x08, 0x8E);
	idt_set_gate(13, (u32int)isr13, 0x08, 0x8E);
	idt_set_gate(14, (u32int)isr14, 0x08, 0x8E);
	idt_set_gate(15, (u32int)isr15, 0x08, 0x8E);
	idt_set_gate(16, (u32int)isr16, 0x08, 0x8E);
	idt_set_gate(17, (u32int)isr17, 0x08, 0x8E);
	idt_set_gate(18, (u32int)isr18, 0x08, 0x8E);
	idt_set_gate(19, (u32int)isr19, 0x08, 0x8E);
	idt_set_gate(20, (u32int)isr20, 0x08, 0x8E);
	idt_set_gate(21, (u32int)isr21, 0x08, 0x8E);
	idt_set_gate(22, (u32int)isr22, 0x08, 0x8E);
	idt_set_gate(23, (u32int)isr23, 0x08, 0x8E);
	idt_set_gate(24, (u32int)isr24, 0x08, 0x8E);
	idt_set_gate(25, (u32int)isr25, 0x08, 0x8E);
	idt_set_gate(26, (u32int)isr26, 0x08, 0x8E);
	idt_set_gate(27, (u32int)isr27, 0x08, 0x8E);
	idt_set_gate(28, (u32int)isr28, 0x08, 0x8E);
	idt_set_gate(29, (u32int)isr29, 0x08, 0x8E);
	idt_set_gate(30, (u32int)isr30, 0x08, 0x8E);
	idt_set_gate(31, (u32int)isr31, 0x08, 0x8E);
	idt_set_gate(32, (u32int)irq0, 0x08, 0x8E);
	idt_set_gate(33, (u32int)irq1, 0x08, 0x8E);
	idt_set_gate(34, (u32int)irq2, 0x08, 0x8E);
	idt_set_gate(35, (u32int)irq3, 0x08, 0x8E);
	idt_set_gate(36, (u32int)irq4, 0x08, 0x8E);
	idt_set_gate(37, (u32int)irq5, 0x08, 0x8E);
	idt_set_gate(38, (u32int)irq6, 0x08, 0x8E);
	idt_set_gate(39, (u32int)irq7, 0x08, 0x8E);
	idt_set_gate(40, (u32int)irq8, 0x08, 0x8E);
	idt_set_gate(41, (u32int)irq9, 0x08, 0x8E);
	idt_set_gate(42, (u32int)irq10, 0x08, 0x8E);
	idt_set_gate(43, (u32int)irq11, 0x08, 0x8E);
	idt_set_gate(44, (u32int)irq12, 0x08, 0x8E);
	idt_set_gate(45, (u32int)irq13, 0x08, 0x8E);
	idt_set_gate(46, (u32int)irq14, 0x08, 0x8E);
	idt_set_gate(47, (u32int)irq15, 0x08, 0x8E);
	idt_set_gate(128, (u32int)isr128, 0x08, 0xEE);

	idt_flush((u32int)&idt_ptr);
}

void isr_handler(registers_t regs) {
	isr_t handler;
	if ((handler = isr_handlers[regs.int_no]) != NULL)
		handler(&regs);
	else {
		monitor_write("Unhandled interrupt: ");
		monitor_write_udec(regs.int_no);
		monitor_put('\n');
		halt();
	}
}

void irq_handler(registers_t regs) {
	isr_t handler;
	if ((handler = isr_handlers[regs.int_no]) != NULL)
		handler(&regs);

	if (regs.int_no >= IRQ8) // Signal came from slave
		outb(0xA0, 0x20); // Reset slave

	outb(0x20, 0x20); // Reset master
}
