/* ordered_array.c - Contains implementation of binary min-heap */
/* Copyright (C) 2011, 2012 Bth8 <bth8fwd@gmail.com>
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
#include <ordered_array.h>
#include <kmalloc.h>
#include <string.h>

u32int std_lessthan(type_t a, type_t b) {
	return (a < b) ? 1 : 0;
}

ordered_array_t create_ordered_array(u32int max_size, lessthan_predicate_t less) {
	ordered_array_t ret;
	ret.array = (void *)kmalloc(max_size * sizeof(type_t));
	memset(ret.array, 0, max_size * sizeof(type_t));
	ret.size = 0;
	ret.max_size = max_size;
	ret.less_than = less;
	return ret;
}

ordered_array_t place_ordered_array(void *addr, u32int max_size, lessthan_predicate_t less) {
	ordered_array_t ret;
	ret.array = (type_t *)addr;
	memset(ret.array, 0, max_size * sizeof(type_t));
	ret.size = 0;
	ret.max_size = max_size;
	ret.less_than = less;
	return ret;
}

void destroy_ordered_array(ordered_array_t *array) {
	kfree(array);
}

void insert_ordered_array(type_t item, ordered_array_t *array) {
	ASSERT(array->less_than);
	u32int i = array->size++;
	array->array[i] = item;
	type_t tmp;
	// If item is greater than its parent, swap them
	for (; i && array->less_than(array->array[i], array->array[(i-1)/2]); i = (i-1)/2) {
		tmp = array->array[i];
		array->array[i] = array->array[(i-1)/2];
		array->array[(i-1)/2] = tmp;
	}
}

type_t lookup_ordered_array(u32int i, ordered_array_t *array) {
	ASSERT(i < array->size);
	return array->array[i];
}

void remove_ordered_array(u32int i, ordered_array_t *array) {
	u32int i_cache = i;
	// Sanity checks
	ASSERT(i < array->size);
	ASSERT(array->less_than);
	if (--array->size && i < array->size) {
		array->array[i] = array->array[array->size];
		// if new value is less than its parent's, swap
		type_t tmp;
		for (; i && array->less_than(array->array[i], array->array[(i-1)/2]); i = (i-1)/2) {
			tmp = array->array[i];
			array->array[i] = array->array[(i-1)/2];
			array->array[(i-1)/2] = tmp;
		}
		i = i_cache;
		// If one or both children are less than new value, swap with least
		while ((2*i+1 < array->size) && (array->less_than(array->array[2*i+1], array->array[i]) || (2*i+2 < array->size) ? array->less_than(array->array[2*i+2], array->array[i]) : 0)) {
			u32int swap = (2*i+2 < array->size) ? (array->less_than(array->array[2*i+1], array->array[2*i+2]) ? 2*i+1 : 2*i+2) : 2*i+1;
			tmp = array->array[i];
			array->array[i] = array->array[swap];
			array->array[swap] = tmp;
			i = swap;
		}
	}
}
