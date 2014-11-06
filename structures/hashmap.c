/* hashmap.c - basic hashmap functions. Nothing special */

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

#include <common.h>
#include <structures/hashmap.h>
#include <string.h>
#include <errno.h>
#include <kmalloc.h>


// Taken from <http://www.cse.yorku.ca/~oz/hash.html>
static uint32_t djb2(const char *key) {
	ASSERT(key);

	uint32_t hash = 5381;

	uint32_t c;
	while ((c = *key++) != 0)
		hash = ((hash << 5) + hash) + c; // hash = 33 * hash + c

	return hash;
}

hashmap_t *hashmap_create(uint32_t n, hash_t hash) {
	if (n == 0)
		return NULL;

	hashmap_t *hashmap = (hashmap_t *)kmalloc(sizeof(hashmap_t));
	if (!hashmap)
		return NULL;

	hashmap->array = (kv_t **)kmalloc(sizeof(kv_t) * n);
	if (!hashmap->array) {
		kfree(hashmap);
		return NULL;
	}

	uint32_t i;
	for (i = 0; i < n; i++)
		hashmap->array[i] = NULL;

	hashmap->n = n;
	if (hash)
		hashmap->hash = hash;
	else
		hashmap->hash = djb2;

	return hashmap;
}

void hashmap_destroy(hashmap_t *hashmap) {
	if (!hashmap)
		return;

	uint32_t i;
	for (i = 0; i < hashmap->n; i++) {
		kv_t *x = hashmap->array[i];
		kv_t *p;
		while (x) {
			p = x;
			x = x->next;
			kfree(p->key);
			kfree(p);
		}
	}

	kfree(hashmap->array);
	kfree(hashmap);
}

int hashmap_insert(hashmap_t *hashmap, const char *key, void *value) {
	if (!hashmap || !key)
		return -EFAULT;

	char *key_cpy = (char *)kmalloc(strlen(key) + 1);
	if (!key_cpy)
		return -ENOMEM;

	strcpy(key_cpy, key);

	uint32_t i = hashmap->hash(key_cpy) % hashmap->n;

	if (!hashmap->array[i]) {
		kv_t *entry = (kv_t *)kmalloc(sizeof(kv_t));
		if (!entry) {
			kfree(key_cpy);
			return -ENOMEM;
		}

		entry->key = key_cpy;
		entry->value = value;
		entry->next = NULL;

		hashmap->array[i] = entry;
	} else {
		kv_t *x = hashmap->array[i];
		kv_t *p;
		while (x) {
			if (strcmp(key_cpy, x->key) == 0) {
				kfree(key_cpy);
				return -EEXIST;
			}
			p = x;
			x = x->next;
		}

		kv_t *entry = (kv_t *)kmalloc(sizeof(kv_t));
		if (!entry) {
			kfree(key_cpy);
			return -ENOMEM;
		}

		entry->key = key_cpy;
		entry->value = value;
		entry->next = NULL;
		p->next = entry;
	}

	return 0;
}

void *hashmap_find(hashmap_t *hashmap, const char *key) {
	if (!hashmap || !key)
		return NULL;

	uint32_t i = hashmap->hash(key) % hashmap->n;
	kv_t *x = hashmap->array[i];

	while (x) {
		if (strcmp(x->key, key) == 0) {
			return x->value;
		}
		x = x->next;
	}

	return NULL;
}

void *hashmap_remove(hashmap_t *hashmap, const char *key) {
	if (!hashmap || !key)
		return NULL;

	uint32_t i = hashmap->hash(key) % hashmap->n;
	kv_t *x = hashmap->array[i];
	kv_t *p = NULL;

	while (x) {
		if (strcmp(x->key, key) == 0) {
			void *value = x->value;
			kfree(x->key);
			if (p)
				p->next = x->next;
			else
				hashmap->array[i] = x->next;
			kfree(x);
			return value;
		}
		p = x;
		x = x->next;
	}

	return NULL;
}