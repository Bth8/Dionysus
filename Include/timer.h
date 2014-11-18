/* timer.h - declarations for initialization of timer */

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

#ifndef TIMER_H
#define TIMER_H
#include <common.h>

#define PIT_PORT0	0x40
#define PIT_PORT1	0x41
#define PIT_PORT2	0x42
#define PIT_CMD		0x43

#define PIT_CHAN0	0x00
#define PIT_CHAN1	0x40
#define PIT_CHAN2	0x80
#define PIT_READBCK	0xC0
#define PIT_LATCH	0x00
#define PIT_LOW		0x10
#define PIT_HIGH	0x20
#define PIT_LOHI	0x30
#define PIT_MODE0	0x00
#define PIT_MODE1	0x02
#define PIT_MODE2	0x04
#define PIT_MODE3	0x06
#define PIT_MODE4	0x08
#define PIT_MODE5	0x0C
#define PIT_BIN		0x00
#define PIT_BCD		0x01

#define HZ			1000

struct timer {
	void (*callback)(void);
	uint32_t expires;
};

void init_timer(void);
void wait(uint32_t ms);
void sleep_thread(void);
int add_timer(struct timer *timer);
void del_timer(struct timer *timer);

#endif /* TIMER_H */
