CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -fPIC $(MACROS) -I../renderdoc/
CPPFLAGS=-std=c++11 -g -Wno-unused -Wno-unknown-pragmas -Wno-reorder
LDFLAGS=-L../renderdoc -lrenderdoc -lGL
OBJECTS=renderdoccmd.o renderdoccmd_linux.o

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
