.PHONY: all
all:
	cd renderdoc && make librenderdoc.so
	cd renderdoccmd && make bin/renderdoccmd
	mkdir -p bin/
	cp renderdoc/librenderdoc.so renderdoccmd/bin/renderdoccmd bin/

.PHONY: clean
clean:
	cd renderdoc && make clean
	cd renderdoccmd && make clean
