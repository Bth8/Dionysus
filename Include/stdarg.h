/* stdarg.h - header for va_args */

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

#ifndef STDARG_H
#define STDARG_H

#define va_start(v, l)	__builtin_va_start(v, l)
#define va_arg(v, l)	__builtin_va_arg(v, l)
#define va_end(v)	__builtin_va_end(v)
#define va_copy(v, l)	__builtin_va_copy(v, l)
typedef __builtin_va_list va_list;

#endif /* STDARG_H */
