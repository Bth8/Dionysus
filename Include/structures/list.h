/* list.h - doubly-linked lists */

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
node_t *list_dequeue(list_t *list, node_t *node);
node_t *list_find(list_t *list, void *key);
node_t *list_get_index(list_t *list, uint32_t index);
void list_merge(list_t *dest, list_t *src);

#define foreach(i, list) for (i = list->head; i != NULL; i = i->next)

#endif