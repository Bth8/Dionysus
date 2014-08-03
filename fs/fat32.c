/* fat32.h - FAT32 driver */

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

/* NOTE: we use private_data to store a cached copy of the cluster we've
 *  accessed most recently, inode to contain the actual number of the first
 *  cluster, and the low 28 bits of impl to contain the index of the cached
 *  cluster (i.e., if impl is 0, private_data contains a copy of the cluster
 *  indicated by inode, if it's 1, we've cached the next cluster in the file,
 *  etc). The highest nibble of impl is used to store whether or not the
 *  cached copy is dirty. For directories, low 16 bits of impl contain the
 *  cluster number, high 16 contain the index number of the first valid file
 *  entry.
 */

#include <common.h>
#include <fs/fat32.h>
#include <vfs.h>
#include <kmalloc.h>
#include <string.h>
#include <time.h>
#include <dev.h>

static struct superblock *return_sb(uint32_t flags, fs_node_t *dev);
struct file_system_type fat32 = {
	"fat32",
	0,
	return_sb,
	NULL
};

static int open(fs_node_t *file, uint32_t flags);
static int close(fs_node_t *file);
static uint32_t read(fs_node_t *file, void *buff, size_t count, off_t off);
static uint32_t write(fs_node_t *file, const void *buff, size_t count,
		off_t off);
static int readdir(fs_node_t *file, struct dirent *dirp, uint32_t index);
static fs_node_t *finddir(fs_node_t *file, const char *name);
static int stat(fs_node_t *file, struct stat *buff);
struct file_ops fat32_ops = {
	.open = open,
	.close = close,
	.read = read,
	.write = write,
	.readdir = readdir,
	.finddir = finddir,
	.stat = stat,
};

static uint32_t fat32_attr_to_vfs(uint32_t attr) {
	uint32_t ret = 0;

	if (attr & FAT32_DIR)
		ret |= VFS_DIR;
	else
		ret |= VFS_FILE;

	return ret;
}

static struct superblock *return_sb(uint32_t flags, fs_node_t *dev) {
	struct fat32_boot_record *sb =
		(struct fat32_boot_record*)kmalloc(sizeof(struct fat32_boot_record));
	if (sb == NULL)
		return NULL;
	if (read_vfs(dev, sb, sizeof(struct fat32_boot_record), 0) <
			sizeof(struct fat32_boot_record))
		goto error;
	if (sb->sig != 0x28 && sb->sig != 0x29)
		goto error;

	sb->inode_list = NULL;
	sb->lock = 0;

	fs_node_t *root = (fs_node_t *)kmalloc(sizeof(fs_node_t));
	if (root == NULL)
		goto error;

	// FAT32 doesn't support permissions, just give everyone everything.
	root->mask = VFS_O_EXEC | VFS_O_READ |
				 VFS_G_EXEC | VFS_G_READ |
				 VFS_U_EXEC | VFS_U_READ;

	if (!(flags & O_RDONLY))
		root->mask |= VFS_O_WRITE | VFS_G_WRITE | VFS_U_WRITE;

	// Doesn't support users or groups either...
	root->uid = root->gid = 0;
	root->flags = fat32_attr_to_vfs(FAT32_DIR);
	root->inode = sb->root_cluster;
	root->ops = fat32_ops;

	struct superblock *ret =
		(struct superblock*)kmalloc(sizeof(struct superblock));
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

static uint32_t read_cluster(struct superblock *sb, void *buff,
		uint32_t cluster) {
	// For the root dir
	if (cluster == 0)
		cluster = 2;
	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)sb->private_data;
	uint32_t first_data_sect = bpb->nres + (bpb->nFAT * bpb->spf);
	uint32_t cluster_sect = first_data_sect + ((cluster - 2) * bpb->spc);
	off_t off = cluster_sect * bpb->bps;

	return read_vfs(sb->dev, buff, bpb->spc * bpb->bps, off) ==
		bpb->spc * bpb->bps;
}

static uint32_t write_cluster(struct superblock *sb, void *buff,
		uint32_t cluster) {
	if (cluster == 0)
		cluster = 2;
	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)sb->private_data;
	uint32_t first_data_sect = bpb->nres + (bpb->nFAT * bpb->spf);
	uint32_t cluster_sect = first_data_sect + ((cluster - 2) * bpb->spc);
	off_t off = cluster_sect * bpb->bps;

	return write_vfs(sb->dev, buff, bpb->spc * bpb->bps, off) ==
		bpb->spc * bpb->bps;
}

static uint32_t follow_cluster_chain(struct superblock *sb, uint32_t cluster0,
		uint32_t depth) {
	// Just expedite the process a bit
	if (depth == 0)
		return cluster0;

	cluster0 &= 0x0FFFFFFF;

	struct fat32_boot_record *bpb =
		(struct fat32_boot_record *)sb->private_data;
	uint32_t *FAT = (uint32_t *)kmalloc(bpb->spf * bpb->bps);
	if (FAT == NULL)
		return 0;

	off_t off = (bpb->nres +
			(cluster0 / (bpb->spf * bpb->bps / sizeof(uint32_t))) * bpb->spf) *
		bpb->bps;
	read_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);

	uint32_t fat_index = cluster0 % (bpb->spf * bpb->bps / sizeof(uint32_t));
	uint32_t fat_offset = cluster0 - fat_index;

	for (; depth > 0; depth--) {
		fat_index = (FAT[fat_index] & 0x0FFFFFFF) - fat_offset;
		if (fat_index + fat_offset == 0 ||
				fat_index + fat_offset >= FAT32_LAST_CLUSTER) {
			kfree(FAT);
			return 0;
		}
		if (fat_index * sizeof(uint32_t) > bpb->spf * bpb->bps) {
			off += bpb->spf * bpb->bps;
			read_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);
			fat_offset += fat_index;
			fat_index = 0;
		}
	}

	kfree(FAT);
	return fat_index + fat_offset;
}

static uint32_t alloc_clusters(struct superblock *sb, uint32_t chain_start,
		uint32_t nclusters) {
	chain_start &= 0x0FFFFFFF;

	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)sb->private_data;
	uint32_t *FAT = (uint32_t *)kmalloc(bpb->spf * bpb->bps);
	if (FAT == NULL)
		return 0;

	off_t off = (bpb->nres +
			(chain_start / (bpb->spf * bpb->bps / sizeof(uint32_t))) *
			bpb->spf) * bpb->bps;
	read_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);

	uint32_t fat_index = chain_start % (bpb->spf * bpb->bps / sizeof(uint32_t));
	uint32_t fat_offset = chain_start - fat_index;

	// Allocate a block without any parents
	if (chain_start == 0) {
		for (fat_index = 2; FAT[fat_index] != 0;
				fat_index = (FAT[fat_index] & 0x0FFFFFFF) - fat_offset) {
			if (fat_index * sizeof(uint32_t) >= bpb->spf * bpb->bps) {
				if ((fat_index + fat_offset) * bpb->spc > bpb->nsect_long) {
					kfree(FAT);
					return 0xFFFFFFFF;
				}
				off += bpb->spf * bpb->bps;
				read_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);
				fat_offset += fat_index;
				fat_index = 0;
			}
		}
		FAT[fat_index] = FAT32_LAST_CLUSTER;
		write_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);
		kfree(FAT);
		return fat_offset + fat_index;
	}

	// Seek index to end of current chain
	for (; FAT[fat_index] < FAT32_LAST_CLUSTER;
			fat_index = (FAT[fat_index] & 0x0FFFFFFF) - fat_offset) {
		if (fat_index * sizeof(uint32_t) >= bpb->spf * bpb->bps) {
			off += bpb->spf * bpb->bps;
			read_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);
			fat_offset += fat_index;
			fat_index = 0;
		} else if (fat_index + fat_offset == 0) {
			kfree(FAT);
			return 0;
		}
	}

	uint32_t search_index = 0;
	uint32_t nalloc = 0;
	for (; nclusters > 0; nclusters--) {
		// Get first empty cluster
		while ((FAT[search_index] & 0x0FFFFFFF) != 0) {
			// We've passed the end of the FAT. Shit just got difficult
			if (++search_index * sizeof(uint32_t) >= bpb->spf * bpb->bps) {
				// We're not actually going to handle this. Fuck it.
				write_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);
				kfree(FAT);
				return nalloc;
			}
		}

		FAT[search_index] = FAT32_LAST_CLUSTER;
		FAT[fat_index] = search_index + fat_offset;
		fat_index = search_index;
		nalloc++;
	}

	write_vfs(sb->dev, FAT, bpb->spf * bpb->bps, off);
	kfree(FAT);
	return nalloc;
}

static int update_dirent(fs_node_t *file) {
	if (file->inode != 2) {
		struct fat32_inode *inode =
			((struct fat32_boot_record*)file->fs_sb->private_data)->
				inode_list;
		ASSERT(inode != NULL);
		while (inode->first_cluster != file->inode)
			inode = inode->next;

		struct fat32_dirent *our_entry = kmalloc(sizeof(struct fat32_dirent));
		if (our_entry == NULL)
			return -1;

		read_vfs(file->fs_sb->dev, our_entry, sizeof(struct fat32_dirent),
				inode->disk_off);

		time_t cur_time = time(NULL);
		struct tm *time = gmtime(&cur_time);

		our_entry->mod_time = (uint16_t)(time->tm_hour << 11);
		our_entry->mod_time |= (uint16_t)((time->tm_min & 0x3F) << 5);
		our_entry->mod_time |= (uint16_t)(time->tm_sec & 0x1F);

		our_entry->mod_date = (uint16_t)((time->tm_year - 80) << 9);
		our_entry->mod_date |= (uint16_t)(((time->tm_mon + 1) & 0x3F) << 5);
		our_entry->mod_date |= (uint16_t)(time->tm_mday & 0x1F);

		our_entry->size = (uint32_t)file->len;

		write_vfs(file->fs_sb->dev, our_entry, sizeof(struct fat32_dirent),
				inode->disk_off);
		kfree(our_entry);
	}
	return 0;
}

static uint32_t read(fs_node_t *file, void *buff, size_t count, off_t off) {
	if (count == 0)
		return 0;

	if (count + off >= file->len)
		count = file->len - off;

	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)(file->fs_sb->private_data);
	uint32_t cluster = 0;

	// Offset not in the cached cluster
	if ((off / (bpb->spc * bpb->bps)) != FAT32_CLUSTER_NUM(file->impl)) {
		if (FAT32_CHECK_DIRTY(file->impl)) {
			cluster = follow_cluster_chain(file->fs_sb, file->inode,
					FAT32_CLUSTER_NUM(file->impl));
			if (!write_cluster(file->fs_sb, file->private_data, cluster))
				return 0;
			update_dirent(file);
		}

		file->impl = off / (bpb->spc * bpb->bps);
		cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return 0;
	}

	off %= (bpb->spc * bpb->bps);
	size_t read = 0;

	// We straddle a cluster
	if (off + count >= bpb->spc * bpb->bps) {
		memcpy(buff, file->private_data + off, bpb->spc * bpb->bps - off);
		count -= bpb->spc * bpb->bps - off;
		read += bpb->spc * bpb->bps - off;
		off = 0;
		file->impl++;
		cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return read;
	}

	while (count >= bpb->spc * bpb->bps) {
		memcpy(buff + read, file->private_data, bpb->spc * bpb->bps);
		count -= bpb->spc * bpb->bps;
		read += bpb->spc * bpb->bps;
		file->impl++;
		cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return read;
	}

	memcpy(buff + read, file->private_data + off, count);
	read += count;

	return read;
}

static uint32_t write(fs_node_t *file, const void *buff, size_t count,
		off_t off) {
	if (count == 0)
		return 0;

	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)(file->fs_sb->private_data);

	if (count + off >= file->len) {
		size_t delta = count + off - file->len;
		uint32_t nclusters = delta / (bpb->spc * bpb->bps);
		if (delta % (bpb->spc * bpb->bps) != 0)
			nclusters++;
		if (nclusters > 0) {
			uint32_t nalloc = alloc_clusters(file->fs_sb, file->inode,
					nclusters);
			if (nalloc < nclusters)
				file->len += nclusters * bpb->spc * bpb->bps;
			else
				file->len = off + count;
		} else
			file->len = off + count;
	}

	uint32_t cluster = 0;

	// Offset not in the cached cluster
	if ((off / (bpb->spc * bpb->bps)) != FAT32_CLUSTER_NUM(file->impl)) {
		if (FAT32_CHECK_DIRTY(file->impl)) {
			cluster = follow_cluster_chain(file->fs_sb, file->inode,
					FAT32_CLUSTER_NUM(file->impl));
			if (!write_cluster(file->fs_sb, file->private_data, cluster))
				return 0;
			update_dirent(file);
		}

		file->impl = off / (bpb->spc * bpb->bps);
		cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return 0;
	}

	off %= (bpb->spc * bpb->bps);
	size_t written = 0;

	FAT32_SET_DIRTY(file->impl);

	// We straddle a cluster
	if (off + count >= bpb->spc * bpb->bps) {
		memcpy(file->private_data + off, buff, bpb->spc * bpb->bps - off);
		cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		if (!write_cluster(file->fs_sb, file->private_data, cluster))
			return written;
		update_dirent(file);
		count -= bpb->spc * bpb->bps - off;
		written += bpb->spc * bpb->bps - off;
		off = 0;
		file->impl++;
		cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return written;
	}

	while (count >= bpb->spc * bpb->bps) {
		memcpy(file->private_data, buff + written, bpb->spc * bpb->bps);
		cluster = follow_cluster_chain(file->fs_sb, file->inode, FAT32_CLUSTER_NUM(file->impl));
		// We don't need to update the mod time here, since we just did it
		// in the previous check
		if (!write_cluster(file->fs_sb, file->private_data, cluster))
			return written;
		count -= bpb->spc * bpb->bps;
		written += bpb->spc * bpb->bps;
		file->impl++;
		cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		if (!read_cluster(file->fs_sb, file->private_data, cluster))
			return written;
	}

	memcpy(file->private_data + off, buff + written, count);

	return written;
}

static int open(fs_node_t *file, uint32_t flags) {
	if (!(flags & O_RDONLY))
		file->mask &= ~(VFS_O_READ | VFS_G_READ | VFS_U_READ);
	if (!(flags & O_WRONLY))
		file->mask &= ~(VFS_O_WRITE | VFS_G_WRITE | VFS_U_WRITE);

	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)(file->fs_sb->private_data);
	void *cached_cluster = kmalloc(bpb->spc * bpb->bps);
	if (cached_cluster == NULL)
		return -1;

	if (file->inode != 2) {
		struct fat32_inode *inode =
			(struct fat32_inode*)kmalloc(sizeof(struct fat32_inode));
		if (inode == NULL) {
			kfree(cached_cluster);
			return -1;
		}

		inode->first_cluster = file->inode;
		inode->parent_first_cluster = file->uid;
		inode->dirent_cluster = file->impl & 0xFFFF;
		inode->dirent_index = file->impl >> 16;
		inode->next = NULL;

		uint32_t cluster = follow_cluster_chain(file->fs_sb,
				inode->parent_first_cluster, inode->dirent_cluster);
		uint32_t first_data_sect = bpb->nres + (bpb->nFAT * bpb->spf);
		uint32_t cluster_sect = first_data_sect + ((cluster - 2) * bpb->spc);
		inode->disk_off = cluster_sect * bpb->bps +
			inode->dirent_index * sizeof(struct fat32_dirent);

		spin_lock(&((struct fat32_boot_record*)file->fs_sb->private_data)->
				lock);
		struct fat32_inode *iter = ((struct fat32_boot_record*)file->
				fs_sb->private_data)->inode_list;
		if (iter == NULL) {
			((struct fat32_boot_record*)file->fs_sb->private_data)
				->inode_list = inode;
		} else {
			while (iter->next != NULL)
				iter = iter->next;
			iter->next = inode;
		}
		spin_unlock(&((struct fat32_boot_record*)file->fs_sb->private_data)->
				lock);
	}

	file->impl = 0;
	file->uid = file->gid = 0;

	read_cluster(file->fs_sb, cached_cluster, file->inode);
	file->private_data = cached_cluster;

	return 0;
}

static int close(fs_node_t *file) {
	if (FAT32_CHECK_DIRTY(file->impl)) {
		uint32_t cluster = follow_cluster_chain(file->fs_sb, file->inode,
				FAT32_CLUSTER_NUM(file->impl));
		write_cluster(file->fs_sb, file->private_data, cluster);
		update_dirent(file);
	}

	if (file->inode != 2) {
		spin_lock(&((struct fat32_boot_record*)file->fs_sb->private_data)->
				lock);
		struct fat32_inode *inode = ((struct fat32_boot_record*)file->fs_sb->
				private_data)->inode_list;
		ASSERT(inode != NULL);
		void *our_inode;
		if (inode->next == NULL) {
			our_inode = inode;
			((struct fat32_boot_record*)file->fs_sb->private_data)->
				inode_list = NULL;
		} else {
			while (inode->next->first_cluster != file->inode)
				inode = inode->next;
			our_inode = inode->next;
			inode->next = inode->next->next;
		}
		kfree(our_inode);
		spin_unlock(&((struct fat32_boot_record*)file->fs_sb->private_data)->
				lock);
	}

	kfree(file->private_data);
	return 0;
}

static int get_name(fs_node_t *file, uint32_t entry, char *buff) {
	struct fat32_long_name *entries =
		(struct fat32_long_name*)file->private_data;
	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)(file->fs_sb->private_data);

	if (entries[entry - 1].attr == FAT32_LONG_NAME) {
			uint32_t i;
			uint32_t cluster_old = 0;
			entry--;

			for (; !(entries[entry].seq & FAT32_LAST_LONG); entry--) {
				for (i = 0; i < 5; i++)
					buff[(entries[entry].seq - 1) * 13 + i] =
						entries[entry].name0[2 * i];
				for (; i < 6; i++)
					buff[(entries[entry].seq - 1) * 13 + i] =
						entries[entry].name1[2 * (i - 5)];
				for (; i < 2; i++)
					buff[(entries[entry].seq - 1) * 13 + i] =
						entries[entry].name2[2 * (i - 11)];
				if (entry == 0) {
					if (!cluster_old)
						cluster_old = FAT32_DIR_GET_CL(file->impl);
					uint32_t cluster;
					if (FAT32_DIR_CHK_DIRTY(file->impl)) {
						cluster = follow_cluster_chain(file->fs_sb,
								file->inode, FAT32_DIR_GET_CL(file->impl));
						write_cluster(file->fs_sb, file->private_data,
								cluster);
						update_dirent(file);
						FAT32_DIR_SET_CLEAN(file->impl);
					}
					FAT32_DIR_SET_CL(file->impl,
							FAT32_DIR_GET_CL(file->impl) - 1);
					cluster = follow_cluster_chain(file->fs_sb, file->inode,
							FAT32_DIR_GET_CL(file->impl));
					read_cluster(file->fs_sb, file->private_data, cluster);
					entry = bpb->spc * bpb->bps /
						sizeof(struct fat32_long_name) - 1;
				}
			}

			for (i = 0; i < 5; i++)
				buff[((entries[entry].seq & ~FAT32_LAST_LONG) - 1) * 13 + i]
					= entries[entry].name0[2 * i];
			for (; i < 11; i++)
				buff[((entries[entry].seq & ~FAT32_LAST_LONG) - 1) * 13 + i]
					= entries[entry].name1[2 * (i - 5)];
			for (; i < 13; i++)
				buff[((entries[entry].seq & ~FAT32_LAST_LONG) - 1) * 13 + i]
					= entries[entry].name2[2 * (i - 11)];

			if (cluster_old) {
				FAT32_DIR_SET_CL(file->impl, cluster_old);
				uint32_t cluster =
					follow_cluster_chain(file->fs_sb, file->inode,
							cluster_old);
				read_cluster(file->fs_sb, file->private_data, cluster);
			}
	} else {
		uint32_t nameBuff = 0;
		if (((struct fat32_dirent*)entries)[entry].attr & FAT32_HIDDEN) {
			buff[0] = '.';
			nameBuff++;
		}
		uint32_t nameEnt;
		for (nameEnt = 0; nameEnt < 8 &&
				((struct fat32_dirent*)entries)[entry].name_short[nameEnt] !=
				0x20; nameEnt++) {
			buff[nameBuff] =
				((struct fat32_dirent*)entries)[entry].name_short[nameEnt];
			nameBuff++;
		}
		if (((struct fat32_dirent*)entries)[entry].name_short[8] != 0x20) {
			buff[nameBuff] = '.';
			nameBuff++;
			for (nameEnt = 8; nameEnt < 11 &&
					((struct fat32_dirent *)entries)[entry].
						name_short[nameEnt] != 0x20; nameEnt++) {
				buff[nameBuff] =
					((struct fat32_dirent *)entries)[entry].
						name_short[nameEnt];
				nameBuff++;
			}
		}

		buff[nameBuff] = '\0';
	}


	return 0;
}

// Args are something of a kludge...
// Offset automatically starts at the entry offset in the dir to make
// finddir easier
static int find_file_ent(fs_node_t *file, uint32_t index, int offset) {
	struct fat32_dirent *entries = (struct fat32_dirent*)file->private_data;
	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)(file->fs_sb->private_data);
	index -= FAT32_DIR_GET_ENT(file->impl);

	uint32_t i = 0;
	// Probably a simpler way to do this, but this is how imma roll
	// Locate the entry.
	while (1) {
		// We've passed the end of the current cluster
		if (offset * sizeof(struct fat32_dirent) > bpb->spc * bpb->bps) {
			uint32_t cluster;
			if (FAT32_DIR_CHK_DIRTY(file->impl)) {
				cluster = follow_cluster_chain(file->fs_sb, file->inode,
						FAT32_DIR_GET_CL(file->impl));
				write_cluster(file->fs_sb, file->private_data, cluster);
				update_dirent(file);
				FAT32_DIR_SET_CLEAN(file->impl);
			}
			FAT32_DIR_SET_CL(file->impl, FAT32_DIR_GET_CL(file->impl) + 1);
			cluster = follow_cluster_chain(file->fs_sb, file->inode,
					FAT32_DIR_GET_CL(file->impl));
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

static int readdir(fs_node_t *file, struct dirent *dirp, uint32_t index) {
	struct fat32_dirent *entries = (struct fat32_dirent*)file->private_data;

	// Offset is always 0 since we want to start from the beginning of
	// the directory
	int i = find_file_ent(file, index, 0);
	if (i < 0)
		return i;

	dirp->d_ino = (entries[i].cluster_high << 16) | entries[i].cluster_low;

	get_name(file, i, dirp->d_name);

	return 0;
}

static fs_node_t *finddir(fs_node_t *file, const char *name) {
	struct fat32_dirent *entries = (struct fat32_dirent*)file->private_data;

	fs_node_t *ret = (fs_node_t*)kmalloc(sizeof(fs_node_t));
	if (ret == NULL)
		return NULL;

	int offset = -1;

	while ((offset = find_file_ent(file, 0, offset + 1)) >= 0) {
		get_name(file, offset, ret->name);
		if (strcmp(ret->name, name) == 0)
			break;
	}

	if (offset < 0) {
		kfree(ret);
		return NULL;
	}

	ret->mask = VFS_O_EXEC | VFS_O_READ |
				VFS_G_EXEC | VFS_G_READ |
				VFS_U_EXEC | VFS_U_READ;

	if (!(entries[offset].attr & FAT32_RDONLY) ||
			!(file->fs_sb->flags & O_RDONLY))
		ret->mask |= VFS_O_WRITE | VFS_G_WRITE | VFS_U_WRITE;

	ret->flags = fat32_attr_to_vfs(entries[offset].attr);
	ret->inode = (entries[offset].cluster_high << 16) |
		entries[offset].cluster_low;
	ret->len = entries[offset].size;
	ret->ops = fat32_ops;
	ret->fs_sb = file->fs_sb;

	/* Ugly little kludge going on here. Since we don't want to allocate
	 * memory for an inode struct yet, we store the dirent_cluster and
	 * dirent_index in impl since we're not using it.
	 * To store the parent's cluster, we use the uid since FAT32 doesn't
	 * support file ownership.
	 * All is rectified once the file is opened.
	 */
	ret->uid = file->inode;
	ret->impl = (offset << 16) | FAT32_DIR_GET_CL(file->impl);

	return ret;
}

static int stat(fs_node_t *file, struct stat *buff) {
	struct fat32_boot_record *bpb =
		(struct fat32_boot_record*)(file->fs_sb->private_data);
	buff->st_dev = get_dev(file->fs_sb->dev);
	buff->st_ino = file->inode;
	buff->st_mode = file->mask;
	buff->st_nlink = 1;
	buff->st_uid = file->uid;
	buff->st_gid = file->gid;
	buff->st_size = file->len;
	buff->st_blksize = bpb->spc * bpb->bps;
	buff->st_blocks = file->len / 512;
	if (file->inode != 2) {
		struct fat32_inode *inode = bpb->inode_list;
		ASSERT(inode != NULL);
		while (inode->first_cluster != file->inode)
			inode = inode->next;

		struct fat32_dirent *our_entry = kmalloc(sizeof(struct fat32_dirent));
		if (our_entry == NULL)
			return -1;

		read_vfs(file->fs_sb->dev, our_entry, sizeof(struct fat32_dirent),
				inode->disk_off);

		struct tm temp;
		temp.tm_sec = 0;
		temp.tm_min = 0;
		temp.tm_hour = 0;
		temp.tm_mday = our_entry->last_access & 0x1F;
		temp.tm_mon = ((our_entry->last_access >> 5) & 0x3F) - 1;
		temp.tm_year = ((our_entry->last_access >> 9) + 80);
		buff->st_atime = mktime(&temp);

		temp.tm_sec = our_entry->mod_time & 0x1F;
		temp.tm_min = (our_entry->mod_time >> 5) & 0x3F;
		temp.tm_hour = our_entry->mod_time >> 11;
		temp.tm_mday = our_entry->mod_date & 0x1F;
		temp.tm_mon = ((our_entry->mod_date >> 5) & 0x3F) - 1;
		temp.tm_year = ((our_entry->mod_date >> 9) + 80);
		buff->st_mtime = mktime(&temp);

		temp.tm_sec = our_entry->create_time & 0x1F;
		temp.tm_min = (our_entry->create_time >> 5) & 0x3F;
		temp.tm_hour = our_entry->create_time >> 11;
		temp.tm_mday = our_entry->create_date & 0x1F;
		temp.tm_mon = ((our_entry->create_date >> 5) & 0x3F) - 1;
		temp.tm_year = ((our_entry->create_date >> 9) + 80);
		buff->st_ctime = mktime(&temp);
	}
	return 0;
}

void init_fat32(void) {
	register_fs(&fat32);
}
