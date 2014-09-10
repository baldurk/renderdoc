CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -Wno-unknown-pragmas -Wno-error=format -fPIC $(MACROS) -I.
CPPFLAGS=-std=c++11 -g -Wno-unused -Wno-unknown-pragmas -Wno-reorder -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS=-lpthread -lrt -shared -ldl -lX11
OBJDIR=.obj
OBJECTS=replay/replay_output.o \
replay/replay_renderer.o \
replay/entry_points.o \
replay/basic_types.o \
hooks/hooks.o \
hooks/gl_linux_hooks.o \
serialise/serialiser.o \
common/common.o \
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
driver/gl/gl_common.o \
driver/gl/gl_driver.o \
driver/gl/gl_manager.o \
driver/gl/gl_debug.o \
driver/gl/gl_replay.o \
driver/gl/gl_replay_linux.o \
driver/gl/gl_shader_refl.o \
driver/gl/gl_resources.o \
driver/gl/gl_renderstate.o \
driver/gl/wrappers/gl_buffer_funcs.o \
driver/gl/wrappers/gl_debug_funcs.o \
driver/gl/wrappers/gl_draw_funcs.o \
driver/gl/wrappers/gl_framebuffer_funcs.o \
driver/gl/wrappers/gl_get_funcs.o \
driver/gl/wrappers/gl_query_funcs.o \
driver/gl/wrappers/gl_sampler_funcs.o \
driver/gl/wrappers/gl_shader_funcs.o \
driver/gl/wrappers/gl_state_funcs.o \
driver/gl/wrappers/gl_texture_funcs.o \
driver/gl/wrappers/gl_uniform_funcs.o \
os/linux/linux_callstack.o \
os/linux/linux_network.o \
os/linux/linux_process.o \
os/linux/linux_stringio.o \
os/linux/linux_threading.o \
hooks/linux_libentry.o
SHADERS=data/glsl/blit.frago \
data/glsl/blit.verto \
data/glsl/texdisplay.frago \
data/glsl/checkerboard.frago \
data/glsl/generic.frago \
data/glsl/generic.verto \
data/glsl/mesh.verto

.PHONY: all
all: librenderdoc.so

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $$(dirname $@)
	$(CPP) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	@$(CPP) $(CFLAGS) $(CPPFLAGS) -MM -MT $(OBJDIR)/$*.o $*.cpp > $(OBJDIR)/$*.d

$(OBJDIR)/%.o: %.c
	@mkdir -p $$(dirname $@)
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MT $(OBJDIR)/$*.o $*.c > $(OBJDIR)/$*.d

# objcopy needs to be run in with paths in current directory
# to produce the right symbol names
$(OBJDIR)/%.verto: %.vert
	@mkdir -p $$(dirname $@)
	cd $$(dirname $<) && objcopy --input binary --output elf64-x86-64 --binary-architecture i386 $$(basename $<) $$(basename $@)
	@mv $$(dirname $<)/$$(basename $@) $@

$(OBJDIR)/%.frago: %.frag
	@mkdir -p $$(dirname $@)
	cd $$(dirname $<) && objcopy --input binary --output elf64-x86-64 --binary-architecture i386 $$(basename $<) $$(basename $@)
	@mv $$(dirname $<)/$$(basename $@) $@

librenderdoc.so: $(addprefix $(OBJDIR)/, $(SHADERS) $(OBJECTS))
	g++ -o librenderdoc.so $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf librenderdoc.so $(OBJDIR)
