CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -Wno-unused -Wno-unknown-pragmas -fPIC $(MACROS) -I. -I3rdparty/
CPPFLAGS=-std=c++11 -g -Wno-reorder -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS=-lpthread -lrt -shared -ldl -lX11
LIBS=driver/gl/rdoc_gl.a driver/shaders/spirv/rdoc_spirv.a
OBJDIR=.obj
OBJECTS=replay/replay_output.o \
replay/replay_renderer.o \
replay/entry_points.o \
replay/type_helpers.o \
replay/app_api.o \
replay/capture_options.o \
hooks/hooks.o \
serialise/serialiser.o \
serialise/grisu2.o \
serialise/utf8printf.o \
serialise/string_utils.o \
common/common.o \
common/dds_readwrite.o \
core/remote_access.o \
core/replay_proxy.o \
core/remote_replay.o \
core/resource_manager.o \
core/core.o \
maths/camera.o \
maths/matrix.o \
os/os_specific.o \
3rdparty/jpeg-compressor/jpgd.o \
3rdparty/jpeg-compressor/jpge.o \
3rdparty/lz4/lz4.o \
3rdparty/stb/stb_impl.o \
3rdparty/tinyexr/tinyexr.o \
os/linux/linux_callstack.o \
os/linux/linux_hook.o \
os/linux/linux_network.o \
os/linux/linux_process.o \
os/linux/linux_stringio.o \
os/linux/linux_threading.o \
os/linux/linux_libentry.o
# linux_libentry must be the last object file

DATA=data/glsl/debuguniforms.ho \
data/glsl/texsample.ho \
data/glsl/blit.frago \
data/glsl/blit.verto \
data/glsl/texdisplay.frago \
data/glsl/checkerboard.frago \
data/glsl/generic.frago \
data/glsl/generic.verto \
data/glsl/mesh.verto \
data/glsl/mesh.frago \
data/glsl/mesh.geomo \
data/glsl/mesh.compo \
data/glsl/text.verto \
data/glsl/text.frago \
data/glsl/quadoverdraw.frago \
data/glsl/histogram.compo \
data/glsl/arraymscopy.compo \
data/glsl/outline.frago \
data/sourcecodepro.ttfo

.PHONY: all
all: librenderdoc.so

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $$(dirname $@)
	$(CPP) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	@$(CPP) $(CFLAGS) $(CPPFLAGS) -MM -MT $(OBJDIR)/$*.o $< > $(OBJDIR)/$*.d

$(OBJDIR)/%.o: %.c
	@mkdir -p $$(dirname $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MT $(OBJDIR)/$*.o $< > $(OBJDIR)/$*.d

# objcopy needs to be run in with paths in current directory
# to produce the right symbol names
OBJGEN = \
mkdir -p $$(dirname $@); \
cd $$(dirname $<); \
objcopy --input binary --output elf64-x86-64 --binary-architecture i386 $$(basename $<) $$(basename $@); \
cd - > /dev/null 2>&1; \
mv $$(dirname $<)/$$(basename $@) $@;

$(OBJDIR)/%.verto: %.vert
	@echo Object building $@
	@$(OBJGEN)
$(OBJDIR)/%.frago: %.frag
	@echo Object building $@
	@$(OBJGEN)
$(OBJDIR)/%.geomo: %.geom
	@echo Object building $@
	@$(OBJGEN)
$(OBJDIR)/%.compo: %.comp
	@echo Object building $@
	@$(OBJGEN)
$(OBJDIR)/%.ttfo: %.ttf
	@echo Object building $@
	@$(OBJGEN)
$(OBJDIR)/%.ho: %.h
	@echo Object building $@
	@$(OBJGEN)

OBJDIR_OBJECTS=$(addprefix $(OBJDIR)/, $(OBJECTS))
OBJDIR_DATA=$(addprefix $(OBJDIR)/, $(DATA))

-include $(OBJDIR_OBJECTS:.o=.d)

driver/gl/rdoc_gl.a: force_look
	cd driver/gl && $(MAKE) rdoc_gl.a

driver/shaders/spirv/rdoc_spirv.a: force_look
	cd driver/shaders/spirv && $(MAKE) rdoc_spirv.a

force_look:
	true

librenderdoc.so: $(OBJDIR_OBJECTS) $(OBJDIR_DATA) $(LIBS)
	$(CPP) -o librenderdoc.so $(OBJDIR_DATA) -Wl,--whole-archive $(LIBS) -Wl,--no-whole-archive $(OBJDIR_OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf librenderdoc.so $(OBJDIR)
	cd driver/gl && $(MAKE) clean
	cd driver/shaders/spirv && $(MAKE) clean
