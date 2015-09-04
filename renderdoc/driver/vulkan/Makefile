CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -Wno-unused -Wno-unknown-pragmas -fPIC $(MACROS) -I../../ -I../../3rdparty/ -I$(VULKAN_SDK)/include/vulkan
CPPFLAGS=-std=c++11 -g -Wno-reorder -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS=-lpthread -lrt -shared -ldl -lX11
OBJDIR=.obj
OBJECTS=vk_common.o \
vk_core.o \
vk_hooks_linux.o \
vk_manager.o \
vk_replay.o \
vk_info.o \
vk_replay_linux.o \
vk_resources.o \

.PHONY: all
all: rdoc_vulkan.a

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $$(dirname $@)
	$(CPP) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	@$(CPP) $(CFLAGS) $(CPPFLAGS) -MM -MT $(OBJDIR)/$*.o $< > $(OBJDIR)/$*.d

$(OBJDIR)/%.o: %.c
	@mkdir -p $$(dirname $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MT $(OBJDIR)/$*.o $< > $(OBJDIR)/$*.d

OBJDIR_OBJECTS=$(addprefix $(OBJDIR)/, $(OBJECTS))

-include $(OBJDIR_OBJECTS:.o=.d)

rdoc_vulkan.a: $(OBJDIR_OBJECTS)
	ar rcs rdoc_vulkan.a $(OBJDIR_OBJECTS)

.PHONY: clean
clean:
	rm -rf rdoc_vulkan.a $(OBJDIR)
