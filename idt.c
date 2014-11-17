/* idt.c - Code for setting up the IDT, registering ISRs, and
 * managing the PIC
 */

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

#include <idt.h>
#include <common.h>
#include <printf.h>

// Flush IDT. Defined in descriptor_tables.s
extern void idt_flush(uint32_t);

idt_entry_t idt_entries[256];
idt_ptr_t idt_ptr;

isr_t isr_handlers[256];

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags) {
	idt_entries[num].handler_low = handler & 0xFFFF;
	idt_entries[num].handler_high = handler >> 16;

	idt_entries[num].selector = sel;
	idt_entries[num].zero = 0;
	idt_entries[num].flags = flags;
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
	isr_handlers[n] = handler;
}

void init_idt(void) {
	idt_ptr.offset = (sizeof(idt_entry_t) * 256) - 1;
	idt_ptr.base = (uint32_t)&idt_entries;

	// Initialize 8259A
	outb(PIC_MASTER_A, PIC_COMMAND_START | PIC_ICW1_ICW4); 
	outb(PIC_SLAVE_A, PIC_COMMAND_START | PIC_ICW1_ICW4);
	outb(PIC_MASTER_B, 32);
	outb(PIC_SLAVE_B, 40);
	outb(PIC_MASTER_B, 0x04); 
	outb(PIC_SLAVE_B, 0x02);
	outb(PIC_MASTER_B, PIC_ICW4_MASTER | PIC_ICW4_8086);
	outb(PIC_SLAVE_B, PIC_ICW4_8086);

	// Base flags
	uint8_t flags = IDT_ENTRY_INTERRUPT | IDT_ENTRY_32BIT | IDT_ENTRY_PRESENT;
	idt_set_gate(0, (uint32_t)isr0, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(1, (uint32_t)isr1, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(2, (uint32_t)isr2, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(3, (uint32_t)isr3, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(4, (uint32_t)isr4, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(5, (uint32_t)isr5, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(6, (uint32_t)isr6, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(7, (uint32_t)isr7, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(8, (uint32_t)isr8, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(9, (uint32_t)isr9, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(10, (uint32_t)isr10, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(11, (uint32_t)isr11, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(12, (uint32_t)isr12, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(13, (uint32_t)isr13, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(14, (uint32_t)isr14, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(15, (uint32_t)isr15, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(16, (uint32_t)isr16, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(17, (uint32_t)isr17, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(18, (uint32_t)isr18, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(19, (uint32_t)isr19, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(20, (uint32_t)isr20, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(21, (uint32_t)isr21, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(22, (uint32_t)isr22, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(23, (uint32_t)isr23, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(24, (uint32_t)isr24, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(25, (uint32_t)isr25, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(26, (uint32_t)isr26, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(27, (uint32_t)isr27, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(28, (uint32_t)isr28, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(29, (uint32_t)isr29, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(30, (uint32_t)isr30, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(31, (uint32_t)isr31, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(32, (uint32_t)irq0, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(33, (uint32_t)irq1, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(34, (uint32_t)irq2, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(35, (uint32_t)irq3, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(36, (uint32_t)irq4, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(37, (uint32_t)irq5, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(38, (uint32_t)irq6, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(39, (uint32_t)irq7, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(40, (uint32_t)irq8, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(41, (uint32_t)irq9, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(42, (uint32_t)irq10, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(43, (uint32_t)irq11, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(44, (uint32_t)irq12, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(45, (uint32_t)irq13, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(46, (uint32_t)irq14, 0x08, flags | IDT_DPL_RING0);
	idt_set_gate(47, (uint32_t)irq15, 0x08, flags | IDT_DPL_RING0);

	idt_set_gate(128, (uint32_t)isr128, 0x08, flags | IDT_DPL_RING3);

	idt_flush((uint32_t)&idt_ptr);
}

void isr_handler(registers_t *regs) {
	isr_t handler;
	if ((handler = isr_handlers[regs->int_no]) != NULL)
		handler(regs);
	else {
		printf("Unhandled interrupt: %i\nErr code: %i\n", regs->int_no,
			regs->err_code);
		halt();
	}
}

void irq_handler(registers_t *regs) {
	isr_t handler;
	if ((handler = isr_handlers[regs->int_no]) != NULL)
		handler(regs);

	if (regs->int_no >= IRQ8) // Signal came from slave
		outb(PIC_SLAVE_A, PIC_COMMAND_EOI); // Reset slave

	outb(PIC_MASTER_A, PIC_COMMAND_EOI); // Reset master
}
