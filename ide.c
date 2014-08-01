/* ide.c - Enumerate, read, write and control IDE drives */

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

#include <common.h>
#include <ide.h>
#include <printf.h>
#include <timer.h>
#include <idt.h>
#include <dev.h>

struct IDEChannelRegisters channels[2];
struct IDEDevice ide_devices[4];
u8int ide_buf[2048] = {0};
u8int ide_irq_invoked = 0;
u8int atapi_packet[] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

u32int ide_read_sectors(u32int drive, u32int lba, u32int numsects, void *edi);
u32int ide_write_sectors(u32int drive, u32int lba, u32int numsects,
		const void *esi);

static void ide_irq(registers_t *regs) {
	regs = regs;
	ide_irq_invoked = 1;
}

void ide_write(u8int channel, u8int reg, u8int data) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nEIN);
	if (reg < 0x08)
		outb(channels[channel].base + reg - 0x00, data);
	else if (reg < 0x0C)
		outb(channels[channel].base + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(channels[channel].ctrl + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(channels[channel].bmide + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nEIN);
}

u8int ide_read(u8int channel, u8int reg) {
	u8int ret = 0;
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nEIN);
	if (reg < 0x08)
		ret = inb(channels[channel].base + reg - 0x00);
	else if (reg < 0x0C)
		ret = inb(channels[channel].base + reg - 0x06);
	else if (reg < 0x0E)
		ret = inb(channels[channel].ctrl + reg - 0x0A);
	else if (reg < 0x16)
		ret = inb(channels[channel].bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nEIN);
	return ret;
}

void ide_read_buffer(u8int channel, u8int reg, void *buffer, u32int count) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nEIN);
	if (reg < 0x08)
		insw(channels[channel].base + reg - 0x00, buffer, count);
	else if (reg < 0x0C)
		insw(channels[channel].base + reg - 0x06, buffer, count);
	else if (reg < 0x0E)
		insw(channels[channel].ctrl + reg - 0x0A, buffer, count);
	else if (reg < 0x16)
		insw(channels[channel].bmide + reg - 0x0E, buffer, count);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nEIN);
}

u8int ide_polling(u8int channel, u32int advanced_check) {
	int i;
	// Delay 400 ns for BSY to be set
	for (i = 0; i < 4; i++)
		// Reading alternate status port wastes 100 ns
		ide_read(channel, ATA_REG_ALTSTATUS);
	// Wait for BSY to clear
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		continue;

	if (advanced_check) {
		u8int state = ide_read(channel, ATA_REG_STATUS);
		// Check for errors
		if (state & ATA_SR_ERR)
			return 2;
		// Check for device fault
		if (state & ATA_SR_DF)
			return 1;
		// Check DRQ
		if (!(state & ATA_SR_DRQ))
			return 3; // DRQ should be set
	}

	return 0;
}

u8int ide_print_err(u8int drive, u8int err) {
	if (!err)
		return err;

	printf("IDE:\n\t");
	if (err == 1) {printf("- Device fault\n\t"); err = 19;}
	else if (err == 2) {
		u8int st = ide_read(ide_devices[drive].channel, ATA_REG_ERROR);
		if (st & ATA_ER_AMNF) {printf("- Address mark not found\n\t");
			err = 7;}
		if (st & ATA_ER_TK0NF) {printf("- No media or media error\n\t");
			err = 3;}
		if (st & ATA_ER_ABRT) {printf("- Command aborted\n\t"); err = 20;}
		if (st & ATA_ER_MCR) {printf("- No media or media error\n\t");
			err = 3;}
		if (st & ATA_ER_IDNF) {printf("- ID mark not found\n\t"); err = 21;}
		if (st & ATA_ER_MC) {printf("- No media or media error\n\t");
			err = 3;}
		if (st & ATA_ER_UNC) {printf("- Uncorrectable data error\n\t");
			err = 22;}
		if (st & ATA_ER_BBK) {printf("- Bad sectors\n\t"); err = 13;}
	} else if (err == 3) {printf("- Reads nothing\n\t"); err = 23;}
	else if (err == 4) {printf("- Write protected\n\t"); err = 8;}
	printf("- [%s %s] %s\n",
			(char *[]){"Primary", "Secondary"}[ide_devices[drive].channel],
			(char *[]){"master", "slave"}[ide_devices[drive].drive],
			ide_devices[drive].model);

	return err;
}

void init_ide(u32int BAR0, u32int BAR1, u32int BAR2, u32int BAR3,
		u32int BAR4) {
	u32int i, j, k, count = 0, sizes[4] = {0,};

	register_interrupt_handler(IRQ14, ide_irq);
	register_interrupt_handler(IRQ15, ide_irq);

	// Detect IO ports
	channels[ATA_PRIMARY].base = (BAR0 & 0xFFFFFFFC) + 0x01F0 * (!BAR0);
	channels[ATA_PRIMARY].ctrl = (BAR1 & 0xFFFFFFFC) + 0x03F4 * (!BAR1);
	channels[ATA_SECONDARY].base = (BAR2 & 0xFFFFFFFC) + 0x0170 * (!BAR2);
	channels[ATA_SECONDARY].ctrl = (BAR3 & 0xFFFFFFFC) + 0x0374 * (!BAR3);
	channels[ATA_PRIMARY].bmide = (BAR4 & 0xFFFFFFFC);
	channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8;

	// Disable IRQs by setting bit 1
	ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
	ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

	// Detect ATA/ATAPI devices
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++) {
			u8int err = 0, type = IDE_ATA, status;
			ide_devices[count].reserved = 0; // Assume no device

			// Select drive
			ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
			wait(1);

			// Send ATA identify command
			ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			wait(1);

			// Polling
			if (ide_read(i, ATA_REG_STATUS) == 0)
				continue;	// Status 0 means no device

			while (1) {
				status = ide_read(i, ATA_REG_STATUS);
				if (status & ATA_SR_ERR) {err = 1; break;} // Device is ATA
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			// Probe for ATAPI
			if (err) {
				u8int cl = ide_read(i, ATA_REG_LBA1);
				u8int ch = ide_read(i, ATA_REG_LBA2);

				if (cl == 0x14 && ch == 0xEB)
					type = IDE_ATAPI;
				else if (cl == 0x69 && ch == 0x96)
					type = IDE_ATAPI;
				else
					continue;

				ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				wait(1);
			}

			// Read id space
			ide_read_buffer(i, ATA_REG_DATA, ide_buf, 128);

			// Read device parameters
			ide_devices[count].reserved = 1;
			ide_devices[count].channel = i;
			ide_devices[count].drive = j;
			ide_devices[count].type = type;
			ide_devices[count].sig =
				*(u16int *)(ide_buf + ATA_IDENT_DEVICETYPE);
			ide_devices[count].cap =
				*(u16int *)(ide_buf + ATA_IDENT_CAPABILITIES);
			ide_devices[count].commandsets =
				*(u32int *)(ide_buf + ATA_IDENT_COMMANDSETS);

			// Get size
			// Check if device uses 48-bit addressing
			if (ide_devices[count].commandsets & (1 << 26))
				ide_devices[count].size =
					*(u32int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT);
			else
				ide_devices[count].size = sizes[count] =
					*(u32int *)(ide_buf + ATA_IDENT_MAX_LBA);

			// Get device string
			for (k = 0; k < 40; k += 2) {
				ide_devices[count].model[k] =
					ide_buf[ATA_IDENT_MODEL + k + 1];
				ide_devices[count].model[k + 1] =
					ide_buf[ATA_IDENT_MODEL + k];
			}
			ide_devices[count].model[40] = '\0';

			count++;
		}

	for (i = 0; i < 4; i++)
		if (ide_devices[i].reserved)
			printf("Found %s%i-%i %iMB - %s\n",
					(char *[]){"ATA", "ATAPI"}[ide_devices[i].type],
					i / 2, i % 2, ide_devices[i].size / 1024 / 2,
					ide_devices[i].model);

	int major =
		register_blkdev(IDE_MAJOR, "IDE", ide_read_sectors, ide_write_sectors,
				NULL, count, 512, sizes);

	if (major < 0)
		printf("IDE: Error registering blkdev driver\n");
}

/* ATA/ATAPI Read/Write Modes:
 * ++++++++++++++++++++++++++++++++
 *  Addressing Modes:
 *  ================
 *   - LBA28 Mode.     (+)
 *   - LBA48 Mode.     (+)
 *   - CHS.            (+)
 *  Reading Modes:
 *  ================
 *   - PIO Modes (0 : 6)       (+) // Slower than DMA, but not a problem.
 *   - Single Word DMA Modes (0, 1, 2).
 *   - Double Word DMA Modes (0, 1, 2).
 *   - Ultra DMA Modes (0 : 6).
 *  Polling Modes:
 *  ================
 *   - IRQs
 *   - Polling Status   (+) // Suitable for Singletasking
 */
u8int ide_ata_access(u8int direction, u8int drive, u32int lba,
		u8int numsects, void *edi) {
	u8int lba_mode, /* 0: CHS, 1: LBA28, 2: LBA48 */ dma, cmd = 0, lba_io[6];
	u32int channel = ide_devices[drive].channel;
	u32int slave = ide_devices[drive].drive;
	u32int bus = channels[channel].base, words = 256;
	u16int cyl, i;
	u8int head, sect, err;

	ide_write(channel, ATA_REG_CONTROL,
			channels[channel].nEIN = (ide_irq_invoked = 0) + 2);

	// Select CHS, LBA28 or 48
	if (lba >= 0x10000000) {
		// LBA48
		lba_mode = 2;
		lba_io[0] = lba & 0xFF;
		lba_io[1] = (lba & 0xFF00) >> 8;
		lba_io[2] = (lba & 0xFF0000) >> 16;
		lba_io[3] = (lba & 0xFF000000) >> 24;
		lba_io[4] = 0;	// 32 bits is enough for 2 TB
		lba_io[5] = 0;
		head = 0;
	} else if (ide_devices[drive].cap & 0x200) { // Supports LBA
		// LBA28
		lba_mode = 1;
		lba_io[0] = lba & 0xFF;
		lba_io[1] = (lba & 0xFF00) >> 8;
		lba_io[2] = (lba & 0xFF0000) >> 16;
		lba_io[3] = 0; // unused
		lba_io[4] = 0;
		lba_io[5] = 0;
		head = (lba & 0xF000000) >> 24;
	} else {
		// CHS
		lba_mode = 0;
		sect = (lba % 63) + 1;
		cyl = (lba + 1 - sect) / (63*16);
		lba_io[0] = sect;
		lba_io[1] = cyl & 0xFF;
		lba_io[2] = (cyl >> 8) & 0xFF;
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		head = (lba + 1 - sect) % (63 * 16) / (63);
	}

	// See if drive supports DMA
	dma = 0; // TODO

	// Wait if drive is busy
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		continue;

	// Select drive
	if (!lba_mode) {
		 // Drive and CHS
		ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slave << 4) | head);
	} else {
		//Drive and LBA
		ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slave << 4) | head);
	}

	// Wait 400ns
	for (i = 0; i < 4; i++)
		ide_read(channel, ATA_REG_ALTSTATUS);

	// Write parameters
	if (lba_mode == 2) {
		ide_write(channel, ATA_REG_SECCOUNT1, 0);
		ide_write(channel, ATA_REG_LBA3, lba_io[3]);
		ide_write(channel, ATA_REG_LBA4, lba_io[4]);
		ide_write(channel, ATA_REG_LBA5, lba_io[5]);
	}
	ide_write(channel, ATA_REG_SECCOUNT0, numsects);
	ide_write(channel, ATA_REG_LBA0, lba_io[0]);
	ide_write(channel, ATA_REG_LBA1, lba_io[1]);
	ide_write(channel, ATA_REG_LBA2, lba_io[2]);

	if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
	else if (lba_mode == 1 && dma == 0 && direction == 0)
		cmd = ATA_CMD_READ_PIO;
	else if (lba_mode == 2 && dma == 0 && direction == 0)
		cmd = ATA_CMD_READ_PIO_EXT;
	else if (lba_mode == 0 && dma == 1 && direction == 0)
		cmd = ATA_CMD_READ_DMA;
	else if (lba_mode == 1 && dma == 1 && direction == 0)
		cmd = ATA_CMD_READ_DMA;
	else if (lba_mode == 2 && dma == 1 && direction == 0)
		cmd = ATA_CMD_READ_DMA_EXT;
	else if (lba_mode == 0 && dma == 0 && direction == 1)
		cmd = ATA_CMD_WRITE_PIO;
	else if (lba_mode == 1 && dma == 0 && direction == 1)
		cmd = ATA_CMD_WRITE_PIO;
	else if (lba_mode == 2 && dma == 0 && direction == 1)
		cmd = ATA_CMD_WRITE_PIO_EXT;
	else if (lba_mode == 0 && dma == 1 && direction == 1)
		cmd = ATA_CMD_WRITE_DMA;
	else if (lba_mode == 1 && dma == 1 && direction == 1)
		cmd = ATA_CMD_WRITE_DMA;
	else if (lba_mode == 2 && dma == 1 && direction == 1)
		cmd = ATA_CMD_WRITE_DMA_EXT;
	ide_write(channel, ATA_REG_COMMAND, cmd);	// Send command

	if (dma) {} // TODO
	else {
		if (direction == 0) // PIO read
			for (i = 0; i < numsects; i++) {
				if ((err = ide_polling(channel, 1)))
					return err;
				insw(bus, edi, words);
				edi += words * 2;
			}
		else { // PIO write
			for (i = 0; i < numsects; i++) {
				ide_polling(channel, 0);
				outsw(bus, edi, words);
				edi += words * 2;
			}
			ide_write(channel, ATA_REG_COMMAND,
					(char []) {ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH,
					ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
			ide_polling(channel, 0);
		}
	}

	return 0;
}

void ide_wait_irq(void) {
	while (!ide_irq_invoked) {}
	ide_irq_invoked = 0;
}

u32int ide_atapi_read(u32int drive, u32int lba, u32int numsects, void *edi) {
	u32int channel = ide_devices[drive].channel;
	u32int slave = ide_devices[drive].drive;
	u32int bus = channels[channel].base;
	u32int words = 1024, i;
	u8int err;

	// Enable IRQs
	ide_write(channel, ATA_REG_CONTROL,
			channels[channel].nEIN = ide_irq_invoked = 0);

	// Set up SCSI packet
	atapi_packet[0] = ATAPI_CMD_READ;
	atapi_packet[1] = 0;
	atapi_packet[2] = (lba >> 24) & 0xFF;
	atapi_packet[3] = (lba >> 16) & 0xFF;
	atapi_packet[4] = (lba >> 8) & 0xFF;
	atapi_packet[5] = lba & 0xFF;
	atapi_packet[6] = 0;
	atapi_packet[7] = 0;
	atapi_packet[8] = 0;
	atapi_packet[9] = numsects;
	atapi_packet[10] = 0;
	atapi_packet[11] = 0;

	// Select drive
	ide_write(channel, ATA_REG_HDDEVSEL, slave << 4);

	// Wait 400ns
	for (i = 0; i < 4; i++)
		ide_read(channel, ATA_REG_ALTSTATUS);

	// Tell controller to use PIO
	ide_write(channel, ATA_REG_FEATURES, 0);

	// Tell controller size of the buffer
	ide_write(channel, ATA_REG_LBA1, (words * 2) & 0xFF);
	ide_write(channel, ATA_REG_LBA1, (words * 2) >> 8);

	// Send packet command
	ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

	// Wait for error/success
	if ((err = ide_polling(channel, 1)))
		return err;

	// Send packet data
	outsw(bus, atapi_packet, 6);

	// Receive data
	for (i = 0; i < numsects; i++) {
		ide_wait_irq();
		if ((err = ide_polling(channel, 1)))
			return err;
		insw(bus, edi, words);
		edi += words * 2;
	}

	// Wait for IRQ
	ide_wait_irq();

	// Wait for busy and DRQ to clear
	while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ)) {}

	return 0;
}

u32int ide_read_sectors(u32int drive, u32int lba, u32int numsects,
		void *edi) {
	u32int i;

	// Check if drive present
	if (drive > 3 || !ide_devices[drive].reserved)
		return 1;

	// Check for valid input
	else if ((lba + numsects) > ide_devices[drive].size &&
			ide_devices[drive].type == IDE_ATA)
		return 2;

	// Read
	else {
		u8int err = 0;
		if (ide_devices[drive].type == IDE_ATA)
			err = ide_ata_access(ATA_READ, drive, lba, numsects, edi);
		else
			for (i = 0; i < numsects; i++)
				err = ide_atapi_read(drive, lba + i, 1, edi + i * 2048);
		return ide_print_err(drive, err);
	}
}

u32int ide_write_sectors(u32int drive, u32int lba, u32int numsects,
		const void *esi) {
	// Check if drive present
	if (drive > 3 || !ide_devices[drive].reserved)
		return 1;

	// Check for valid input
	else if ((lba + numsects) > ide_devices[drive].size &&
			ide_devices[drive].type == IDE_ATA)
		return 2;

	// Write
	else {
		u8int err;
		if (ide_devices[drive].type == IDE_ATA)
			err = ide_ata_access(ATA_WRITE, drive, lba, numsects,
					(void *)esi);
		else
			err = 4; // Write-protected
		return ide_print_err(drive, err);
	}
}

u8int ide_atapi_eject(u8int drive) {
	u32int channel = ide_devices[drive].channel;
	u32int slave = ide_devices[drive].drive;
	u32int bus = channels[channel].base;
	u32int i;
	u8int err = 0;

	// Check if present
	if (drive > 3 || !ide_devices[drive].reserved)
		return 1;

	// Check if ATAPI
	else if (ide_devices[drive].type == IDE_ATA)
		return 20;

	else {
		// Enable IRQs
		ide_write(channel, ATA_REG_CONTROL,
				channels[channel].nEIN = ide_irq_invoked = 0);

		// Setup SCSI packet
		atapi_packet[0] = ATAPI_CMD_EJECT;
		atapi_packet[1] = 0;
		atapi_packet[2] = 0;
		atapi_packet[3] = 0;
		atapi_packet[4] = 2;
		atapi_packet[5] = 0;
		atapi_packet[6] = 0;
		atapi_packet[7] = 0;
		atapi_packet[8] = 0;
		atapi_packet[9] = 0;
		atapi_packet[10] = 0;
		atapi_packet[11] = 0;

		// Select drive
		ide_write(channel, ATA_REG_HDDEVSEL, slave << 4);

		// Wait 400ns
		for (i = 0; i < 4; i++)
			ide_read(channel, ATA_REG_ALTSTATUS);

		// Send packet command
		ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

		// Wait for success or failure
		if (!(err = ide_polling(channel, 1))) {
			outsw(bus, atapi_packet, 6);
			ide_wait_irq();
			err = ide_polling(channel, 1);
			if ((err = 3)) err = 0; // DRQ isn't needed
		}
		return ide_print_err(drive, err);
	}
}
