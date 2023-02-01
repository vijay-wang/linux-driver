KERNELDIR := ../../linux-alientek
CURRENT_PATH := $(shell pwd)
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m := key0irq_blockio.o
app := key0irq_blockioApp


build: kernel_modules
	$(CROSS_COMPILE)gcc $(app).c -o $(app)

kernel_modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)  -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	rm $(app) -f
