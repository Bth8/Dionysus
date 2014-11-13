SOURCES_MAIN=boot.o main.o port.o common.o monitor.o printf.o string.o \
	descriptor_tables.o gdt.o idt.o interrupt.o paging.o kmalloc.o timer.o \
	time.o process.o task.o syscall.o vfs.o block.o char.o fileops.o elf.o pci.o

SOURCES_FS=dev.o

SOURCES_CHARDEV=term.o

SOURCES_PCI=

SOURCES_STRUCTURES=tree.o list.o hashmap.o

SOURCES_ALL=$(SOURCES_MAIN) $(addprefix fs/, $(SOURCES_FS)) \
			$(addprefix chardev/, $(SOURCES_CHARDEV)) \
			$(addprefix pci/, $(SOURCES_PCI)) \
			$(addprefix structures/, $(SOURCES_STRUCTURES))

CC=i686-pc-dionysus-gcc
CFLAGS=-Wall -Wextra -Werror -Wno-unused-parameter \
	   -Wno-missing-field-initializers -nostdlib \
	   -fomit-frame-pointer -I./Include -fno-leading-underscore -O \
	   -ffreestanding
LDFLAGS=-Tlink.ld
LIBS=-lgcc
AS=nasm
ASFLAGS=-felf32

all: kernel

debug: CFLAGS += -g
debug: all

clean:
	rm -f $(SOURCES_ALL) kernel

kernel: $(SOURCES_ALL)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(SOURCES_ALL) $(LIBS)

%.o: %.s
	$(AS) $(ASFLAGS) $<
