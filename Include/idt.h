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

// 8259A stuff. Read the Intel manuals. Kinda out of date, but it works
#define PIC_MASTER_A 0x20
#define PIC_SLAVE_A 0xA0
#define PIC_MASTER_B 0x21
#define PIC_SLAVE_B 0xA1

#define PIC_COMMAND_START 0x10
#define PIC_COMMAND_EOI 0x20

#define PIC_ICW1_ICW4 0x01
#define PIC_ICW1_MASTER_ONLY 0x02
#define PIC_ICW1_LEVEL 0x08

#define PIC_ICW4_8086 0x01
#define PIC_ICW4_AEOI 0x02
#define PIC_ICW4_MASTER 0x04
#define PIC_ICW4_BUF 0x08
#define PIC_ICW4_SFNM 0x10

// IDT entry numbers for IRQ, which here means a hardware interrupt
// caught by the 8059A
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

#define IDT_ENTRY_INTERRUPT 0x06
#define IDT_ENTRY_TRAP 0x07
#define IDT_ENTRY_32BIT 0x08
#define IDT_ENTRY_PRESENT 0x80

#define IDT_DPL_RING0 0x00
#define IDT_DPL_RING1 0x20
#define IDT_DPL_RING2 0x40
#define IDT_DPL_RING3 0x60

typedef struct idt_entry_struct {
	uint16_t	handler_low; // Lower 16 bits of the handler address
	uint16_t	selector; // Code segment selector
	uint8_t	zero; // Must be zero
	uint8_t	flags; // Flags. See Intel documentation
	uint16_t	handler_high; // High 16 bits of the handler address
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr_struct {
	uint16_t offset; // Offset (from base) of last entry in table
	uint32_t base; // Address of first entry
} __attribute__((packed)) idt_ptr_t;

typedef struct registers {
	uint32_t ds; // Data segment
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pushad
	uint32_t int_no, err_code;
	uint32_t eip, cs, eflags, useresp, ss; // Pushed automatically by processor
										 // upon interrupt
} registers_t;

typedef void (*isr_t)(registers_t*);

void init_idt(void);
int32_t register_interrupt_handler(uint8_t n, isr_t handler);
void irq_ack(uint32_t int_no);

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
