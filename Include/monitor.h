/* monitor.h - function declarations for printing text */
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

#ifndef MONITOR_H
#define MONITOR_H
#include <common.h>

// VGA Text colors
#define BLACK		0
#define BLUE		1
#define GREEN		2
#define CYAN		3
#define RED			4
#define MAGENTA		5
#define BROWN		6
#define LGRAY		7
#define DGRAY		8
#define LBLUE		9
#define LGREEN		10
#define LCYAN		11
#define LRED		12
#define LMAGENTA	13
#define YELLOW		14
#define WHITE		15

void monitor_put(char c);
void monitor_clear(void);
void monitor_write(const char *s);
void monitor_write_hex(u32int n);
void monitor_write_udec(u32int n);
void monitor_write_sdec(s32int n);
void setcolor(u8int bg, u8int fg);

#endif
