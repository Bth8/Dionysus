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