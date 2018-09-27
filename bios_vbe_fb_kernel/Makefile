CC      = gcc
CFLAGS  = -Wall -fno-builtin -nostdinc -nostdlib
LD      = ld
CFLAGS  += -m32
LFLAGS = -m elf_i386
ASFLAGS = --32

OBJFILES = \
	loader.o  \
	common/printf.o  \
	common/screen.o  \
	common/bios.o  \
	common/vbe.o  \
	common/qdivrem.o  \
	common/udivdi3.o  \
	common/umoddi3.o  \
	common/divdi3.o  \
	common/moddi3.o  \
	common/setjmp.o  \
	common/string.o  \
	common/s_floor.o  \
	x86emu/x86emu.o  \
	x86emu/x86emu_util.o  \
	fractal.o \
	pci.o \
	kernel.o

image: kernel.bin
	@echo "Creating hdd.img..."
	@dd if=/dev/zero of=./hdd.img bs=512 count=16065 1>/dev/null 2>&1

	@echo "Creating bootable first FAT32 partition..."
	sudo losetup /dev/loop1 ./hdd.img
	@(echo c; echo u; echo n; echo p; echo 1; echo ;  echo ; echo a; echo 1; echo t; echo c; echo w;) | sudo fdisk /dev/loop1 1>/dev/null 2>&1 || true

	@echo "Mounting partition to /dev/loop2..."
	
	@sudo losetup /dev/loop2 ./hdd.img --offset `echo \`sudo fdisk -lu /dev/loop1 | tail -n 1 | awk '{print $$3}'\`*512 | bc` --sizelimit `echo \`sudo fdisk -lu /dev/loop1 | tail -n 1 | awk '{print $$4}'\`*512 | bc`

	@echo "Format partition..."
	sudo mkfs.vfat /dev/loop2

	@echo "Copy kernel and grub files on partition..."
	@mkdir -p tempdir
	sudo mount /dev/loop2 ./tempdir

	@sudo mkdir tempdir/boot
	@sudo cp -r grub tempdir/boot/
	@sudo cp kernel.bin tempdir/

#	echo "Install bootloader:"
#	sudo grub-install --target=i386-pc --force --removable --no-uefi-secure-boot --boot-directory=./tempdir/boot /dev/loop1
#	sudo cp -f ./kernel.bin tempdir/
#	sudo cp -f ./grub.cfg tempdir/boot/grub/
	@sleep 1
	sudo losetup -d /dev/loop1
	@sudo umount /dev/loop2
	@rm -r tempdir
	sudo losetup -d /dev/loop2

	@echo "Installing GRUB..."
	@echo "device (hd0) hdd.img \n \
	       root (hd0,0)         \n \
	       setup (hd0)          \n \
	       quit\n" | grub --batch 1>/dev/null
	@echo "Done!"

	@echo "Done!"

all: image

rebuild: clean all
.s.o:
	as ${ASFLAGS} -o $@ $<

.c.o:
	$(CC) -Ix86emu -Iinclude $(CFLAGS) -o $@ -c $<

kernel.bin: $(OBJFILES)
	$(LD) ${LFLAGS} -T linker.ld -o $@ $^

clean:
	rm -f $(OBJFILES) hdd.img kernel.bin
