/* arithmetic.c - 64-bit arithmetic functions */
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

static inline u32int divl(u64int n, u32int d) {
	u32int n1 = n >> 32;
	u32int n0 = n;
	u32int q, r;

	asm("divl %4": "=d" (r), "=a" (q): "0" (n1), "1" (n0), "rm" (d));

	return q;
}

static inline u32int nlz(u32int n) {
	u32int ret;

	asm("bsr %1, %0": "=r" (ret): "r" (n));

	return ret;
}

static u64int u64div(u64int n, u64int d) {
	if ((d >> 32) == 0) {
		u64int b = 1ULL << 32;
		u32int n1 = n >> 32;
		u32int n0 = n;
		u32int d0 = d;

		return divl(b * (n1 % d0) + n0, d0) + b * (n1 / d0);
	} else {
		if (n < d)
			return 0;
		else {
			u32int d1 = d >> 32;
			int s = nlz(d1);
			u64int q = divl(n >> 1, (d << s) >> 32) >> (31 - s);
			return (n - (q - 1) * d) < d ? q - 1 : q;
		}
	}
}

static inline u32int u64mod(u64int n, u64int d) {
	return n - d * u64div(n, d);
}

static s64int s64div(s64int n, s64int d) {
	u64int n_abs = n >= 0 ? (u64int)n : -(u64int)n;
	u64int d_abs = d >= 0 ? (u64int)d : -(u64int)d;
	u64int q_abs = u64div(n_abs, d_abs);

	return ((n < 0) == (d < 0)) ? (s64int)q_abs : -(s64int)q_abs;
}

static inline s32int s64mod(s64int n, s64int d) {
	return n - d * s64div(n, d);
}

long long __divdi3(long long n, long long d) {
	return s64div(n, d);
}

long long __moddi3(long long n, long long d) {
	return s64mod(n, d);
}

unsigned long long __udivdi3(unsigned long long n, unsigned long long d) {
	return u64div(n, d);
}

unsigned long long __umoddi3(unsigned long long n, unsigned long long d) {
	return u64mod(n, d);
}
