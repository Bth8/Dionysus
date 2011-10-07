/* ide.h - Definitions and functions for IDE operations */
/* Copyright (C) 2011 Bth8 <bth8fwd@gmail.com>
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

#ifndef IDE_H
#define IDE_H
#include <common.h>

// Status masks
#define ATA_SR_BSY				0x80
#define ATA_SR_DRDY				0x40
#define ATA_SR_DF				0x20
#define ATA_SR_DSC				0x10
#define ATA_SR_DRQ				0x08
#define ATA_SR_CORR				0x04
#define ATA_SR_IDX				0x02
#define ATA_SR_ERR				0x01

// Error masks
#define ATA_ER_BBK				0x80
#define ATA_ER_UNC				0x40
#define ATA_ER_MC				0x20
#define ATA_ER_IDNF				0x10
#define ATA_ER_MCR				0x08
#define ATA_ER_ABRT				0x04
#define ATA_ER_TK0NF			0x02
#define ATA_ER_AMNF				0x01

// ATA commands
#define ATA_CMD_READ_PIO		0x20
#define ATA_CMD_READ_PIO_EXT	0x24
#define ATA_CMD_READ_DMA		0xC8
#define ATA_CMD_READ_DMA_EXT	0x25
#define ATA_CMD_WRITE_PIO		0x30
#define ATA_CMD_WRITE_PIO_EXT	0x34
#define ATA_CMD_WRITE_DMA		0xCA
#define ATA_CMD_WRITE_DMA_EXT	0x35
#define ATA_CMD_CACHE_FLUSH		0xE7
#define ATA_CMD_CACHE_FLUSH_EXT	0xEA
#define ATA_CMD_PACKET			0xA0
#define ATA_CMD_IDENTIFY_PACKET	0xA1
#define ATA_CMD_IDENTIFY		0xEC

// ATAPI commands
#define ATAPI_CMD_READ			0xA8
#define ATAPI_CMD_EJECT			0x1B

// Identification space
#define ATA_IDENT_DEVICETYPE	0
#define ATA_IDENT_CYLINDERS		2
#define ATA_IDENT_HEADS			6
#define ATA_IDENT_SECTORS		12
#define ATA_IDENT_SERIAL		20
#define ATA_IDENT_MODEL			54
#define ATA_IDENT_CAPABILITIES	98
#define ATA_IDENT_FIELDVALID	106
#define ATA_IDENT_MAX_LBA		120
#define ATA_IDENT_COMMANDSETS	164
#define ATA_IDENT_MAX_LBA_EXT	200

// Type of drive
#define IDE_ATA					0x00
#define IDE_ATAPI				0x01

// Position on cable
#define ATA_MASTER				0x00
#define ATA_SLAVE				0x01

// Registers
#define ATA_REG_DATA			0x00
#define ATA_REG_ERROR			0x01
#define ATA_REG_FEATURES		0x01
#define ATA_REG_SECCOUNT0		0x02
#define ATA_REG_LBA0			0x03
#define ATA_REG_LBA1			0x04
#define ATA_REG_LBA2			0x05
#define ATA_REG_HDDEVSEL		0x06
#define ATA_REG_COMMAND			0x07
#define ATA_REG_STATUS			0x07
#define ATA_REG_SECCOUNT1		0x08
#define ATA_REG_LBA3			0x09
#define ATA_REG_LBA4			0x0A
#define ATA_REG_LBA5			0x0B
#define ATA_REG_CONTROL			0x0C
#define ATA_REG_ALTSTATUS		0x0C
#define ATA_REG_DEVADDRESS		0x0D

// Channels
#define ATA_PRIMARY				0x00
#define ATA_SECONDARY			0x01
 
// Directions
#define ATA_READ				0x00
#define ATA_WRITE				0x01

struct IDEChannelRegisters {
	u16int base;
	u16int ctrl;
	u16int bmide;
	u8int nEIN;
};

struct IDEDevice {
	u8int reserved;				// Does exist if set
	u8int channel;				// Secondary if set
	u8int drive;				// Slave if set
	u16int type;				// ATAPI if set
	u16int sig;
	u16int cap;					// Drive features
	u32int commandsets;
	u32int size;				// Drive size in sectors
	char model[41];				// Model string
};

void init_ide(u32int BAR0, u32int BAR1, u32int BAR2, u32int BAR3, u32int BAR4);
u8int ide_read_sectors(u8int drive, u32int lba, u8int numsects, void *edi);
u8int ide_write_sectors(u8int drive, u32int lba, u8int numsects, void *esi);
u8int ide_atapi_eject(u8int drive);

#endif
