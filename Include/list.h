#ifndef LIST_H
#define LIST_H

typedef struct node {
	struct node *prev;
	struct node *next;

	void *data;
	void *owner;
} node_t;

typedef struct {
	node_t *head;
	node_t *tail;
} list_t;

list_t *list_create(void);
void list_destroy(list_t *list);
node_t *list_insert(list_t *list, void *data);
void list_remove(list_t *list, node_t *node);
node_t *list_find(list_t *list, void *key);
void list_merge(list_t *dest, list_t *src);

#define foreach(i, list) for (i = list->head; i != NULL; i = i->next)

#endif