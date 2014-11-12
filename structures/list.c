/* list.c - list management functions */

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
	if (!list)
		return;

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
	if (!list)
		return NULL;

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
	if (!list_dequeue(list, node))
		return;

	if (node->data)
		kfree(node->data);
	kfree(node);
}

node_t *list_dequeue(list_t *list, node_t *node) {
	if (!list || !node)
		return NULL;

	ASSERT(node->owner == list);

	if (list->head == node)
		list->head = node->next;
	if (list->tail == node)
		list->tail = node->prev;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	return node;
}

node_t *list_find(list_t *list, void *key) {
	if (!list || !key)
		return NULL;

	node_t *element;
	foreach(element, list) {
		if (element->data == key)
			return element;
	}

	return NULL;
}

node_t *list_get_index(list_t *list, uint32_t index) {
	if (!list)
		return NULL;

	node_t *node = list->head;
	uint32_t i;
	for (i = 0; i < index; i++) {
		if (!node)
			return NULL;
		node = node->next;
	}

	return node;
}

// appends src onto dest
void list_merge(list_t *dest, list_t *src) {
	if (!dest || !src)
		return;

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