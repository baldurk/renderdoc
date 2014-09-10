.PHONY: all
all:
	cd renderdoc && make librenderdoc.so
	cd renderdoccmd && make bin/renderdoccmd

.PHONY: clean
clean:
	cd renderdoc && make clean
	cd renderdoccmd && make clean
