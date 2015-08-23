CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -fPIC $(MACROS) -I../renderdoc/api/
CPPFLAGS=-std=c++11 -g -Wno-unused -Wno-unknown-pragmas -Wno-reorder
LDFLAGS=-L../renderdoc/ -lrenderdoc -lGL -lX11 -Wl,-rpath,'$$ORIGIN/'
OBJDIR=.obj
OBJECTS=renderdoccmd.o renderdoccmd_linux.o

.PHONY: all
all: bin/renderdoccmd

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $$(dirname $@)
	$(CPP) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	@$(CPP) $(CFLAGS) $(CPPFLAGS) -MM -MT $(OBJDIR)/$*.o $*.cpp > $(OBJDIR)/$*.d

$(OBJDIR)/%.o: %.c
	@mkdir -p $$(dirname $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MT $(OBJDIR)/$*.o $*.c > $(OBJDIR)/$*.d

OBJDIR_OBJECTS=$(addprefix $(OBJDIR)/, $(OBJECTS))

-include $(OBJDIR_OBJECTS:.o=.d)

bin/renderdoccmd: $(OBJDIR_OBJECTS) ../renderdoc/librenderdoc.so
	mkdir -p bin/
	$(CPP) -o bin/renderdoccmd $(OBJDIR_OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf bin/ $(OBJDIR)
