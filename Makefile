all-dep := chdevbase led

all:
	mkdir output; \
	cp chdevbase/chdevbase.ko output; \
	cp led/led.ko output

build-chdevbase:
	$(MAKE) -C chdevbase

led:



clean-all:


clean-chdevbase:
	$(MAKE) -C chdevbase clean


clean-led:
