/* pci.h - PCI ops header */
/* Copyright (C) 2011, 2012 Bth8 <bth8fwd@gmail.com>
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

#ifndef PCI_H
#define PCI_H
#include <common.h>

struct pci_dev;

struct pci_bus {
	struct pci_bus *parent;		// Parent bus
	struct pci_bus *children;	// Chain of busses under this one
	struct pci_bus *sibling;	// Next bus on parent bus
	struct pci_bus *next;		// Chain of all busses

	struct pci_dev *self;		// This bus as seen by its parent
	struct pci_dev *devs;		// Chain of devices on this bus

	u8int primary;				// Number of the parent bus
	u8int secondary;			// Number of this bus
	u8int subordinate;			// Config stuff
};

struct pci_dev {
	struct pci_bus *bus;		// Bus we're on
	struct pci_dev *sibling;	// Next device on this bus
	struct pci_dev *next;		// Chain of all devices

	u8int slot;					// Slot number
	u8int func;					// Function number (for multifunction devices)
	u16int vendor;				// Vendor number
	u16int device;				// Device ID
	u32int class;				// Class, subclass, Prog IF
};

#define CONFIG_ADDRESS	0x0CF8
#define CONFIG_DATA		0x0CFC

void init_pci(void);
void dump_pci(void);

#endif
