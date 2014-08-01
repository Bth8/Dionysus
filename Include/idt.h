/* idt.h - data types and declarations and definitions for IDT setup and
 * interrupt handling */

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
	u16int	handler_low; // Lower 16 bits of the handler address
	u16int	selector; // Code segment selector
	u8int	zero; // Must be zero
	u8int	flags; // Flags. See Intel documentation
	u16int	handler_high; // High 16 bits of the handler address
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr_struct {
	u16int offset; // Offset (from base) of last entry in table
	u32int base; // Address of first entry
} __attribute__((packed)) idt_ptr_t;

typedef struct registers {
	u32int ds; // Data segment
	u32int edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pushad
	u32int int_no, err_code;
	u32int eip, cs, eflags, useresp, ss; // Pushed automatically by processor
										 // upon interrupt
} registers_t;

typedef void (*isr_t)(registers_t*);

void init_idt(void);
void register_interrupt_handler(u8int n, isr_t handler);

// Interrupts. Define in interrupt.s
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

extern void isr128(void);

#endif /* IDT_H */
