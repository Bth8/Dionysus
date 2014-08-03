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
	uint16_t bps;					// Bytes per sector
	uint8_t spc;					// Sectors per cluster
	uint16_t nres;				// Number of reserved sectors
	uint8_t nFAT;					// Number of file allocation tables
	uint16_t rootEntries;			// # of entries in root dir, ignored in FAT32
	uint16_t nsect_short;			// If 0, real value in nsect_long
	uint8_t desc;
	uint16_t spf_old;				// Sectors per FAT in FAT12/16
	uint16_t spt;					// Sectors per track
	uint16_t nheads;
	uint32_t nhidden;				// Number of "hidden" sectors at the beginning
	uint32_t nsect_long;
	// This is where things diverge from FAT12/16
	uint32_t spf;					// Sectors per FAT
	uint16_t flags;
	uint16_t version;
	uint32_t root_cluster;		// Cluster # of root dir
	uint16_t fsinfo_cluster;
	uint16_t backup_cluster;		// Backup boot sect
	uint8_t reserved[12];
	uint8_t drive;
	uint8_t ntfslags;
	uint8_t sig;					// Signature. Must be 0x28 or 0x29
	uint32_t serial;
	char label[11];				// Padded with spaces
	char id[8];					// Should be "FAT32   ", but don't trust it
	/* These are both present in the actual structure, but aren't needed and
	 * make our representation here way too big

	uint8_t code[420];
	uint8_t bootable[2];			// 0x55, 0xAA
	*/

	// We add these in
	struct fat32_inode *inode_list;
	uint8_t lock;
} __attribute__((packed));

struct fat32_dirent {
	unsigned char name_short[11];
	uint8_t attr;
	uint8_t res_nt;				// Reserved for Windows NT use
	uint8_t tenths;				// Creation time in tenths of second
	uint16_t create_time;			// hhhhh mmmmmm sssss
	uint16_t create_date;			// yyyyyyy mmmm ddddd
	uint16_t last_access;			// Same format as create
	uint16_t cluster_high;
	uint16_t mod_time;
	uint16_t mod_date;
	uint16_t cluster_low;
	uint32_t size;
} __attribute__((packed));

struct fat32_long_name {
	uint8_t seq;
	char name0[10];				// First 5 chars. Actually wchars
	uint8_t attr;					// always FAT32_LONG_NAME
	uint8_t type;					// zero for name entries
	uint8_t checksum;
	char name1[12];
	uint16_t zero;
	char name2[4];
} __attribute__((packed));

struct fat32_inode {
	uint32_t first_cluster;
	uint32_t parent_first_cluster;
	off_t disk_off;
	uint16_t dirent_cluster;
	uint16_t dirent_index;
	struct fat32_inode *next;
};

void init_fat32(void);

#endif /* FAT32_H */
