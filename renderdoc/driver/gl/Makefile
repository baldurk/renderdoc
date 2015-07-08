CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -Wno-unused -Wno-unknown-pragmas -fPIC $(MACROS) -I../../ -I../../3rdparty/
CPPFLAGS=-std=c++11 -g -Wno-reorder -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS=-lpthread -lrt -shared -ldl -lX11
OBJDIR=.obj
OBJECTS=gl_common.o \
gl_driver.o \
gl_manager.o \
gl_debug.o \
gl_counters.o \
gl_replay.o \
gl_replay_linux.o \
gl_hooks_linux.o \
gl_shader_refl.o \
gl_resources.o \
gl_renderstate.o \
wrappers/gl_buffer_funcs.o \
wrappers/gl_debug_funcs.o \
wrappers/gl_draw_funcs.o \
wrappers/gl_emulated.o \
wrappers/gl_framebuffer_funcs.o \
wrappers/gl_get_funcs.o \
wrappers/gl_query_funcs.o \
wrappers/gl_sampler_funcs.o \
wrappers/gl_shader_funcs.o \
wrappers/gl_state_funcs.o \
wrappers/gl_texture_funcs.o \
wrappers/gl_uniform_funcs.o

.PHONY: all
all: rdoc_gl.a

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

rdoc_gl.a: $(OBJDIR_OBJECTS)
	ar rcs rdoc_gl.a $(OBJDIR_OBJECTS)

.PHONY: clean
clean:
	rm -rf rdoc_gl.a $(OBJDIR)
