/* fat32.h - FAT32 driver */
/* Copyright (C) 2011-2013 Bth8 <bth8fwd@gmail.com>
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

/* NOTE: we use private_data to store a cached copy of the cluster we've
 *  accessed most recently, inode contains the actual number of the first
 *  cluster, and the low 28 bits of impl contain the index of the cached
 *  cluster (i.e., if impl is 0, private_data contains a copy of the cluster
 *  indicated by inode. if it's 1, we've cached the next cluster in the file,
 *  and so on). The highest nibble of impl is used to store whether or not
 *  the cached copy is dirty. For directories, low 16 bits of impl contain the
 *  cluster number, high 16 contain the index number of the first valid file
 *  entry.
 */

#include <common.h>
#include <fs/fat32.h>
#include <vfs.h>
#include <kmalloc.h>
#include <string.h>

static struct superblock *return_sb(u32int flags, fs_node_t *dev);
struct file_system_type fat32 = {
	"fat32",
	0,
	return_sb,
	NULL
};

static int open(fs_node_t *file, u32int flags);
static int close(fs_node_t *file);
static u32int read(fs_node_t *file, void *buff, size_t count, off_t off);
static int readdir(fs_node_t *file, struct dirent *dirp, u32int index);
struct file_ops fat32_ops = {
	.open = open,
	.close = close,
	.read = read,
	.readdir = readdir,
};

static u32int fat32_attr_to_vfs(u32int attr) {
	u32int ret = 0;

	if (attr & FAT32_DIR)
		ret |= VFS_DIR;
	else
		ret |= VFS_FILE;

	return ret;
}

static struct superblock *return_sb(u32int flags, fs_node_t *dev) {
	struct fat32_boot_record *sb = (struct fat32_boot_record*)kmalloc(sizeof(struct fat32_boot_record));
	if (sb == NULL)
		return NULL;
	if (read_vfs(dev, sb, sizeof(struct fat32_boot_record), 0) < sizeof(struct fat32_boot_record))
		goto error;
	if (sb->sig != 0x28 && sb->sig != 0x29)
		goto error;

	fs_node_t *root = (fs_node_t *)kmalloc(sizeof(fs_node_t));
	if (root == NULL)
		goto error;

	// FAT32 doesn't support permissions, just give everyone everything.
	root->mask = VFS_O_EXEC | VFS_O_WRITE | VFS_O_READ |
				 VFS_G_EXEC | VFS_G_WRITE | VFS_G_READ |
				 VFS_U_EXEC | VFS_U_WRITE | VFS_U_READ;

	// Doesn't support users or groups either...
	root->uid = root->gid = 0;
	root->flags = fat32_attr_to_vfs(FAT32_DIR);
	root->inode = sb->root_cluster;
	root->ops = fat32_ops;

	struct superblock *ret = (struct superblock*)kmalloc(sizeof(struct superblock));
	if (ret == NULL)
		goto error2;

	ret->dev = dev;
	ret->private_data = sb;
	ret->root = root;
	root->fs_sb = ret;
	ret->flags = flags;

	return ret;

error2:
	kfree(root);
error:
	kfree(sb);
	return NULL;
}

static u32int read_cluster(struct superblock *sb, void *buff, u32int cluster) {
	struct fat32_boot_record *bpb = (struct fat32_boot_record*)sb->private_data;
	u32int first_data_sect = bpb->nres + (bpb->nFAT * bpb->spf);
	u32int cluster_sect = first_data_sect + ((cluster - 2) * bpb->spc);
	off_t off = cluster_sect * bpb->bps;

	return read_vfs(sb->dev, buff, bpb->spc * bpb->bps, off) == bpb->spc * bpb->bps;
}

static u32int write_cluster(struct superblock *sb, void *buff, u32int cluster) {
	struct fat32_boot_record *bpb = (struct fat32_boot_record*)sb->private_data;
	u32int first_data_sect = bpb->nres + (bpb->nFAT * bpb->spf);
	u32int cluster_sect = first_data_sect + ((cluster - 2) * bpb->spc);
	off_t off = cluster_sect * bpb->bps;

	return write_vfs(sb->dev, buff, bpb->spc * bpb->bps, off) == bpb->spc * bpb->bps;
}

static u32int follow_cluster_chain(struct superblock *sb, u32int cluster0, u32int index) {
	// Just expedite the process a bit
	if (index == 0)
		return cluster0;

	struct fat32_boot_record *bpb = (struct fat32_boot_record *)sb->private_data;
	u32int *FAT = (u32int *)kmalloc(bpb->spf * bpb->bps);
	if (FAT == NULL)
		return 0;

	off_t off = (bpb->nres + (cluster0 / (bpb->spf * bpb->bps / sizeof(u32int))) * bpb->spf) * bpb->bps;
	read_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);

	u32int ret = cluster0 & 0x0FFFFFFF;

	for (; index > 0; index--) {
		ret = FAT[ret % (bpb->spf * bpb->bps / sizeof(u32int))] & 0x0FFFFFFF;
		if (ret == 0 || ret >= FAT32_LAST_CLUSTER) {
			ret = 0;
			break;
		}
	}

	kfree(FAT);
	return ret;
}

static u32int read(fs_node_t *file, void *buff, size_t count, off_t off) {
	struct fat32_boot_record *bpb = (struct fat32_boot_record*)(file->fs_sb->private_data);
	u32int cluster = 0;

	// Offset not in the cached cluster
	if ((off / (bpb->spc * bpb->bps)) != FAT32_CLUSTER_NUM(file->impl)) {
		if (FAT32_CHECK_DIRTY(file->impl)) {
			cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_CLUSTER_NUM(file->impl));
			if (!write_cluster(file->fs_sb, file->private_data, cluster))
				return 0;
		}

		file->impl = off / (bpb->spc * bpb->bps);
		cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return 0;
	}

	off %= (bpb->spc * bpb->bps);
	u32int i = 0;

	// We straddle a cluster
	if (off + count >= bpb->spc * bpb->bps) {
		i = bpb->spc * bpb->bps - off;
		memcpy(buff, file->private_data, i);
		count -= i;
		off = 0;
		file->impl++;
		cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return i;
	}

	memcpy(buff + i, file->private_data + off, count);

	return i + count;
}


static int open(fs_node_t *file, u32int flags) {
	flags = flags;
	file->impl = 0;

	struct fat32_boot_record *bpb = (struct fat32_boot_record*)(file->fs_sb->private_data);
	void *cached_cluster = kmalloc(bpb->spc * bpb->bps);
	if (cached_cluster == NULL)
		return -1;

	read_cluster(file->fs_sb, cached_cluster, file->inode);
	file->private_data = cached_cluster;
	return 0;
}

static int close(fs_node_t *file) {
	if (FAT32_CHECK_DIRTY(file->impl)) {
		u32int cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_CLUSTER_NUM(file->impl));
		write_cluster(file->fs_sb, file->private_data, cluster);
		FAT32_SET_CLEAN(file->impl);
	}

	kfree(file->private_data);
	return 0;
}

static int getlongname(fs_node_t *file, u32int long_entry, char *buff) {
	struct fat32_long_name *entries = (struct fat32_long_name*)file->private_data;
	struct fat32_boot_record *bpb = (struct fat32_boot_record*)(file->fs_sb->private_data);
	u32int i;
	u32int cluster_old = 0;

	for (; !(entries[long_entry].seq & FAT32_LAST_LONG); long_entry--) {
		for (i = 0; i < 5; i++)
			buff[(entries[long_entry].seq - 1) * 13 + i] = entries[long_entry].name0[2 * i];
		for (; i < 6; i++)
			buff[(entries[long_entry].seq - 1) * 13 + i] = entries[long_entry].name1[2 * (i - 5)];
		for (; i < 2; i++)
			buff[(entries[long_entry].seq - 1) * 13 + i] = entries[long_entry].name2[2 * (i - 11)];
		if (long_entry == 0) {
			if (!cluster_old)
				cluster_old = FAT32_DIR_GET_CL(file->impl);
			u32int cluster;
			if (FAT32_DIR_CHK_DIRTY(file->impl)) {
				cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_DIR_GET_CL(file->impl));
				write_cluster(file->fs_sb, file->private_data, cluster);
				FAT32_DIR_SET_CLEAN(file->impl);
			}
			FAT32_DIR_SET_CL(file->impl, FAT32_DIR_GET_CL(file->impl) - 1);
			cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_DIR_GET_CL(file->impl));
			read_cluster(file->fs_sb, file->private_data, cluster);
			long_entry = bpb->spc * bpb->bps / sizeof(struct fat32_long_name) - 1;
		}
	}

	for (i = 0; i < 5; i++)
		buff[((entries[long_entry].seq & ~FAT32_LAST_LONG) - 1) * 13 + i] = entries[long_entry].name0[2 * i];
	for (; i < 11; i++)
		buff[((entries[long_entry].seq & ~FAT32_LAST_LONG) - 1) * 13 + i] = entries[long_entry].name1[2 * (i - 5)];
	for (; i < 13; i++)
		buff[((entries[long_entry].seq & ~FAT32_LAST_LONG) - 1) * 13 + i] = entries[long_entry].name2[2 * (i - 11)];

	if (cluster_old) {
		FAT32_DIR_SET_CL(file->impl, cluster_old);
		u32int cluster = follow_cluster_chain(file->fs_sb, file->inode, cluster_old);
		read_cluster(file->fs_sb, file->private_data, cluster);
	}

	return 0;
}

static int find_file_ent(fs_node_t *file, u32int index) {
	struct fat32_dirent *entries = (struct fat32_dirent*)file->private_data;
	struct fat32_boot_record *bpb = (struct fat32_boot_record*)(file->fs_sb->private_data);
	index -= FAT32_DIR_GET_ENT(file->impl);

	u32int i = 0;
	int offset = 0;
	// Probably a simpler way to do this, but this is how imma roll. Locate the entry.
	while (1) {
		// We've passed the end of the current cluster
		if (offset * sizeof(struct fat32_dirent) > bpb->spc * bpb->bps) {
			u32int cluster;
			if (FAT32_DIR_CHK_DIRTY(file->impl)) {
				cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_DIR_GET_CL(file->impl));
				write_cluster(file->fs_sb, file->private_data, cluster);
				FAT32_DIR_SET_CLEAN(file->impl);
			}
			FAT32_DIR_SET_CL(file->impl, FAT32_DIR_GET_CL(file->impl) + 1);
			cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_DIR_GET_CL(file->impl));
			read_cluster(file->fs_sb, file->private_data, cluster);
			FAT32_DIR_SET_ENT(file->impl, FAT32_DIR_GET_ENT(file->impl) + i);
			index -= i;
			i = offset = 0;
		}

		if (entries[offset].attr != FAT32_LONG_NAME && 
			entries[offset].name_short[0] != FAT32_RESERVED &&
			entries[offset].attr & ~FAT32_VOL_ID &&
			i++ == index)
			break;
		else if (entries[offset].name_short[0] == FAT32_END_DIR)
			return -1;
		offset++;
	}

	return offset;
}

static int readdir(fs_node_t *file, struct dirent *dirp, u32int index) {
	struct fat32_dirent *entries = (struct fat32_dirent*)file->private_data;

	int i = find_file_ent(file, index);
	if (i < 0)
		return i;

	dirp->d_ino = (entries[i].cluster_high << 16) | entries[i].cluster_low;

	if (entries[i - 1].attr == FAT32_LONG_NAME) {
		return getlongname(file, i - 1, dirp->d_name);
	} else {
		u32int nameDirp = 0;
		if (entries[i].attr & FAT32_HIDDEN) {
			dirp->d_name[0] = '.';
			nameDirp++;
		}
		u32int nameEnt;
		for (nameEnt = 0; nameEnt < 8 && entries[i].name_short[nameEnt] != 0x20; nameEnt++) {
			dirp->d_name[nameDirp] = entries[i].name_short[nameEnt];
			nameDirp++;
		}
		if (entries[i].name_short[8] != 0x20) {
			dirp->d_name[nameDirp] = '.';
			nameDirp++;
			for (nameEnt = 8; nameEnt < 11 && entries[i].name_short[nameEnt] != 0x20; nameEnt++) {
				dirp->d_name[nameDirp] = entries[i].name_short[nameEnt];
				nameDirp++;
			}
		}

		dirp->d_name[nameDirp] = '\0';
	}

	return 0;
}

void init_fat32(void) {
	register_fs(&fat32);
}
