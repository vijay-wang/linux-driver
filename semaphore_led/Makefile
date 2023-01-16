KERNELDIR := ../../linux-alientek
CURRENT_PATH := $(shell pwd)
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m := semaphore_led.o
app := semaphore_ledApp


build: kernel_modules
	$(CROSS_COMPILE)gcc $(app).c -o $(app)

kernel_modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)  -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	rm $(app) -f
