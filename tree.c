#include <common.h>
#include <tree.h>
#include <kmalloc.h>
#include <list.h>

tree_t *tree_create(void) {
	tree_t *tree = (tree_t *)kmalloc(sizeof(tree_t));
	tree->root = NULL;
	return tree;
}

/* Deletes node. Orphans go to parent
void tree_delete_node(tree_t *tree, tree_node_t *node) {
	ASSERT(node->owner == tree);

	node_t *child;
	foreach(child, node->children)
		((tree_node_t *)(child->data))->parent = node->parent;

	list_merge(node->parent->children, node->children);
	list_destroy(node->children);

	if (node->data)
		kfree(node->data);
	kfree(node);
} */

// Recursively frees node and all children
void tree_delete_branch(tree_t *tree, tree_node_t *node) {
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
	}

	if (node->data)
		kfree(node->data);
	kfree(node);
}

void tree_destroy(tree_t *tree) {
	tree_delete_branch(tree, tree->root);
	kfree(tree);
}

tree_node_t *tree_set_root(tree_t *tree, void *data) {
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
	return node;
}

tree_node_t *tree_insert_node(tree_t *tree, tree_node_t *parent, void *data) {
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