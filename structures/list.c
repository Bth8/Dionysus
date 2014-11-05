#include <common.h>
#include <structures/list.h>
#include <kmalloc.h>

list_t *list_create(void) {
	list_t *list = (list_t *)kmalloc(sizeof(list_t));
	if (!list)
		return NULL;
	list->head = NULL;
	list->tail = NULL;
	return list;
}

// Frees contents of list followed by the list itself
void list_destroy(list_t *list) {
	node_t *node = list->head;

	while(node) {
		node_t *next = node->next;

		if (node->data)
			kfree(node->data);
		kfree(node);

		node = next;
	}

	kfree(list);
}

node_t *list_insert(list_t *list, void *data) {
	node_t *node = (node_t *)kmalloc(sizeof(node_t));
	if (!node)
		return NULL;

	node->data = data;
	node->owner = list;
	node->next = NULL;

	if (!list->head) {
		node->prev = NULL;
		list->head = node;
		list->tail = node;
		return node;
	}

	list->tail->next = node;
	node->prev = list->tail;
	list->tail = node;
	return node;
}

void list_remove(list_t *list, node_t *node) {
	ASSERT(node->owner == list);

	if (list->head == node)
		list->head = node->next;
	if (list->tail == node)
		list->tail = node->prev;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	if (node->data)
		kfree(node->data);
	kfree(node);
}

node_t *list_find(list_t *list, void *key) {
	node_t *element;
	foreach(element, list) {
		if (element->data == key)
			return element;
	}

	return NULL;
}

// appends src onto dest
void list_merge(list_t *dest, list_t *src) {
	node_t *node;
	foreach(node, src)
		node->owner = dest;

	if (!dest->head) {
		dest->head = src->head;
		dest->tail = src->tail;
	} else {
		src->head->prev = dest->tail;
		dest->tail = src->tail;
	}

	src->head = NULL;
	src->tail = NULL;
}