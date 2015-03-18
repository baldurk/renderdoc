.PHONY: all
all:
	mkdir -p bin/
	cd renderdoc && make librenderdoc.so
	cd renderdoccmd && make bin/renderdoccmd
	cd qrenderdoc && qmake "CONFIG+=debug" && make
	cp renderdoc/librenderdoc.so renderdoccmd/bin/renderdoccmd bin/

.PHONY: clean
clean:
	cd renderdoc && make clean
	cd renderdoccmd && make clean
	cd qrenderdoc && rm -rf .obj Makefile*
