/* hashmap.h - hashmaps. Strings as keys only */

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

 #ifndef HASHMAP_H
#define HASHMAP_H

#include <common.h>

typedef uint32_t (*hash_t)(const char*);

typedef struct keyvalue {
	char *key;
	void *value;
	struct keyvalue *next;
} kv_t;

typedef struct {
	kv_t **array;
	hash_t hash;
	uint32_t n;
} hashmap_t;

// Note: keys must be strings. Key management is our responsibility.
// Value management is not.

hashmap_t *hashmap_create(uint32_t n, hash_t hash);
void hashmap_destroy(hashmap_t *hashmap);
int hashmap_insert(hashmap_t *hashmap, const char *key, void *value);
void *hashmap_find(hashmap_t *hashmap, const char *key);
void *hashmap_remove(hashmap_t *hashmap, const char *key);

#endif /* HASHMAP_H */