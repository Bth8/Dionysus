/* time.h - Header for time data types/functions */

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

#ifndef TIME_H
#define TIME_H

#define CMOS_REGISTER	0x70
#define CMOS_SELECTED	0x71

#define CMOS_RTC_DNMI	0x80
#define CMOS_RTC_SEC	0x00
#define CMOS_RTC_MIN	0x02
#define CMOS_RTC_HOUR	0x04
#define CMOS_RTC_DOW	0x06
#define CMOS_RTC_DOM	0x07
#define CMOS_RTC_MONTH	0x08
#define CMOS_RTC_YEAR	0x09
#define CMOS_RTC_CENT	0x32
#define CMOS_RTC_STAT_A	0x0A
#define CMOS_RTC_STAT_B	0x0B
#define CMOS_RTC_STAT_C	0x0C

#define CMOS_RTC_24HR	0x02
#define CMOS_RTC_BIN	0X04
#define CMOS_RTC_INT	0x40

#define READ_CMOS(addr) ({ \
		outb(CMOS_REGISTER, CMOS_RTC_DNMI | addr); \
		inb(CMOS_SELECTED); \
})
#define WRITE_CMOS(addr, val) ({ \
		outb(CMOS_REGISTER, CMOS_RTC_DNMI | addr); \
		outb(CMOS_SELECTED, val); \
})

#ifndef TIME_T
#define TIME_T
typedef unsigned int time_t;
#endif /* TIME_T */

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

time_t mktime(struct tm *tp);
struct tm *gmtime(const time_t *timer);
time_t time(time_t *timer);
void init_time(void);

#endif /* TIME_H */
