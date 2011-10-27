#ifndef DEV_H
#define DEV_H
#include <common.h>
#include <vfs.h>

struct dev_file {
	fs_node_t node;
	struct dev_file *next;	// Linked list, y'all
};

s32int register_chrdev(u32int major, const char *name, struct file_ops *fops);

#endif
