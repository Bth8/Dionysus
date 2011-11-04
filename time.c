/* time.c - get and play with time. Bits borrowed from minix source and linux v0.01 */
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

#include <common.h>
#include <time.h>
#include <monitor.h>

#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)
#define YEAR0 1900
#define EPOCH_YR 1970
#define LEAPYEAR(year) (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year) (LEAPYEAR(year) ? 366 : 365)

time_t start_time = 0, current_time = 0;

// We assume leap years
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

static unsigned int _ytab[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30}
};

time_t mktime(struct tm *tp) {
	u32int res;
	int year;

	year = tp->tm_year - 70;
	// Offsets needed to get leap years right
	res = YEAR * year + DAY * ((year+1)/4);
	res += month[tp->tm_mon];
	// Correct if not a leap year
	if (tp->tm_mon > 1 && (year+2)%4)
		res -= DAY;
	res += DAY * (tp->tm_mday-1);
	res += HOUR * tp->tm_hour;
	res += MINUTE * tp->tm_min;
	res += tp->tm_sec;
	return res;
}

struct tm *gmtime(const time_t *timer) {
	static struct tm ret;
	struct tm *retp = &ret;
	time_t time = *timer;
	time_t clock = time % DAY;
	time_t dayno = time / DAY;
	int year = EPOCH_YR;

	retp->tm_sec = clock % MINUTE;
	retp->tm_min = (clock % HOUR) / MINUTE;
	retp->tm_hour = clock / HOUR;
	retp->tm_wday = (dayno + 4) % 7;	// Epoch was a thursday
	while (dayno >= YEARSIZE(year)) {
		dayno -= YEARSIZE(year);
		year++;
	}
	retp->tm_year -= YEAR0;
	retp->tm_yday = dayno;
	retp->tm_mon = 0;
	while (dayno >= _ytab[LEAPYEAR(year)][retp->tm_mon]) {
		dayno -= _ytab[LEAPYEAR(year)][retp->tm_mon];
		retp->tm_mon++;
	}
	retp->tm_mday = dayno + 1;
	retp->tm_isdst = 0;

	return retp;
}

void init_time(void) {
	struct tm time;
	int pm, century;

	do {
		time.tm_sec = READ_CMOS(0);
		time.tm_min = READ_CMOS(2);
		time.tm_hour = READ_CMOS(4);
		time.tm_mday = READ_CMOS(7);
		time.tm_mon = READ_CMOS(8)-1;
		time.tm_year = READ_CMOS(9);
		century = READ_CMOS(0x32);
	} while (time.tm_sec != READ_CMOS(0));

	if (!(READ_CMOS(0xB) & 2)) {
		pm = time.tm_hour & 0x80;
		time.tm_hour &= ~0x80;
		if (time.tm_hour == 0x12 || time.tm_hour == 12)
			time.tm_hour = 0;
		time.tm_hour += pm ? ((READ_CMOS(0xB) & 4) ? 12 : 0x12) : 0;
	}

	// BCD mode
	if (!(READ_CMOS(0xB) & 4)) {
		BCD2HEX(time.tm_sec);
		BCD2HEX(time.tm_min);
		BCD2HEX(time.tm_hour);
		BCD2HEX(time.tm_mday);
		BCD2HEX(time.tm_mon);
		BCD2HEX(time.tm_year);
		BCD2HEX(century);
	}

	century = (century * 100) - 1900;
	time.tm_year += century;

	current_time = start_time = mktime(&time);
}
