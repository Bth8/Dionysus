/* term.c - initializes and handles terminal stuff */

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

#include <chardev/term.h>
#include <common.h>
#include <monitor.h>
#include <idt.h>
#include <vfs.h>
#include <char.h>
#include <errno.h>
#include <task.h>

#define ESCAPE 0x01
#define BACKSPACE 0x0E
#define CTRL 0x1D
#define LSHIFT 0x2A
#define RSHIFT 0x36
#define ALT 0x38
#define CAPS 0x3A
#define NUMLOCK 0x45
#define SCRLLOCK 0x46
#define SCROLL_LED 1
#define NUM_LED 2
#define CAPS_LED 4

#define BUFSIZE 1024

char noshiftmap[128] =
	{0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
	'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '};

char shiftmap[128] =
	{0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
	'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, 0, ' '};

int caps_stat = 0, shift_stat = 0, alt_stat = 0, ctrl_stat = 0;
uint8_t leds = 0;
char inbuf[BUFSIZE];
char *readbufpos = inbuf;
char *writebufpos = inbuf;
int echo = 1;

waitqueue_t *term_wq = NULL;

static void update_leds(uint8_t stat) {
	// Spin until keyboard buffer is 0
	while (inb(0x64) & 2) {}
	outb(0x60, 0xED);

	while (inb(0x64) & 2) {}
	outb(0x60, stat);
}

static void kbd_isr(registers_t *regs) {
	char trans_code;
	uint8_t scode = inb(0x60), oldleds = leds;

	if (scode & 0x80) { // Key released
		// Get base key
		scode &= ~(0x80);
		switch (scode) {
		case LSHIFT:
		case RSHIFT:
			shift_stat = 0;
			break;
		case ALT:
			alt_stat = 0;
			break;
		case CTRL:
			ctrl_stat = 0;
			break;
		default:
			break;
		}
	} else
		switch (scode) {
		case LSHIFT:
		case RSHIFT:
			shift_stat = 1;
			break;
		case ALT:
			alt_stat = 1;
			break;
		case CTRL:
			ctrl_stat = 1;
			break;
		case NUMLOCK:
			leds ^= NUM_LED;
			break;
		case SCRLLOCK:
			leds ^= SCROLL_LED;
			break;
		case CAPS:
			leds ^= CAPS_LED;
			caps_stat = !caps_stat;
			break;
		case BACKSPACE:
			if (readbufpos == writebufpos)
				break;
			writebufpos--;
			monitor_put('\x08'); // Backspace
			break;
		default:
			trans_code = (shift_stat ? shiftmap : noshiftmap)[scode];
			if (trans_code) {
				if (caps_stat) {
					if (trans_code >= 'A' && trans_code <= 'Z')
						trans_code += 0x20;	// Makes lowercase
					else if (trans_code >= 'a' && trans_code <= 'z')
						trans_code -= 0x20;	// Makes uppercase
				}
				*writebufpos++ = trans_code;
				if (writebufpos == inbuf + BUFSIZE)
					writebufpos = inbuf;
				if (writebufpos == readbufpos)
					readbufpos++;
				if (echo)
					monitor_put(trans_code);
				wake_queue(term_wq);
			}
			break;
		}

	if (leds != oldleds)
		update_leds(leds);

	irq_ack(regs->int_no);
}

static ssize_t read(struct fs_node *node, void *dest, size_t count,
		off_t off) {
	// Block if we're up to date
	if (readbufpos == writebufpos)
		wait_event_interruptable(term_wq, readbufpos != writebufpos);

	uint32_t i;
	for (i = 0; i < count; i++) {
		if (readbufpos == writebufpos)
			return i;
		*(char *)dest++ = *readbufpos++;
		// Circular buffer y'all
		if (readbufpos == inbuf + BUFSIZE)
			readbufpos = inbuf;
	}
	return i;
}

static ssize_t write(struct fs_node *node, const void *src, size_t count,
		off_t off) {
	size_t i;
	for (i = 0; i < count; i++)
		monitor_put(*(char *)src++);

	return i;
}

static int32_t ioctl(struct fs_node *node, uint32_t req, void *ptr) {
	int ret;
	switch (req) {
	case TERMIOECHO:
		if (ptr) {
			if (*(int*)ptr)
				echo = 1;
			else
				echo = 0;
			ret = 0;
		} else
			ret = -EFAULT;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

struct file_ops fops = {
	.read = read,
	.write = write,
	.ioctl = ioctl
};

void init_term(void) {
	term_wq = create_waitqueue();
	ASSERT(term_wq);
	ASSERT(register_interrupt_handler(IRQ1, kbd_isr) == 0);
	update_leds(leds);
	register_chrdev(TERM_MAJOR, "tty", fops);
}
