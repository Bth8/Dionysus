/* fat32.h - FAT32 driver header */

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

#ifndef FAT32_H
#define FAT32_H

#include <common.h>

#define FAT32_RDONLY	0x01
#define FAT32_HIDDEN	0x02
#define FAT32_SYS		0x04
#define FAT32_VOL_ID	0x08
#define FAT32_LONG_NAME	0x0F
#define FAT32_DIR		0x10
#define FAT32_ARCHIVE	0x20

#define FAT32_END_DIR	0x00
#define FAT32_RESERVED	0xE5

#define FAT32_LAST_LONG	0x40

#define FAT32_LAST_CLUSTER	0x0FFFFFF8U
#define FAT32_BAD_CLUSTER	0x0FFFFFF7U

#define FAT32_CLUSTER_NUM(x)	((x) & 0x0FFFFFFF)
#define FAT32_CHECK_DIRTY(x)	((x) >> 28)
#define FAT32_SET_DIRTY(x)		((x) |= 0x10000000)
#define FAT32_SET_CLEAN(x)		((x) &= 0x0FFFFFFF)

#define FAT32_DIR_CHK_DIRTY(x)	(((x) >> 15) & 0x01)
#define FAT32_DIR_SET_DIRTY(x)	((x) |= 0x8000)
#define FAT32_DIR_SET_CLEAN(x)	((x) |= ~0x8000)
#define FAT32_DIR_GET_CL(x)		((x) & 0xFFFF)
#define FAT32_DIR_GET_ENT(x)	((x) >> 16)
#define FAT32_DIR_SET_CL(x, y)	((x) = ((x) & 0xFFFF0000) | (y))
#define FAT32_DIR_SET_ENT(x, y)	((x) = ((x) & 0xFFFF) | ((y) << 16))

struct fat32_boot_record {
	char jump[3];				// Jump instruction
	char oemID[8];
	u16int bps;					// Bytes per sector
	u8int spc;					// Sectors per cluster
	u16int nres;				// Number of reserved sectors
	u8int nFAT;					// Number of file allocation tables
	u16int rootEntries;			// # of entries in root dir, ignored in FAT32
	u16int nsect_short;			// If 0, real value in nsect_long
	u8int desc;
	u16int spf_old;				// Sectors per FAT in FAT12/16
	u16int spt;					// Sectors per track
	u16int nheads;
	u32int nhidden;				// Number of "hidden" sectors at the beginning
	u32int nsect_long;
	// This is where things diverge from FAT12/16
	u32int spf;					// Sectors per FAT
	u16int flags;
	u16int version;
	u32int root_cluster;		// Cluster # of root dir
	u16int fsinfo_cluster;
	u16int backup_cluster;		// Backup boot sect
	u8int reserved[12];
	u8int drive;
	u8int ntfslags;
	u8int sig;					// Signature. Must be 0x28 or 0x29
	u32int serial;
	char label[11];				// Padded with spaces
	char id[8];					// Should be "FAT32   ", but don't trust it
	/* These are both present in the actual structure, but aren't needed and
	 * make our representation here way too big

	u8int code[420];
	u8int bootable[2];			// 0x55, 0xAA
	*/

	// We add these in
	struct fat32_inode *inode_list;
	u8int lock;
} __attribute__((packed));

struct fat32_dirent {
	unsigned char name_short[11];
	u8int attr;
	u8int res_nt;				// Reserved for Windows NT use
	u8int tenths;				// Creation time in tenths of second
	u16int create_time;			// hhhhh mmmmmm sssss
	u16int create_date;			// yyyyyyy mmmm ddddd
	u16int last_access;			// Same format as create
	u16int cluster_high;
	u16int mod_time;
	u16int mod_date;
	u16int cluster_low;
	u32int size;
} __attribute__((packed));

struct fat32_long_name {
	u8int seq;
	char name0[10];				// First 5 chars. Actually wchars
	u8int attr;					// always FAT32_LONG_NAME
	u8int type;					// zero for name entries
	u8int checksum;
	char name1[12];
	u16int zero;
	char name2[4];
} __attribute__((packed));

struct fat32_inode {
	u32int first_cluster;
	u32int parent_first_cluster;
	off_t disk_off;
	u16int dirent_cluster;
	u16int dirent_index;
	struct fat32_inode *next;
};

void init_fat32(void);

#endif /* FAT32_H */
