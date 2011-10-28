#ifndef DEV_H
#define DEV_H
#include <common.h>
#include <vfs.h>

#define MAJOR(x) ((x >> 24) & 0xFF)
#define MINOR(x) (x & 0xFFFFFF)

struct chrdev_driver {
	const char *name;
	struct file_ops ops;
};

struct dev_file {
	fs_node_t node;
	struct dev_file *next;	// Linked list, y'all
};

void devfs_init(void);
u32int devfs_register(const char *name, u32int flags, u32int major,
					u32int minor, u32int mode, u32int uid, u32int gid);
u32int register_chrdev(u32int major, const char *name, struct file_ops fops);

#endif
