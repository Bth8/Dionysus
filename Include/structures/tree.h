/* tree.h - tree implementation */

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

#ifndef TREE_H
#define TREE_H

#include <structures/list.h>

typedef struct tree_node {
	void *data;
	list_t *children;
	struct tree_node *parent;
	void *owner;
} tree_node_t;

typedef struct {
	tree_node_t *root;
} tree_t;

tree_t *tree_create(void);
void tree_destroy(tree_t *tree);
tree_node_t *tree_set_root(tree_t *tree, void *data);
tree_node_t *tree_insert_node(tree_t *tree, tree_node_t *parent, void *data);
//void tree_delete_node(tree_t *tree, tree_node_t *node);
void tree_delete_branch(tree_t *tree, tree_node_t *node);

#endif