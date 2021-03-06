/* monitor.h - function declarations for printing text */

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

#define CURSOR_REG	0x03D4
#define CURSOR_SEL	0x03D5

#define CURSOR_LOC_HI	0x0E
#define CURSOR_LOC_LO	0x0F

void monitor_put(char c);
void monitor_clear(void);
void setcolor(uint8_t bg, uint8_t fg);

#endif /* MONITOR_H */
