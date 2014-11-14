/* tree.c - tree creation/deletion/management */

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
#include <structures/tree.h>
#include <kmalloc.h>
#include <structures/list.h>

tree_t *tree_create(void) {
	tree_t *tree = (tree_t *)kmalloc(sizeof(tree_t));
	tree->root = NULL;
	return tree;
}

// Frees the node, not the data
void tree_delete_node(tree_node_t *node) {
	if (!node)
		return;
	ASSERT(!node->parent);
	ASSERT(!node->owner);
	ASSERT(!node->children->head);
	list_destroy(node->children);
	kfree(node);
}

// Recursively frees node and all children
void tree_delete_branch(tree_t *tree, tree_node_t *node) {
	if (!tree || !node)
		return;
	ASSERT(node->owner == tree);

	// We can't use foreach because the child deletes itself from our list
	node_t *child = node->children->head;
	node_t *next_child;
	while (child) {
		next_child = child->next;
		tree_delete_branch(tree, child->data);
		child = next_child;
	}

	list_destroy(node->children);

	if (node->parent) {
		node_t *parent_ptr = list_find(node->parent->children, node);
		list_remove(node->parent->children, parent_ptr);
	} else
		tree->root = NULL;

	if (node->data)
		kfree(node->data);
	kfree(node);
}

tree_node_t *tree_detach_branch(tree_t *tree, tree_node_t *node) {
	if (!tree || !node)
		return NULL;
	ASSERT(node->owner == tree);

	if (!node->parent) {
		tree->root = NULL;
		return node;
	}

	node_t *parent_ptr = list_find(node->parent->children, node);
	list_remove(node->parent->children, parent_ptr);

	node->parent = NULL;
	node->owner = NULL;
	return node;
}

void tree_destroy(tree_t *tree) {
	if (!tree)
		return;
	tree_delete_branch(tree, tree->root);
	kfree(tree);
}

tree_node_t *tree_set_root(tree_t *tree, void *data) {
	if (!tree || !data)
		return NULL;

	tree_node_t *node = (tree_node_t *)kmalloc(sizeof(tree_node_t));
	if (!node)
		return NULL;
	node->children = list_create();
	if (!node->children) {
		kfree(node);
		return NULL;
	}
	node->data = data;
	node->parent = NULL;
	node->owner = tree;
	tree->root = node;
	return node;
}

tree_node_t *tree_insert_node(tree_t *tree, tree_node_t *parent, void *data) {
	if (!tree || !parent)
		return NULL;
	ASSERT(parent->owner == tree);
	tree_node_t *node = (tree_node_t *)kmalloc(sizeof(tree_node_t));
	if (!node)
		return NULL;
	node->children = list_create();
	if (!node->children) {
		kfree(node);
		return NULL;
	}
	node->data = data;
	node->parent = parent;
	node->owner = tree;

	list_insert(parent->children, node);
	return node;
}

tree_node_t *tree_insert_direct(tree_t *tree, tree_node_t *parent,
		tree_node_t *node) {
	if (!tree || !parent || !node)
		return NULL;
	ASSERT(parent->owner == tree);
	ASSERT(!node->owner); // No double inserts
	node->parent = parent;
	node->owner = tree;
	list_insert(parent->children, node);

	return node;
}

void tree_inherit_children(tree_t *tree, tree_node_t *newparent,
	tree_node_t *oldparent) {
	if (!tree || !newparent || !oldparent)
		return;

	ASSERT(newparent->owner == tree);
	list_merge(newparent->children, oldparent->children);
}