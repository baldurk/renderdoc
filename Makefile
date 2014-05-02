all:
	cd renderdoc && make librenderdoc.so
	cd renderdoccmd && make bin/renderdoccmd

clean:
	cd renderdoc && make clean
