CC=gcc
CPP=g++
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGIT_COMMIT_HASH='"'$$(git rev-parse HEAD)'"' \
			 -DRENDERDOC_VERSION_STRING='"0.20"'
CFLAGS=-c -Wall -Werror -fPIC $(MACROS) -I.
CPPFLAGS=-std=c++11 -g -Wno-unused -Wno-unknown-pragmas -Wno-reorder
LDFLAGS=-L../renderdoc -lrenderdoc
OBJECTS=linux_specific.o

all: bin/renderdoccmd

%.o: %.cpp
	$(CPP) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

../renderdoc/librenderdoc.so:

bin/renderdoccmd: $(OBJECTS) $(SOURCES) ../renderdoc/librenderdoc.so
	mkdir -p bin/
	g++ -o bin/renderdoccmd $(OBJECTS) $(LDFLAGS)

clean:
	find -type f -iname \*.o -exec rm '{}' \;
	rm -f bin/renderdoccmd
