CC = gcc
AS = nasm
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
LDFLAGS = -m32 -T src/linker.ld -ffreestanding -O2 -nostdlib -no-pie

OBJS = build/boot.o build/gdt_flush.o build/gdt.o build/idt_flush.o build/idt.o build/interrupt.o build/isr.o build/irq.o build/keyboard.o build/mouse.o build/pic.o build/paging.o build/kmalloc.o build/kernel.o
ISO = os.iso

all: $(ISO)

build/boot.o: src/boot.s
	$(AS) -f elf32 src/boot.s -o build/boot.o

build/gdt_flush.o: src/gdt_flush.s
	$(AS) -f elf32 src/gdt_flush.s -o build/gdt_flush.o

build/gdt.o: src/gdt.c
	$(CC) $(CFLAGS) -c src/gdt.c -o build/gdt.o

build/idt_flush.o: src/idt_flush.s
	$(AS) -f elf32 src/idt_flush.s -o build/idt_flush.o

build/idt.o: src/idt.c
	$(CC) $(CFLAGS) -c src/idt.c -o build/idt.o

build/interrupt.o: src/interrupt.s
	$(AS) -f elf32 src/interrupt.s -o build/interrupt.o

build/isr.o: src/isr.c
	$(CC) $(CFLAGS) -c src/isr.c -o build/isr.o

build/irq.o: src/irq.c
	$(CC) $(CFLAGS) -c src/irq.c -o build/irq.o

build/keyboard.o: src/keyboard.c
	$(CC) $(CFLAGS) -c src/keyboard.c -o build/keyboard.o

build/mouse.o: src/mouse.c
	$(CC) $(CFLAGS) -c src/mouse.c -o build/mouse.o

build/pic.o: src/pic.c
	$(CC) $(CFLAGS) -c src/pic.c -o build/pic.o

build/paging.o: src/paging.c
	$(CC) $(CFLAGS) -c src/paging.c -o build/paging.o

build/kmalloc.o: src/kmalloc.c
	$(CC) $(CFLAGS) -c src/kmalloc.c -o build/kmalloc.o

build/kernel.o: src/kernel.c
	$(CC) $(CFLAGS) -c src/kernel.c -o build/kernel.o

build/myos.bin: $(OBJS)
	$(CC) $(LDFLAGS) -o build/myos.bin $(OBJS)

$(ISO): build/myos.bin
	mkdir -p iso/boot/grub
	cp build/myos.bin iso/boot/myos.bin
	echo 'menuentry "Il Mio OS" {' > iso/boot/grub/grub.cfg
	echo '	multiboot /boot/myos.bin' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -f build/*.o build/myos.bin $(ISO)
	rm -f iso/boot/myos.bin
	rm -f iso/boot/grub/grub.cfg
