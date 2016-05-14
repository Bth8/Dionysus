ENTRY(start)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

SECTIONS {
	. = 0xC0100000;

	kbegin = .;

	.text : AT(ADDR(.text) - 0xC0000000) {
		*(.text)
	}

	.data ALIGN(0x1000) : AT(ADDR(.data) - 0xC0000000){
		*(.data)
		*(.rodata)
	}

	.bss ALIGN(0x1000) : AT(ADDR(.bss) - 0xC0000000){
		*(.bss)
	}

	kend = .;
}
