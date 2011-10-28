#include <common.h>
#include <vfs.h>
#include <dev.h>
#include <string.h>
#include <kmalloc.h>

fs_node_t dev_root;
struct dev_file *files = NULL;
struct chrdev_driver drivers[256];		// 1 for every valid major number

struct file_ops ops_default = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static struct dirent *readdir(fs_node_t *node, u32int index);
static struct fs_node *finddir(fs_node_t *node, const char *name);

void devfs_init() {
	strcpy(dev_root.name, "dev");
	dev_root.mask = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC | VFS_G_READ |
					VFS_G_EXEC | VFS_O_READ | VFS_O_EXEC;
	dev_root.gid = dev_root.uid = 0;	// Belong to root
	dev_root.flags = VFS_DIR;
	dev_root.inode = 0;
	dev_root.len = 0;
	dev_root.impl = 0;
	dev_root.ops.readdir = readdir;
	dev_root.ops.finddir = finddir;

	u32int i;
	for (i = 0; i < 256; i++) {
		drivers[i].name = "Default";
		drivers[i].ops = ops_default;
	}
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

u32int devfs_register(const char *name, u32int flags, u32int major,
					u32int minor, u32int mode, u32int uid, u32int gid) {
	if (strlen(name) >= NAME_MAX)
		return -1;
	u32int i = 0;
	struct dev_file *filei, *newfile;
	for (filei = files; filei->next != NULL; filei = filei->next)	// Find last entry in files
		i++;

	newfile = (struct dev_file *)kmalloc(sizeof(struct dev_file));	// Create a new file
	strcpy(newfile->node.name, name);
	newfile->node.mask = mode;
	newfile->node.gid = gid;
	newfile->node.uid = uid;
	newfile->node.flags = flags;
	newfile->node.inode = i;
	newfile->node.len = 0;
	newfile->node.impl = ((major & 0xFF) << 24) | (minor & 0xFFFFFF);
	newfile->node.ops = drivers[major].ops;
	newfile->node.ptr = NULL;

	newfile->next = NULL;

	filei->next = newfile;											// Place ourselves in the file list

	return 0;
}

u32int register_chrdev(u32int major, const char *name, struct file_ops fops) {
	if (major == 0)															// Find an open major number if given zero
		for (; strcmp(drivers[major].name, "Default") != 0; major++)
			if (major == 255)												// Gone too far, all drivers taken
				return -1;

	if (strcmp(drivers[major].name, "Default") != 0)
		return -1;															// Already present

	drivers[major].name = name;
	drivers[major].ops = fops;

	struct dev_file *filei;
	for (filei = files; filei->next != NULL; filei = filei->next)			// For files that existed before their majors were registered
		if (MAJOR(filei->node.impl) == major && filei->node.flags & VFS_CHARDEV)
			filei->node.ops = fops;

	return major;
}
