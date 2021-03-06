KERNEL_DIR=kernel
IMAGE_DIR=hdd
TOOLCHAIN_DIR=toolchain

.PHONY: all clean kernel clean-kernel clean-image toolchain

all: kernel dionysus.img

clean: clean-kernel clean-image

kernel: $(KERNEL_DIR)/dionysus

toolchain:
	cd $(TOOLCHAIN_DIR); ./build.sh

clean-kernel:
	$(MAKE) -C $(KERNEL_DIR) clean

dionysus.img: $(IMAGE_DIR)/boot/dionysus
	./build_disk.sh

clean-image:
	rm -f $(IMAGE_DIR)/boot/dionysus dionysus.img

$(KERNEL_DIR)/dionysus:
	$(MAKE) -C $(KERNEL_DIR)

$(IMAGE_DIR)/boot/dionysus: $(KERNEL_DIR)/dionysus
	cp $(KERNEL_DIR)/dionysus $(IMAGE_DIR)/boot