CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DVK_USE_PLATFORM_XCB_KHR \
			 -DVK_USE_PLATFORM_XLIB_KHR \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -Wno-unused -Wno-unknown-pragmas -fPIC $(MACROS) -I../../ -I../../3rdparty/ -I$(VK_SDK_PATH)/include
CPPFLAGS=-std=c++11 -g -Wno-reorder -fvisibility=hidden -fvisibility-inlines-hidden
OBJDIR=.obj
OBJECTS=vk_dispatchtables.o \
vk_tracelayer.o \
vk_core.o \
vk_initstate.o \
vk_memory.o \
vk_common.o \
vk_manager.o \
vk_resources.o \
vk_debug.o \
vk_replay.o \
vk_counters.o \
vk_info.o \
vk_state.o \
vk_linux.o \
wrappers/vk_cmd_funcs.o \
wrappers/vk_dynamic_funcs.o \
wrappers/vk_descriptor_funcs.o \
wrappers/vk_device_funcs.o \
wrappers/vk_draw_funcs.o \
wrappers/vk_get_funcs.o \
wrappers/vk_misc_funcs.o \
wrappers/vk_queue_funcs.o \
wrappers/vk_sync_funcs.o \
wrappers/vk_resource_funcs.o \
wrappers/vk_shader_funcs.o \
wrappers/vk_wsi_funcs.o

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

renderdoc_capture.json: renderdoc.json
	sed '{s%...renderdoc.dll%./librenderdoc.so%}' < renderdoc.json > renderdoc_capture.json

rdoc_vulkan.a: $(OBJDIR_OBJECTS) renderdoc_capture.json
	ar rcs rdoc_vulkan.a $(OBJDIR_OBJECTS)

.PHONY: clean
clean:
	rm -rf rdoc_vulkan.a $(OBJDIR)
