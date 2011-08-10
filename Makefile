SOURCES=boot.o main.o common.o monitor.o string.o gdt.o descriptor_tables.o idt.o interrupt.o kmalloc.o paging.o \
		keyboard.o ordered_array.o kheap.o process.o task.o timer.o syscall.o time.o

CC=gcc
CFLAGS=-Wall -Wextra -Werror -nostdlib -nostartfiles -fomit-frame-pointer -nodefaultlibs -I./Include -fno-leading-underscore -m32 -O -fno-builtin
LD=ld
LDFLAGS=-Tlink.ld
AS=nasm
ASFLAGS=-felf32

all: $(SOURCES) link

clean:
	rm *.o kernel

link: $(SOURCES)
	$(LD) $(LDFLAGS) -o kernel $(SOURCES)

.s.o:
	$(AS) $(ASFLAGS) $<
