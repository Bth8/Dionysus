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

/* static uint8_t ide_polling(struct IDEChannelRegisters *channel,
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
} */

static uint8_t ide_print_err(struct IDEDevice *dev, uint8_t err) {
	if (!err)
		return err;

	printf("IDE:\n\t");
	if (err == 1) {printf("- Device fault\n\t"); err = 19;}
	else if (err == 2) {
		uint8_t st = ide_read(dev->channel, ATA_REG_ERROR);
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
		(char *[]){"Primary", "Secondary"}[dev->channel->channel],
		(char *[]){"master", "slave"}[dev->drive],
		dev->model);

	return err;
}

static blkdev_t *ide_blkdev_create(struct IDEDevice *ide) {
	blkdev_t *dev = alloc_blkdev();
	if (!dev)
		return NULL;

	dev->major = IDE_MAJOR;
	dev->minor = 16 * (2 * ide->channel->channel + ide->drive);
	dev->max_part = 16;
	dev->sector_size = IDE_SECTOR_SIZE;
	dev->size = ide->size * IDE_SECTOR_SIZE / KERNEL_BLOCKSIZE;
	dev->lock = 0;
	dev->handler = request_ide;
	dev->private_data = ide;

	return dev;
}

static int ide_probe(struct pci_dev *pci, const struct pci_dev_id *id) {
	if (claim_pci_dev(pci) < 0)
		return 0;

	blkdev_t *blkdevs[4] = {NULL, NULL, NULL, NULL};

	uint16_t command = pciConfigReadWord(pci->bus->secondary, pci->slot,
		pci->func, PCI_COMMAND);
	command |= 0x04;
	pciConfigWriteWord(pci->bus->secondary, pci->slot, pci->func, PCI_COMMAND,
		command);
	command = pciConfigReadWord(pci->bus->secondary, pci->slot, pci->func,
		PCI_COMMAND);

	if (!(command & 0x04)) {
		release_pci_dev(pci);
		return 0;
	}

	uint16_t prog = pciConfigReadWord(pci->bus->secondary, pci->slot,
		pci->func, PCI_CLASS_PROG);

	if (prog & 0x02)
		prog |= 0x01;
	if (prog & 0x08)
		prog |= 0x04;

	pciConfigWriteWord(pci->bus->secondary, pci->slot, pci->func,
		PCI_CLASS_PROG, prog);
	prog = pciConfigReadWord(pci->bus->secondary, pci->slot, pci->func,
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
		channel->nEIN = 0;
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

		// Disable interrupts
		ide_write(channel, ATA_REG_CONTROL, 2);

		for (j = 0; j < 2; j++) {
			uint8_t type = IDE_ATA;

			// Select drive
			ide_write(channel, ATA_REG_HDDEVSEL, j << 4);
			sleep_until(tick + 2 * HZ / 1000);

			// Send ATA identify command
			ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			sleep_until(tick + 2 * HZ / 1000);

			// Polling
			if (ide_read(channel, ATA_REG_STATUS) == 0)
				continue;	// Status 0 means no device

			uint8_t err = 0;
			while (1) {
				uint8_t status = ide_read(channel, ATA_REG_STATUS);
				if (status & ATA_SR_ERR) {err = 1; break;} // Device isn't ATA
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			// Probe for ATAPI
			if (err) {
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
				sleep_until(tick + 1 * HZ / 1000);
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
				i / 2, i % 2, dev->size * IDE_SECTOR_SIZE / 1024 / 1024,
				dev->model);

			blkdevs[i + j] = ide_blkdev_create(dev);
			if (blkdevs[i + j] == NULL) {
				refs--;
				kfree(dev);
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
			if (add_blkdev(blkdevs[i]) < 0) {
				if (i % 2 == 0 && !blkdevs[i + 1]) {
					destroy_mutex(ide->channel->mutex);
					kfree(ide->channel);
				}
				kfree(ide);
				kfree(blkdevs[i]);
				continue;
			}
			autopopulate_blkdev(blkdevs[i]);
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
			kfree(ide);
			kfree(blkdevs[i]);
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

static int32_t ide_ata_access(struct IDEDevice *dev, request_t *req);

static int32_t request_ide(blkdev_t *dev) {
	int32_t ret = 0;
	spin_lock(&dev->lock);
	struct IDEDevice *ide = (struct IDEDevice *)dev->private_data;
	acquire_mutex(ide->channel->mutex);

	node_t *node = dev->queue->head;
	while (node) {
		request_t *req = (request_t *)node->data;
		int32_t sectors_xferred = ide_ata_access(ide, req);
		int32_t cont;

		if (sectors_xferred < 0) {
			ret = sectors_xferred;
			cont = end_request(req, 0, 0);
		} else
			cont = end_request(req, 1, (uint32_t)sectors_xferred);

		if (cont == 0) {
			list_dequeue(dev->queue, node);
			free_request(req);
			kfree(node);
			if (ret < 0)
				break;
		}
		node = dev->queue->head;
	}

	release_mutex(ide->channel->mutex);
	spin_unlock(&dev->lock);

	return ret;
}

static int statusgood(uint8_t status) {
	if (status & ATA_SR_ERR)
		return 0;
	if (status & ATA_SR_DF)
		return 0;
	if (!(status & ATA_SR_DRQ))
		return 0;

	return 1;
}

static void waste400(struct IDEChannelRegisters *channel) {
	int i;
	for (i = 0; i < 4; i++)
		ide_read(channel, ATA_REG_ALTSTATUS);
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

	// Enable interrupts
	dev->channel->nEIN = 0;

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
	while (ide_read(dev->channel, ATA_REG_STATUS) & ATA_SR_BSY)
		continue;

	// Select drive and method
	if (dev->cap & ATA_CAP_LBA)
		ide_write(dev->channel, ATA_REG_HDDEVSEL, 0xE0 | (slave << 4) | head);
	else
		ide_write(dev->channel, ATA_REG_HDDEVSEL, 0xA0 | (slave << 4) | head);

	// Wait 400ns
	waste400();

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

	if (dma) { // TODO
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

		uint32_t nsects = bio->nbytes / IDE_SECTOR_SIZE;

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

		if (direction == ATA_READ) // PIO read
			for (i = 0; i < nsects; i++) {
				waste400(dev->channel);
				uint8_t status = 0;
				wait_event_interruptable(ide_wq, 
					!((status = ide_read(dev->channel, ATA_REG_STATUS)) & ATA_SR_BSY));
				if (status & ATA_SR_BSY) {
					kernel_unmap((uintptr_t)edi);
					return -EINTR;
				}
				if (!statusgood(status)) {
					kernel_unmap((uintptr_t)edi);
					return -EIO;
				}

				insw(bus, edi + bio->offset + offset, IDE_SECTOR_SIZE / 2);
				offset += IDE_SECTOR_SIZE;
			}
		else { // PIO write
			for (i = 0; i < bio->nbytes / KERNEL_BLOCKSIZE; i++) {
				waste400(dev->channel);
				uint8_t status = 0;
				wait_event_interruptable(ide_wq, 
					!((status = ide_read(dev->channel, ATA_REG_STATUS)) & ATA_SR_BSY));
				if (status & ATA_SR_BSY) {
					kernel_unmap((uintptr_t)edi);
					return -EINTR;
				}
				if (!statusgood(status)) {
					kernel_unmap((uintptr_t)edi);
					return -EIO;
				}

				outsw(bus, edi + offset, IDE_SECTOR_SIZE / 2);
				offset += IDE_SECTOR_SIZE;
			}
			/* if (lba_ext)
				ide_write(channel, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH_EXT);
			else
				ide_write(channel, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);

				ide_polling(channel, 0); */
		}

		kernel_unmap((uintptr_t)edi);

		return (int32_t)nsects;
	}

	return 0;
}
/*
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
} */