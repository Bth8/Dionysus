#include <common.h>
#include <pci.h>
#include <monitor.h>

u32int pciConfigReadWord(u16int bus, u16int slot, u16int func, u16int off) {
	u32int address;
	u16int ret;

	// Create config address
	address = (bus << 16) | (slot << 11) | (func << 8) | (off & 0xFC) | 0x80000000;

	// Write it
	outl(CONFIG_ADDRESS, address);

	// Read in data
	ret = (u16int)(inl(CONFIG_DATA) >> ((off & 2) * 8) & 0xFFFF);
	return ret;
}

void pci_test() {
	u32int bus, slot, func, i;
	for (bus = 0; bus < 256; bus++)
		for (slot = 0; slot < 32; slot++)
			for (func = 0; func < 8; func++) {
			
				int vend = pciConfigReadWord(bus, slot, func, 0);
				if (vend != 0xFFFF) {
					monitor_write("Bus ");
					monitor_write_udec(bus);
					monitor_write(" Slot ");
					monitor_write_udec(slot);
					monitor_write(" Func ");
					monitor_write_udec(func);
					monitor_write(" - Device: ");
					monitor_write_hex(pciConfigReadWord(bus, slot, func, 2));
					monitor_write(", vendor: ");
					monitor_write_hex(vend);
					monitor_write(", class: ");
					monitor_write_hex(pciConfigReadWord(bus, slot, func, 10));
					monitor_put('\n');
				}
			}
}
