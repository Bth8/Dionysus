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
#include <pci/ide.h>
#include <printf.h>
#include <timer.h>
#include <idt.h>
#include <kmalloc.h>
#include <string.h>
#include <pci.h>
#include <pci_regs.h>
#include <pci_ids.h>
#include <errno.h>
#include <block.h>

extern volatile uint32_t tick;
uint8_t ide_irq_invoked = 0;

int32_t open(fs_node_t *node, uint32_t flags) { return 0; }
int32_t close(fs_node_t *node) { return 0; }

struct file_ops ide_fops = {
	.open = open,
	.close = close,
};

struct pci_dev_id ide_ids[] = {
	PCI_DEVICE_CLASS(PCI_CLASS_STORAGE_IDE, PCI_ANY),
	{ 0, }
};

static void ide_irq(registers_t *regs) {
	ide_irq_invoked = 1;
}

static void ide_write(struct IDEChannelRegisters *channel, uint8_t reg, uint8_t data) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channel->nEIN);
	if (reg < 0x08)
		outb(channel->base + reg, data);
	else if (reg < 0x0C)
		outb(channel->base + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(channel->ctrl + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(channel->bmide + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channel->nEIN);
}

static uint8_t ide_read(struct IDEChannelRegisters *channel, uint8_t reg) {
	uint8_t ret = 0;
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channel->nEIN);
	if (reg < 0x08)
		ret = inb(channel->base + reg);
	else if (reg < ATA_REG_ALTSTATUS)
		ret = inb(channel->base + reg - 0x06);
	else if (reg < 0x0E)
		ret = inb(channel->ctrl + reg - 0x0A);
	else if (reg < 0x16)
		ret = inb(channel->bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channel->nEIN);
	return ret;
}

static void ide_read_buffer(struct IDEChannelRegisters *channel, uint8_t reg,
		void *buffer, uint32_t count) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channel->nEIN);
	if (reg < 0x08)
		insw(channel->base + reg, buffer, count);
	else if (reg < 0x0C)
		insw(channel->base + reg - 0x06, buffer, count);
	else if (reg < 0x0E)
		insw(channel->ctrl + reg - 0x0A, buffer, count);
	else if (reg < 0x16)
		insw(channel->bmide + reg - 0x0E, buffer, count);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channel->nEIN);
}

static uint8_t ide_polling(struct IDEChannelRegisters *channel,
		uint32_t advanced_check) {
	int i;
	// Delay 400 ns for BSY to be set
	for (i = 0; i < 4; i++)
		// Reading alternate status port wastes 100 ns
		ide_read(channel, ATA_REG_ALTSTATUS);
	// Wait for BSY to clear
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		continue;

	if (advanced_check) {
		uint8_t state = ide_read(channel, ATA_REG_STATUS);
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

static uint8_t ide_print_err(struct IDEDevice *dev, uint8_t err) {
	if (!err)
		return err;

	printf("IDE:\n\t");
	if (err == 1) {printf("- Device fault\n\t"); err = 19;}
	else if (err == 2) {
		uint8_t st = ide_read(&dev->channel, ATA_REG_ERROR);
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
		(char *[]){"Primary", "Secondary"}[dev->channel.channel],
		(char *[]){"master", "slave"}[dev->drive],
		dev->model);

	return err;
}

static int ide_blkdev_create(struct IDEDevice *ide) {
	blkdev_t *dev = alloc_blkdev();
	if (!dev)
		return -ENOMEM;

	dev->major = IDE_MAJOR;
	dev->minor = 16 * (2 * ide->channel.channel + ide->drive);
	dev->max_part = 16;
	dev->sector_size = IDE_SECTOR_SIZE;
	dev->size = ide->size * IDE_SECTOR_SIZE / KERNEL_BLOCKSIZE;
	dev->lock = 1;
	dev->handler = NULL;
	dev->private_data = ide;

	if (add_blkdev(dev) < 0)
		return -EEXIST;

	while (autopopulate_blkdev(dev) < 0)
		continue;

	return 0;
}

static int ide_probe(struct pci_dev *pci, const struct pci_dev_id *id) {
	if (claim_pci_dev(pci) < 0)
		return 0;

	uint32_t BAR0 = pciConfigReadDword(pci->bus->secondary, pci->slot, 
		pci->func, PCI_BASE_ADDRESS_0);
	uint32_t BAR1 = pciConfigReadDword(pci->bus->secondary, pci->slot, 
		pci->func, PCI_BASE_ADDRESS_1);
	uint32_t BAR2 = pciConfigReadDword(pci->bus->secondary, pci->slot, 
		pci->func, PCI_BASE_ADDRESS_2);
	uint32_t BAR3 = pciConfigReadDword(pci->bus->secondary, pci->slot, 
		pci->func, PCI_BASE_ADDRESS_3);
	uint32_t BAR4 = pciConfigReadDword(pci->bus->secondary, pci->slot, 
		pci->func, PCI_BASE_ADDRESS_4);

	struct IDEChannelRegisters channels[2];
	// Detect IO ports
	channels[0].channel = 0;
	channels[0].base = (BAR0 & 0xFFFFFFFC) + 0x01F0 * (!(BAR0 & 0xFFFFFFFC));
	channels[0].ctrl = (BAR1 & 0xFFFFFFFC) + 0x03F6 * (!(BAR0 & 0xFFFFFFFC));
	channels[0].bmide = (BAR4 & 0xFFFFFFFC);
	channels[0].nEIN = 0;
	channels[1].channel = 1;
	channels[1].base = (BAR2 & 0xFFFFFFFC) + 0x0170 * (!(BAR0 & 0xFFFFFFFC));
	channels[1].ctrl = (BAR3 & 0xFFFFFFFC) + 0x0376 * (!(BAR0 & 0xFFFFFFFC));
	channels[1].bmide = (BAR4 & 0xFFFFFFFC) + 8;
	channels[1].nEIN = 0;

	// Disable IRQs by setting bit 1
	ide_write(&channels[0], ATA_REG_CONTROL, 2);
	ide_write(&channels[1], ATA_REG_CONTROL, 2);

	int i, j;
	// Detect ATA/ATAPI devices
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++) {
			uint8_t type = IDE_ATA;

			// Select drive
			ide_write(&channels[i], ATA_REG_HDDEVSEL, j << 4);
			sleep_until(tick + 2 * HZ / 1000);

			// Send ATA identify command
			ide_write(&channels[i], ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			sleep_until(tick + 2 * HZ / 1000);

			// Polling
			if (ide_read(&channels[i], ATA_REG_STATUS) == 0)
				continue;	// Status 0 means no device

			uint8_t err = 0;
			while (1) {
				uint8_t status = ide_read(&channels[i], ATA_REG_STATUS);
				if (status & ATA_SR_ERR) {err = 1; break;} // Device is ATA
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			// Probe for ATAPI
			if (err) {
				uint8_t cl = ide_read(&channels[i], ATA_REG_LBA1);
				uint8_t ch = ide_read(&channels[i], ATA_REG_LBA2);

				if (cl == 0x14 && ch == 0xEB)
					type = IDE_ATAPI;
				else if (cl == 0x69 && ch == 0x96)
					type = IDE_ATAPI;
				else
					continue;

				ide_write(&channels[i], ATA_REG_COMMAND,
					ATA_CMD_IDENTIFY_PACKET);
				sleep_until(tick + 1 * HZ / 1000);
			}

			char *buff = kmalloc(128);
			ASSERT(buff);
			// Read id space
			ide_read_buffer(&channels[i], ATA_REG_DATA, buff, 128);

			struct IDEDevice *dev =
				(struct IDEDevice *)kmalloc(sizeof(struct IDEDevice));
			ASSERT(dev);

			// Read device parameters
			memcpy(&(dev->channel), &channels[i],
				sizeof(struct IDEChannelRegisters));
			dev->drive = j;
			dev->type = type;
			dev->sig = *(uint16_t *)(buff + ATA_IDENT_DEVICETYPE);
			dev->cap = *(uint16_t *)(buff + ATA_IDENT_CAPABILITIES);
			dev->commandsets = *(uint32_t *)(buff + ATA_IDENT_COMMANDSETS);

			// Get size
			// Check if device uses 48-bit addressing
			if (dev->commandsets & (1 << 26))
				dev->size = *(uint32_t *)(buff + ATA_IDENT_MAX_LBA_EXT);
			else
				dev->size = *(uint32_t *)(buff + ATA_IDENT_MAX_LBA);

			// Get device string
			int k;
			for (k = 0; k < 40; k += 2) {
				dev->model[k] = buff[ATA_IDENT_MODEL + k + 1];
				dev->model[k + 1] = buff[ATA_IDENT_MODEL + k];
			}
			dev->model[40] = '\0';

			kfree(buff);

			printf("Found %s%i-%i %iMB - %s\n",
				(char *[]){"ATA", "ATAPI"}[dev->type],
				i / 2, i % 2, dev->size * 512 / 1024 / 1024,
				dev->model);

			int32_t ret = ide_blkdev_create(dev);
			if (ret < 0) {
				kfree(dev);
				return ret;
			}
		}

		return 0;
}

void init_ide(void) {
	int32_t ret;
	while ((ret = register_blkdev(IDE_MAJOR, "IDE", ide_fops)) == -ENOMEM)
		continue;

	if (ret < 0) {
		printf("IDE: error acquiring major\n");
		return;
	}

	while ((ret = register_pci("IDE", ide_ids, ide_probe)) == -ENOMEM)
		continue;

	if (ret < 0)
		printf("IDE: error registering pci\n");
}

static void request_pci(blkdev_t *dev) {
	node_t *node = dev->queue->head;
	while (node) {
		request_t *req = (request_t *)node->data;
		uint32_t sectors_xferred = 0; //ide_do_transfer(dev, req);
		if (end_request(req, sectors_xferred) == 0) {
			list_dequeue(dev->queue, node);
			free_request(req);
			kfree(node);
		}
		node = dev->queue->head;
	}
}
/*
static uint32_t ide_do_transfer(blkdev_t *dev, request_t *req) {
	struct PRD *prd_table =
		(struct PRD *)kmemalign(4, sizeof(struct PRD) * IDE_MAX_PRD);
	if (!prd_table)
		return 0;

	memset(prd_table, 0, sizeof(struct PRD) * IDE_MAX_PRD);

	node_t *node;
	int i = 0;
	foreach(node, req->bios) {
		if (i > 16)
			break;
		bio_t *bio = (bio_t *)node->data;
		prd_table[i].phys_addr = bio->page + bio->offset;
		prd-table[i].count = bio->nbytes;
		if (i == 15 || node == req->bios->tail)
			prd_table[i].EOT = 0x8000;
		else
			prd_table[i].EOT = 0x0000;
		i++;
	}

} */

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
 
 
uint8_t ide_ata_access(struct IDEDevice *dev, uint32_t lba, uint32_t numsects,
		void *edi, uint32_t dir) {
	uint8_t lba_mode, dma, cmd = 0, lba_io[6];
	uint32_t channel = ide_devices[drive].channel;
	uint32_t slave = ide_devices[drive].drive;
	uint32_t bus = channels[channel].base, words = 256;
	uint16_t cyl, i;
	uint8_t head, sect, err;

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

uint32_t ide_atapi_read(uint32_t drive, uint32_t lba, uint32_t numsects, void *edi) {
	uint32_t channel = ide_devices[drive].channel;
	uint32_t slave = ide_devices[drive].drive;
	uint32_t bus = channels[channel].base;
	uint32_t words = 1024, i;
	uint8_t err;

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

uint32_t ide_read_sectors(uint32_t drive, uint32_t lba, uint32_t numsects,
		void *edi) {
	uint32_t i;

	// Check if drive present
	if (drive > 3 || !ide_devices[drive].reserved)
		return 1;

	// Check for valid input
	else if ((lba + numsects) > ide_devices[drive].size &&
			ide_devices[drive].type == IDE_ATA)
		return 2;

	// Read
	else {
		uint8_t err = 0;
		if (ide_devices[drive].type == IDE_ATA)
			err = ide_ata_access(ATA_READ, drive, lba, numsects, edi);
		else
			for (i = 0; i < numsects; i++)
				err = ide_atapi_read(drive, lba + i, 1, edi + i * 2048);
		return ide_print_err(drive, err);
	}
}

uint32_t ide_write_sectors(uint32_t drive, uint32_t lba, uint32_t numsects,
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
		uint8_t err;
		if (ide_devices[drive].type == IDE_ATA)
			err = ide_ata_access(ATA_WRITE, drive, lba, numsects,
					(void *)esi);
		else
			err = 4; // Write-protected
		return ide_print_err(drive, err);
	}
}

uint8_t ide_atapi_eject(uint8_t drive) {
	uint32_t channel = ide_devices[drive].channel;
	uint32_t slave = ide_devices[drive].drive;
	uint32_t bus = channels[channel].base;
	uint32_t i;
	uint8_t err = 0;

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
*/