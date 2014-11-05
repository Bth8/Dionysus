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