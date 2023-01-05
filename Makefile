#make single obj   : make TARGET=subdir-name
#make all subobj   : make all
#clean all 	   : make all
#clean single obj  : make clean TARGET=subdir-name


TARGET ?=

sub-dirs := $(filter-out . output, $(foreach dir, $(shell find . -maxdepth 1 -type d), $(notdir $(dir))))

all-dep := chdevbase led

.PHONY: $(TARGET)

$(TARGET):
	$(MAKE) -C $(TARGET)

all:
	mkdir output;\
	for target in $(sub-dirs); do \
		$(MAKE) -C $${target}; \
		cp $${target}/$${target}.ko $${target}/$${target}App output; \
       	done

clean-all:
	rm output -rf; \
	for target in $(sub-dirs); do \
		$(MAKE) -C $${target} clean; \
       	done

clean: clean-$(TARGET)

clean-$(TARGET):
	$(MAKE) -C $(TARGET) clean

