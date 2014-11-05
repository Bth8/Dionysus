SOURCES_MAIN=boot.o main.o common.o monitor.o string.o gdt.o idt.o \
	descriptor_tables.o interrupt.o paging.o process.o task.o fileops.o \
	timer.o syscall.o time.o port.o vfs.o elf.o pci.o dev.o kmalloc.o printf.o

SOURCES_FS=rootfs.o fat32.o

SOURCES_CHARDEV=term.o

SOURCES_PCI=ide.o

SOURCES_STRUCTURES=tree.o list.o ordered_array.o

SOURCES_ALL=$(SOURCES_MAIN) $(addprefix fs/, $(SOURCES_FS)) \
			$(addprefix chardev/, $(SOURCES_CHARDEV)) \
			$(addprefix pci/, $(SOURCES_PCI)) \
			$(addprefix structures/, $(SOURCES_STRUCTURES))

CC=i686-pc-dionysus-gcc
CFLAGS=-Wall -Wextra -Werror -Wno-unused-parameter \
	   -Wno-missing-field-initializers -nostdlib \
	   -fomit-frame-pointer -I./Include -fno-leading-underscore -O \
	   -ffreestanding
LD=i686-pc-dionysus-ld
LDFLAGS=-Tlink.ld
LIBS=$(shell $(CC) -print-libgcc-file-name)
AS=nasm
ASFLAGS=-felf32

all: kernel

clean:
	rm *.o fs/*.o chardev/*.o kernel

kernel: $(SOURCES_ALL)
	$(LD) $(LDFLAGS) -o $@ $(SOURCES_ALL) $(LIBS)

%.o: %.s
	$(AS) $(ASFLAGS) $<
