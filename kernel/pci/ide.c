/* ide.c - Enumerate, read, write and control IDE drives */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
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
#include <paging.h>

extern volatile uint32_t tick;
uint8_t ide_irq_invoked = 0;

static int32_t open(fs_node_t *node, uint32_t flags) { return 0; }
static int32_t close(fs_node_t *node) { return 0; }
static int32_t request_ide(blkdev_t *dev);

struct file_ops ide_fops = {
	.open = open,
	.close = close,
};

struct pci_dev_id ide_ids[] = {
	PCI_DEVICE_CLASS(PCI_CLASS_STORAGE_IDE, PCI_ANY),
	{ 0, }
};

waitqueue_t *ide_wq = NULL;

static void wake_ide() {
	wake_queue(ide_wq);
}

static void ide_irq(registers_t *regs) {
	irq_ack(regs->int_no);
	wake_ide();
}

static void ide_write(struct IDEChannelRegisters *channel, uint8_t reg, uint8_t data) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channel->nEIN);
	if (reg < 0x08)
		outb(channel->base + reg, data);
	else if (reg < 0x0C)
		outb(channel->base + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(channel->ctrl + reg - 0x0C, data);
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
	else if (reg < 0x0C)
		ret = inb(channel->base + reg - 0x06);
	else if (reg < 0x0E)
		ret = inb(channel->ctrl + reg - 0x0C);
	else if (reg < 0x16)
		ret = inb(channel->bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channel->nEIN);
	return ret;
}

// Fills buffer with count words
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

static void ide_print_err(struct IDEDevice *dev, uint8_t status) {
	if (status == 0)
		return;

	ASSERT(dev);

	printf("IDE:\n\t");
	if (status & ATA_SR_DF)
		printf("- Device fault\n\t");
	else if (status & ATA_SR_ERR) {
		uint8_t st = ide_read(dev->channel, ATA_REG_ERROR);
		if (st & ATA_ER_AMNF)
			printf("- Address mark not found\n\t");
		if (st & ATA_ER_TK0NF)
			printf("- No media or media error\n\t");
		if (st & ATA_ER_ABRT)
			printf("- Command aborted\n\t");
		if (st & ATA_ER_MCR)
			printf("- No media or media error\n\t");
		if (st & ATA_ER_IDNF)
			printf("- ID mark not found\n\t");
		if (st & ATA_ER_MC)
			printf("- No media or media error\n\t");
		if (st & ATA_ER_UNC)
			printf("- Uncorrectable data error\n\t");
		if (st & ATA_ER_BBK)
			printf("- Bad sectors\n\t");
	}

	printf("- [%s %s] %s\n",
		(char *[]){"Primary", "Secondary"}[dev->channel->channel],
		(char *[]){"master", "slave"}[dev->drive],
		dev->model);
}

static blkdev_t *ide_blkdev_create(struct IDEDevice *ide) {
	blkdev_t *dev = alloc_blkdev();
	if (!dev)
		return NULL;

	dev->major = IDE_MAJOR;
	dev->minor = 16 * (2 * ide->channel->channel + ide->drive);
	dev->max_part = 16;
	dev->sector_size = IDE_SECTOR_SIZE;
	dev->size = ide->size;
	dev->handler = request_ide;
	dev->private_data = ide;

	return dev;
}

static void waste400(struct IDEChannelRegisters *channel) {
	int i;
	for (i = 0; i < 4; i++)
		ide_read(channel, ATA_REG_ALTSTATUS);
}

static void ide_tasklet(void *argp);

static int ide_probe(struct pci_dev *pci, const struct pci_dev_id *id) {
	if (claim_pci_dev(pci) < 0)
		return 0;

	blkdev_t *blkdevs[4] = {NULL, NULL, NULL, NULL};

	uint16_t command = pciConfigReadWord(pci->bus->secondary, pci->slot,
		pci->func, PCI_COMMAND);
	command |= PCI_COMMAND_MASTER;
	pciConfigWriteWord(pci->bus->secondary, pci->slot, pci->func, PCI_COMMAND,
		command);
	command = pciConfigReadWord(pci->bus->secondary, pci->slot, pci->func,
		PCI_COMMAND);

	if (!(command & PCI_COMMAND_MASTER)) {
		release_pci_dev(pci);
		return 0;
	}

	uint8_t prog = pciConfigReadByte(pci->bus->secondary, pci->slot,
		pci->func, PCI_CLASS_PROG);

	if (prog & 0x02)
		prog |= 0x01;
	if (prog & 0x08)
		prog |= 0x04;

	pciConfigWriteByte(pci->bus->secondary, pci->slot, pci->func,
		PCI_CLASS_PROG, prog);
	prog = pciConfigReadByte(pci->bus->secondary, pci->slot, pci->func,
		PCI_CLASS_PROG);

	uint32_t BAR0 = 0x01F0;
	uint32_t BAR1 = 0x03F6;
	uint32_t BAR2 = 0x0170;
	uint32_t BAR3 = 0x0376;
	uint32_t BAR4 = pciConfigReadDword(pci->bus->secondary, pci->slot, 
		pci->func, PCI_BASE_ADDRESS_4);

	register_interrupt_handler(IRQ14, ide_irq);
	register_interrupt_handler(IRQ15, ide_irq);

	if (prog & 0x01) {
		BAR0 = pciConfigReadDword(pci->bus->secondary, pci->slot, pci->func,
			PCI_BASE_ADDRESS_0);
		BAR1 = pciConfigReadDword(pci->bus->secondary, pci->slot, pci->func,
			PCI_BASE_ADDRESS_1);
	}

	if (prog & 0x04) {
		BAR2 = pciConfigReadDword(pci->bus->secondary, pci->slot, pci->func,
			PCI_BASE_ADDRESS_2);
		BAR3 = pciConfigReadDword(pci->bus->secondary, pci->slot, pci->func,
			PCI_BASE_ADDRESS_3);
	}

	int i, j;
	// Detect ATA/ATAPI devices
	for (i = 0; i < 2; i++) {
		int refs = 0;
		struct IDEChannelRegisters *channel = 
			(struct IDEChannelRegisters *)kmalloc(sizeof(struct IDEChannelRegisters));
		if (!channel)
			break;

		channel->channel = i;
		channel->nEIN = 2;
		channel->bmide = BAR4 + 8 * i;

		if (i == 0) {
			channel->base = BAR0;
			channel->ctrl = BAR1;
		} else {
			channel->base = BAR2;
			channel->ctrl = BAR3;
		}

		channel->mutex = create_mutex(0);
		if (!channel->mutex) {
			kfree(channel);
			goto fail;
		}

		for (j = 0; j < 2; j++) {
			uint8_t type = IDE_ATA;

			while (ide_read(channel, ATA_REG_ALTSTATUS) & ATA_SR_BSY)
				continue;

			// Select drive
			ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));

			waste400(channel);

			while (ide_read(channel, ATA_REG_ALTSTATUS) & ATA_SR_BSY)
				continue;

			// Disable interrupts
			ide_write(channel, ATA_REG_CONTROL, 2);

			// Send ATA identify command
			ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

			// Polling
			if (ide_read(channel, ATA_REG_ALTSTATUS) == 0)
				continue;	// Status 0 means no device

			uint8_t err = 0;
			while (1) {
				uint8_t status = ide_read(channel, ATA_REG_ALTSTATUS);
				if (status & ATA_SR_ERR) {err = 1; break;} // Device isn't ATA
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			// Probe for ATAPI
			if (err) {
				continue; // Because we don't support ATAPI yet
				uint8_t cl = ide_read(channel, ATA_REG_LBA1);
				uint8_t ch = ide_read(channel, ATA_REG_LBA2);

				if (cl == 0x14 && ch == 0xEB)
					type = IDE_ATAPI;
				else if (cl == 0x69 && ch == 0x96)
					type = IDE_ATAPI;
				else
					continue;

				ide_write(channel, ATA_REG_COMMAND,
					ATA_CMD_IDENTIFY_PACKET);
			}

			unsigned char *buff = kmalloc(sizeof(uint16_t) * 256);
			if (!buff)
				break;

			// Read id space
			ide_read_buffer(channel, ATA_REG_DATA, buff, 256);

			struct IDEDevice *dev =
				(struct IDEDevice *)kmalloc(sizeof(struct IDEDevice));
			if (!dev) {
				kfree(buff);
				break;
			}

			memset(dev, 0, sizeof(struct IDEDevice));

			// Read device parameters
			dev->channel = channel;
			refs++;
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
				dev->channel->channel, dev->drive,
				dev->size * IDE_SECTOR_SIZE / 1024 / 1024,
				dev->model);

			blkdevs[2 * i + j] = ide_blkdev_create(dev);
			if (blkdevs[2 * i + j] == NULL) {
				refs--;
				kfree(dev);
				break;
			}

			dev->servicer = create_tasklet(ide_tasklet, "[ide]",
				blkdevs[2 * i + j]);
			if (!dev->servicer) {
				refs--;
				break;
			}
		}
		if (refs == 0) {
			destroy_mutex(channel->mutex);
			kfree(channel);
		}

		if (j < 2)
			goto fail;
	}

	for (i = 0; i < 4; i++) {
		if (blkdevs[i]) {
			struct IDEDevice *ide = (struct IDEDevice *)blkdevs[i]->private_data;
			if (autopopulate_blkdev(blkdevs[i]) < 0) {
				printf("Error populating %s%i-%i\n",
					(char *[]){"ATA", "ATAPI"}[ide->type],
					ide->channel->channel, ide->drive);
				struct part *part = (struct part *)kmalloc(sizeof(struct part));
				if (!part) {
					destroy_tasklet(ide->servicer);
					kfree(ide);
					free_blkdev(blkdevs[i]);
					continue;
				}

				part->minor = blkdevs[i]->minor;
				part->offset = 0;
				part->size = ide->size;
				node_t *node = list_insert(blkdevs[i]->partitions, part);
				if (!node) {
					kfree(part);
					destroy_tasklet(ide->servicer);
					kfree(ide);
					free_blkdev(blkdevs[i]);
					continue;
				}
			}

			if (add_blkdev(blkdevs[i]) < 0) {
				if (i % 2 == 0 && !blkdevs[i + 1]) {
					destroy_mutex(ide->channel->mutex);
					kfree(ide->channel);
				}
				destroy_tasklet(ide->servicer);
				kfree(ide);
				free_blkdev(blkdevs[i]);
				continue;
			}
		}
	}

	return 0;

fail:
	for (i = 0; i < 4; i++) {
		if (blkdevs[i]) {
			struct IDEDevice *ide = (struct IDEDevice *)blkdevs[i]->private_data;
			if (i % 2 == 0) {
				destroy_mutex(ide->channel->mutex);
				kfree(ide->channel);
			}
			destroy_tasklet(ide->servicer);
			kfree(ide);
			free_blkdev(blkdevs[i]);
		}
	}

	return -ENOMEM;
}

void init_ide(void) {
	int32_t ret;

	ide_wq = create_waitqueue();
	if (!ide_wq) {
		printf("IDE: error creating waitqueue");
		return;
	}

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

static int32_t request_ide(blkdev_t *dev) {
	struct IDEDevice *ide = (struct IDEDevice *)dev->private_data;

	reset_and_reschedule(ide->servicer);

	return 0;
}

static int32_t ide_ata_access(struct IDEDevice *dev, request_t *req);

static void ide_tasklet(void *argp) {
	blkdev_t *dev = (blkdev_t *)argp;
	struct IDEDevice *ide = (struct IDEDevice *)dev->private_data;
	acquire_mutex(ide->channel->mutex);

	while (1) {
		node_t *node = next_ready_request_blkdev(dev);
		if (!node)
			break;

		request_t *req = (request_t *)node->data;
		int32_t sectors_xferred = ide_ata_access(ide, req);
		int32_t cont;

		if (sectors_xferred < 0)
			cont = end_request(req, 0, 0);
		else
			cont = end_request(req, 1, (uint32_t)sectors_xferred);

		if (cont == 0) {
			list_dequeue(dev->queue, node);
			kfree(node);
		}
	}

	release_mutex(ide->channel->mutex);
}

static int statusgood(uint8_t status) {
	if (status & ATA_SR_ERR)
		return 0;
	if (status & ATA_SR_DF)
		return 0;

	return 1;
}

static int32_t wait_irq(struct IDEDevice *dev, uint32_t timeout) {
	struct timer *timer = (struct timer *)kmalloc(sizeof(struct timer));
	if (!timer)
		return -ENOMEM;

	timer->expires = tick + timeout * HZ;
	timer->callback = wake_ide;
	if (add_timer(timer) < 0) {
		kfree(timer);
		return -ENOMEM;
	}

	int32_t ret = 0;

	uint32_t status;
	uint32_t interrupted = 0;
	do {
		status = ide_read(dev->channel, ATA_REG_STATUS);
		if (!(status & ATA_SR_BSY))
			break;

		timer->expires = tick + timeout * HZ;
		interrupted = sleep_thread(ide_wq, SLEEP_INTERRUPTABLE);
	} while (interrupted == 0);

	if (interrupted)
		ret = -EINTR;
	else if (ret == 0 && !statusgood(status)) {
		ide_print_err(dev, status);
		ret = -EIO;
	}

	del_timer(timer);
	kfree(timer);

	return ret;
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
 *   - PIO Modes (0 : 6)       (+)
 *   - Single Word DMA Modes (0, 1, 2).
 *   - Double Word DMA Modes (0, 1, 2).
 *   - Ultra DMA Modes (0 : 6).
 *  Polling Modes:
 *  ================
 *   - IRQs				(+)
 *   - Polling Status   (+) // Suitable for Singletasking
 */

static int32_t ide_ata_access(struct IDEDevice *dev, request_t *req) {
	uint8_t lba_io[6];
	uint32_t lba_ext = 0;
	uint32_t cmd = 0;
	uint32_t slave = dev->drive;
	uint32_t bus = dev->channel->base;
	uint32_t lba = req->first_sector;
	uint32_t dma = (dev->cap & ATA_CAP_DMA && 0) ? 1 : 0;
	uint32_t direction = (req->flags & BLOCK_DIR_WRITE) ? ATA_WRITE : ATA_READ;
	uint32_t head;

	// Select CHS, LBA28 or 48
	if (lba >= 0x10000000) {
		// LBA48
		lba_ext = 1;
		lba_io[0] = lba & 0xFF;
		lba_io[1] = (lba & 0xFF00) >> 8;
		lba_io[2] = (lba & 0xFF0000) >> 16;
		lba_io[3] = (lba & 0xFF000000) >> 24;
		lba_io[4] = 0;	// 32 bits is enough for 2 TB
		lba_io[5] = 0;
		head = 0;
	} else if (dev->cap & ATA_CAP_LBA) { // Supports LBA
		// LBA28
		lba_io[0] = lba & 0xFF;
		lba_io[1] = (lba & 0xFF00) >> 8;
		lba_io[2] = (lba & 0xFF0000) >> 16;
		lba_io[3] = 0; // unused
		lba_io[4] = 0;
		lba_io[5] = 0;
		head = (lba & 0x0F000000) >> 24;
	} else {
		// CHS
		uint32_t sect = (lba % 63) + 1;
		uint32_t cyl = (lba + 1 - sect) / (63*16);
		lba_io[0] = sect;
		lba_io[1] = cyl & 0xFF;
		lba_io[2] = (cyl >> 8) & 0xFF;
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		head = (lba + 1 - sect) % (63 * 16) / (63);
	}

	// Wait if drive is busy
	while (ide_read(dev->channel, ATA_REG_ALTSTATUS) & ATA_SR_BSY)
		continue;

	// Select drive and method
	if (dev->cap & ATA_CAP_LBA)
		ide_write(dev->channel, ATA_REG_HDDEVSEL, 0xE0 | (slave << 4) | head);
	else
		ide_write(dev->channel, ATA_REG_HDDEVSEL, 0xA0 | (slave << 4) | head);

	// Wait 400ns
	waste400(dev->channel);

	// Wait if drive is busy
	while (ide_read(dev->channel, ATA_REG_ALTSTATUS) & ATA_SR_BSY)
		continue;

	// Enable interrupts
	dev->channel->nEIN = 0;
	ide_write(dev->channel, ATA_REG_CONTROL, 0);

	// Write parameters
	if (lba_ext) {
		ide_write(dev->channel, ATA_REG_LBA3, lba_io[3]);
		ide_write(dev->channel, ATA_REG_LBA4, lba_io[4]);
		ide_write(dev->channel, ATA_REG_LBA5, lba_io[5]);
	}
	ide_write(dev->channel, ATA_REG_LBA0, lba_io[0]);
	ide_write(dev->channel, ATA_REG_LBA1, lba_io[1]);
	ide_write(dev->channel, ATA_REG_LBA2, lba_io[2]);

	if (!lba_ext && !dma && direction == ATA_READ)
		cmd = ATA_CMD_READ_PIO;
	else if (lba_ext && !dma && direction == ATA_READ)
		cmd = ATA_CMD_READ_PIO_EXT;
	else if (!lba_ext && dma && direction == ATA_READ)
		cmd = ATA_CMD_READ_DMA;
	else if (lba_ext && dma && direction == ATA_READ)
		cmd = ATA_CMD_READ_DMA_EXT;
	else if (!lba_ext && !dma && direction == ATA_WRITE)
		cmd = ATA_CMD_WRITE_PIO;
	else if (lba_ext && !dma && direction == ATA_WRITE)
		cmd = ATA_CMD_WRITE_PIO_EXT;
	else if (!lba_ext && dma && direction == ATA_WRITE)
		cmd = ATA_CMD_WRITE_DMA;
	else if (lba_ext && dma && direction == ATA_WRITE)
		cmd = ATA_CMD_WRITE_DMA_EXT;

	if (dma) {
	/*	uint32_t nsects = 0;
		struct PRD *prd_table =
			(struct PRD *)kmemalign(4, sizeof(struct PRD) * IDE_MAX_TRANSFER);
		if (!prd_table)
			return 0;

		memset(prd_table, 0, sizeof(struct PRD) * IDE_MAX_TRANSFER);

		node_t *node;
		foreach(node, req->bios) {
			bio_t *bio = (bio_t *)node->data;

			if (lba_ext) {
				if (nsects + bio->nbytes / IDE_SECTOR_SIZE > IDE_MAX_TRANSFER_48)
					break;
			} else {
				if (nsects + bio->nbytes / IDE_SECTOR_SIZE > IDE_MAX_TRANSFER_28)
					break;
			}

			prd_table[i].phys_addr = bio->page + bio->offset;
			prd-table[i].count = bio->nbytes;

			if (i == 15 || node == req->bios->tail)
				prd_table[i].EOT = 0x8000;
			else
				prd_table[i].EOT = 0x0000;
			nsects += bio->nbytes / KERNEL_BLOCKSIZE;
		} */
	} else {
		bio_t *bio = (bio_t *)req->bios->head->data;

		uint32_t nsects = bio->nsectors;

		if (lba_ext) {
			if (nsects > IDE_MAX_TRANSFER_48)
				nsects = IDE_MAX_TRANSFER_48;
			ide_write(dev->channel, ATA_REG_SECCOUNT1, (nsects >> 8) & 0xFF);
		} else {
			if (nsects > IDE_MAX_TRANSFER_28)
				nsects = IDE_MAX_TRANSFER_28;
		}

		ide_write(dev->channel, ATA_REG_SECCOUNT0, nsects & 0xFF);

		void *edi = (void *)kernel_map(bio->page);
		if (!edi)
			return 0;

		uintptr_t offset = 0;

		ide_write(dev->channel, ATA_REG_COMMAND, cmd);	// Send command

		if (direction == ATA_READ) { // PIO read
			uint32_t i;
			for (i = 0; i < nsects; i++) {
				int32_t error = wait_irq(dev, IDE_TIMEOUT);
				if (error < 0) {
					kernel_unmap((uintptr_t)edi);
					return error;
				}

				insw(bus, edi + bio->offset + offset, IDE_SECTOR_SIZE / 2);
				offset += IDE_SECTOR_SIZE;
			}
		} else { // PIO write
			uint32_t i;
			for (i = 0; i < nsects; i++) {
				int32_t error = wait_irq(dev, IDE_TIMEOUT);
				if (error < 0) {
					kernel_unmap((uintptr_t)edi);
					return error;
				}

				outsw(bus, edi + bio->offset + offset, IDE_SECTOR_SIZE / 2);
				offset += IDE_SECTOR_SIZE;
			}
		}

		kernel_unmap((uintptr_t)edi);

		return (int32_t)nsects;
	}

	return 0;
}

/*
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
} */