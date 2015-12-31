CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\"" \
                         -DVK_PROTOTYPES
CFLAGS=-c -Wall -Werror -Wno-unused -Wno-unknown-pragmas -fPIC $(MACROS) -I../../ -I../../3rdparty/ -I$(VULKAN_SDK_SRC)/include -I$(VULKAN_SDK_SRC)/layers -I$(VULKAN_SDK_SRC)/loader -I$(VULKAN_SDK_SRC)/build/layers
CPPFLAGS=-std=c++11 -g -Wno-reorder -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS=-lpthread -lrt -shared -ldl -lX11
OBJDIR=.obj
OBJECTS=rdtrace.o \
vk_core.o \
vk_common.o \
vk_manager.o \
vk_resources.o \
vk_debug.o \
LoaderAndTools/layers/vk_layer_table.o \
vk_replay.o \
vk_info.o \
vk_replay_linux.o \

DOBJDIR=../../.obj/data/spv
DATA=blitvs.spvo \
checkerboardfs.spvo \
texdisplayfs.spvo \
textvs.spvo \
textfs.spvo \
../sourcecodepro.ttfo

OBJDIR_DATA=$(addprefix $(DOBJDIR)/, $(DATA))

.PHONY: all
all: libVKLayerRenderdoc.so rdoc_vulkan.a

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

STATIC_OBJECTS=$(addprefix $(OBJDIR)/, vk_core.o \
vk_common.o \
vk_manager.o \
vk_resources.o \
LoaderAndTools/layers/vk_layer_table.o \
vk_replay.o \
vk_debug.o \
vk_info.o \
vk_replay_linux.o \
)

EXTRA_OBJECTS=../../.obj/3rdparty/stb/stb_impl.o ../../.obj/3rdparty/jpeg-compressor/jpge.o

rdoc_vulkan.a: $(STATIC_OBJECTS)
	ar rcs rdoc_vulkan.a $(STATIC_OBJECTS)

libVKLayerRenderdoc.so: $(OBJDIR_OBJECTS) $(EXTRA_OBJECTS) $(OBJDIR_DATA) $(VULKAN_SDK_SRC)/build/layers/liblayer_utils.so
	/usr/bin/c++  -fPIC -std=c++11 -DVK_PROTOTYPES -Wpointer-arith -g -Wl,-Bsymbolic -shared -Wl,-soname,libVKLayerRenderdoc.so -o libVKLayerRenderdoc.so $(OBJDIR_OBJECTS) $(EXTRA_OBJECTS) $(OBJDIR_DATA) ../../.obj/os/linux/linux_threading.o ../../.obj/os/linux/linux_stringio.o ../../.obj/os/linux/linux_network.o ../../.obj/os/os_specific.o ../../.obj/serialise/utf8printf.o ../../.obj/serialise/grisu2.o ../../.obj/core/core.o ../../.obj/core/remote_access.o ../../.obj/replay/capture_options.o ../../.obj/replay/type_helpers.o ../../.obj/serialise/serialiser.o ../../.obj/core/resource_manager.o ../../.obj/common/common.o $(VULKAN_SDK_SRC)/build/layers/liblayer_utils.so -Wl,-rpath,$(VULKAN_SDK_SRC)/build/layers -lpthread -lrt -ldl -lX11

.PHONY: clean
clean:
	rm -rf rdoc_vulkan.a libVKLayerRenderdoc.so $(OBJDIR)
