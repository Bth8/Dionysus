SOURCES_MAIN=boot.o main.o common.o monitor.o string.o gdt.o \
		descriptor_tables.o idt.o interrupt.o kmalloc.o paging.o \
		ordered_array.o kheap.o process.o task.o timer.o syscall.o time.o \
		port.o ide.o vfs.o pci.o dev.o printf.o elf.o

SOURCES_FS=rootfs.o fat32.o

SOURCES_CHARDEV=term.o

SOURCES_ALL=$(SOURCES_MAIN) $(addprefix fs/, $(SOURCES_FS)) $(addprefix chardev/, $(SOURCES_CHARDEV))

CC=i686-pc-dionysus-gcc
CFLAGS=-Wall -Wextra -Werror -nostdlib -nostartfiles -fomit-frame-pointer \
		-nodefaultlibs -I./Include -fno-leading-underscore -O -fno-builtin
LD=i686-pc-dionysus-ld
LDFLAGS=-Tlink.ld
LIBS=$(shell $(CC) -print-libgcc-file-name)
AS=nasm
ASFLAGS=-felf32

all: $(SOURCES_ALL) link

clean:
	rm *.o fs/*.o chardev/*.o kernel

link: $(SOURCES_ALL)
	$(LD) $(LDFLAGS) -o kernel $(SOURCES_ALL) $(LIBS)

.s.o:
	$(AS) $(ASFLAGS) $<
