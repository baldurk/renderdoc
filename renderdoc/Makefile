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
data/glsl/blit.frago \
data/glsl/blit.verto \
data/glsl/texdisplay.frago \
data/glsl/checkerboard.frago \
data/glsl/generic.frago \
data/glsl/generic.verto \
data/glsl/mesh.verto \
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

all: librenderdoc.so

%.o: %.cpp
	$(CPP) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.verto: %.vert
	cd $$(dirname $@) && objcopy --input binary --output elf64-x86-64 --binary-architecture i386 $$(basename $<) $$(basename $@)

%.frago: %.frag
	cd $$(dirname $@) && objcopy --input binary --output elf64-x86-64 --binary-architecture i386 $$(basename $<) $$(basename $@)

librenderdoc.so: $(OBJECTS) $(SOURCES)
	g++ -o librenderdoc.so $(OBJECTS) $(LDFLAGS)

clean:
	find -type f -iname \*.o -exec rm '{}' \;
	rm -f librenderdoc.so
