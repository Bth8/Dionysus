/* ordered_array.h - binary min heap and function declarations for
 * management
 */

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

#ifndef ORDERED_ARRAY_H
#define ORDERED_ARRAY_H
#include <common.h>

typedef void *type_t;
// Predicate should return nonzero if first arg is less than second
// Zero otherwise
typedef uint32_t (*lessthan_predicate_t)(type_t, type_t);
typedef struct ordered_array {
	type_t *array;
	uint32_t size;
	uint32_t max_size;
	lessthan_predicate_t less_than;
} ordered_array_t;

uint32_t std_lessthan(type_t a, type_t b);
ordered_array_t create_ordered_array(uint32_t max_size,
		lessthan_predicate_t less);
ordered_array_t place_ordered_array(void *addr, uint32_t max_size,
		lessthan_predicate_t less);
void destroy_ordered_array(ordered_array_t *array);
void insert_ordered_array(type_t item, ordered_array_t *array);
type_t lookup_ordered_array(uint32_t i, ordered_array_t *array);
void remove_ordered_array(uint32_t i, ordered_array_t *array);

#endif /* ORDERED_ARRAY_H */
