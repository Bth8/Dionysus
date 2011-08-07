/* idt.h - data types and declarations and definitions for IDT setup and interrupt handling */
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

#ifndef IDT_H
#define IDT_H
#include <common.h>

#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47


typedef struct idt_entry_struct {
	u16int	handler_low;							// Lower 16 bits of the handler address
	u16int	selector;								// Code segment selector
	u8int	zero;									// Must be zero
	u8int	flags;									// Flags. See Intel documentation
	u16int	handler_high;							// High 16 bits of the handler address
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr_struct {
	u16int offset;									// Offset (from base) of last entry in table
	u32int base;									// Address of first entry
} __attribute__((packed)) idt_ptr_t;

typedef struct registers {
	u32int ds;										// Data segment
	u32int edi, esi, ebp, esp, ebx, edx, ecx, eax;	// Pushed by pushad
	u32int int_no, err_code;
	u32int eip, cs, eflags, useresp, ss;			// Pushed automatically by processor upon interrupt
} registers_t;

typedef void (*isr_t)(registers_t);

void init_idt();
void register_interrupt_handler(u8int n, isr_t handler);

// Interrupts. Define in interrupt.s
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

extern void isr128();

#endif
