#include <common.h>
#include <pci.h>
#include <pci_regs.h>
#include <monitor.h>

u32int pciConfigReadDword(u8int bus, u8int slot, u8int func, u8int off) {
	// Select config address
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	// Read in data
	return inl(CONFIG_DATA);
}

u16int pciConfigReadWord(u8int bus, u8int slot, u8int func, u8int off) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	return inw(CONFIG_DATA + (off & 2));
}

u8int pciConfigReadByte(u8int bus, u8int slot, u8int func, u8int off) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	return inb(CONFIG_DATA + (off & 3));
}

void pciConfigWriteDword(u8int bus, u8int slot, u8int func, u8int off, u32int val) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outl(CONFIG_DATA, val);
}

void pciConfigWriteWord(u8int bus, u8int slot, u8int func, u8int off, u16int val) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outw(CONFIG_DATA + (off & 2), val);
}

void pciConfigWriteByte(u8int bus, u8int slot, u8int func, u8int off, u8int val) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outb(CONFIG_DATA + (off & 3), val);
}

void dump_pci() {
	u32int bus, slot, func;
	for (bus = 0; bus < 256; bus++)
		for (slot = 0; slot < 32; slot++)
			for (func = 0; func < 8; func++) {
				int vend = pciConfigReadWord(bus, slot, func, PCI_VENDOR_ID);
				if (vend != 0xFFFF) {
					monitor_write("Bus ");
					monitor_write_udec(bus);
					monitor_write(" Slot ");
					monitor_write_udec(slot);
					monitor_write(" Func ");
					monitor_write_udec(func);
					monitor_write(" - Device: ");
					monitor_write_hex(pciConfigReadWord(bus, slot, func, PCI_DEVICE_ID));
					monitor_write(" vendor: ");
					monitor_write_hex(vend);
					monitor_write(" class: ");
					monitor_write_hex(pciConfigReadWord(bus, slot, func, PCI_CLASS_DEVICE));
					monitor_put('\n');
				}
			}
}
