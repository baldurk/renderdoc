.PHONY: all
all:
	mkdir -p bin/
	cd renderdoc && $(MAKE) librenderdoc.so
	cd renderdoccmd && $(MAKE) bin/renderdoccmd
	cd qrenderdoc && qmake "CONFIG+=debug" && $(MAKE)
	cp renderdoc/librenderdoc.so renderdoccmd/bin/renderdoccmd bin/

.PHONY: clean
clean:
	cd renderdoc && $(MAKE) clean
	cd renderdoccmd && $(MAKE) clean
	cd qrenderdoc && rm -rf .obj Makefile*
