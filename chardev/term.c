/* term.c - initializes and handles terminal stuff */
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

#include <chardev/term.h>
#include <common.h>
#include <monitor.h>
#include <idt.h>
#include <vfs.h>
#include <timer.h>
#include <dev.h>

#define ESCAPE 0x01
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

char noshiftmap[128] = {0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\x08',
					'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
					'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
					'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '};
char shiftmap[128] = {0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\x08',
					'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
					'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
					'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, 0, ' '};

int caps_stat = 0, shift_stat = 0, alt_stat = 0, ctrl_stat = 0;
u8int leds = 0;
char inbuf[BUFSIZE];
char *readbufpos = inbuf;
char *writebufpos = inbuf;

static void update_leds(u8int stat) {
	while (inb(0x64) & 2) {}	// Loop until keyboard buffer is 0
	outb(0x60, 0xED);

	while (inb(0x64) & 2) {}	// Loop again
	outb(0x60, stat);
}

static void kbd_isr(registers_t *regs) {
	regs = regs;
	char trans_code;
	u8int scode = inb(0x60), oldleds = leds;

	if (scode & 0x80) {			// Key released
		scode &= ~(0x80);		// Get base key
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
					monitor_put(trans_code);
				}
				break;
		}

	if (leds != oldleds)
		update_leds(leds);
}

static u32int read(struct fs_node *node, char *dest, u32int count, u32int off) {
	node = node;						// Compiler complains otherwise
	off = off;
	while (readbufpos == writebufpos)	// No characters have yet to be read
		sleep_thread();					// Put the thread to sleep, wait for something to come along

	u32int i;
	for (i = 0; i < count; i++) {
		while (readbufpos == writebufpos)	// If we've reached the end of characters yet to be written
			sleep_thread();					// and have yet to reach our quota, sleep
		*dest++ = *readbufpos++;
		if (readbufpos == inbuf + BUFSIZE)	// Circle around when we reach the end
			readbufpos = inbuf;
	}
	return i;
}

static u32int write(struct fs_node *node, const char *src, u32int count, u32int off) {
	node = node;
	off = off;
	u32int i;
	for (i = 0; i < count; i++)
		monitor_put(*src++);

	return i;
}

void init_term(void) {
	register_interrupt_handler(IRQ1, kbd_isr);
	static struct file_ops fops;
	fops.read = read;
	fops.write = write;
	register_chrdev(1, "tty", fops);
}
