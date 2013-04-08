/* monitor.c - code for writing to the monitor */
/* Copyright (C) 2011-2013 Bth8 <bth8fwd@gmail.com>
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
#include <monitor.h>
#include <string.h>

u8int attrByte = (BLACK << 4) | (WHITE & 0x0F), cursor_x = 0, cursor_y = 0;
u16int *vmem = (u16int *)0x0B8000;

void setcolor(u8int bg, u8int fg) {
	attrByte = (bg << 4) | (fg & 0x0F);
}

static void update_cursor(void) {
	u16int cursorLocation = cursor_y * 80 + cursor_x;
	outb(0x03D4, 14);					// Setting high cursor byte
	outb(0x03D5, cursorLocation >> 8);
	outb(0x03D4, 15);					// Setting low cursor byte
	outb(0x03D5, cursorLocation);
}

void move_cursor(u32int x, u32int y) {
	cursor_x = x;
	cursor_y = y;
	update_cursor();
}

static void scroll(void) {
	u16int blank = 0x20 | (attrByte << 8);

	if (cursor_y > 24) {
		int i;
		// Move every character up one line
		for (i = 0; i < 25 * 80; i++)
			vmem[i] = vmem[i+80];
		// Blank last line
		for (i = 24 * 80; i < 26 * 80; i++);
			vmem[i] = blank;
		// Set cursor to last line
		cursor_y = 24;
	}
}

// Print 1 character
void monitor_put(char c) {
	u16int *location = vmem + (cursor_y*80 + cursor_x);

	// Backspace
	if (c == 0x08 && (cursor_x | cursor_y)) {
		// Blank the space before the cursor
		location--;
		*location = 0x20 | (attrByte << 8);
		// Move the cursor back one
		if (cursor_x)
			cursor_x--;
	}
	// Tab by increasing cursor_x to a point where it is divisible by 8
	else if (c == '\t')
		cursor_x = (cursor_x + 8) & ~(8 - 1);
	// Carriage return
	else if (c == '\r')
		cursor_x = 0;
	// Newline
	else if (c == '\n') {
		cursor_x = 0;
		cursor_y++;
	}
	// All other printable characters
	else if (c >= ' ') {
		*location = c | (attrByte << 8);
		cursor_x++;
	}

	// Insert a new line if we've reached the end of the current one
	if (cursor_x >= 80) {
		cursor_x = 0;
		cursor_y++;
	}

	// Scroll if necessary
	scroll();
	// Update the cursor
	update_cursor();
}

void monitor_clear(void) {
	u16int blank = 0x20 | (attrByte << 8);

	int i;
	for (i = 0; i < 80*25; i++)
		vmem[i] = blank;

	cursor_x = 0;
	cursor_y = 0;
	update_cursor();
}
