#include <common.h>
#include <vfs.h>
#include <dev.h>
#include <string.h>

fs_node_t dev_root;
struct dev_file *files = NULL;

static struct dirent *readdir(fs_node_t *node, u32int index);
static struct fs_node *finddir(fs_node_t *node, const char *name);

void dev_init() {
	strcpy(dev_root.name, "dev");
	dev_root.mask = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC | VFS_G_READ |
					VFS_G_EXEC | VFS_O_READ | VFS_O_EXEC;
	dev_root.gid = dev_root.uid = 0;
	dev_root.flags = VFS_DIR;
	dev_root.inode = 0;
	dev_root.len = 0;
	dev_root.impl = 0;
	dev_root.ops.readdir = readdir;
	dev_root.ops.finddir = finddir;
}

static struct dirent *readdir(fs_node_t *node, u32int index) {
	ASSERT(node == &dev_root);
	static struct dirent ret;
	u32int i;
	struct dev_file *filep = files;
	for (i = 0; i < index; i++) {
		if (filep == NULL)
			return NULL;
		else
			filep = filep->next;
	}
	ret.d_ino = i;
	strcpy(ret.d_name, filep->node.name);
	return &ret;
}

static struct fs_node *finddir(fs_node_t *node, const char *name) {
	ASSERT(node == &dev_root);
	struct dev_file *filep = files;
	while (strcmp(filep->node.name, name) != 0) {
		filep = filep->next;
		if (filep == NULL)
			return NULL;
	}
	return &filep->node;
}

s32int register_chrdev(u32int major, const char *name, struct file_ops *fops) {
	// TODO
}
