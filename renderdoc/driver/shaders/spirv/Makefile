CC=gcc
CPP=g++
COMMIT=`git rev-parse HEAD`
MACROS=-DLINUX \
			 -DRENDERDOC_PLATFORM=linux \
			 -DRENDERDOC_EXPORTS \
			 -DGLSLANG_OSINCLUDE_UNIX \
			 -DGIT_COMMIT_HASH="\"$(COMMIT)\""
CFLAGS=-c -Wall -Werror -Wno-unused -Wno-unknown-pragmas -fPIC $(MACROS) -I../../../ -I../../../3rdparty/
CPPFLAGS=-std=c++11 -g -Wno-reorder -fvisibility=hidden -fvisibility-inlines-hidden
LDFLAGS=-lpthread -lrt -shared -ldl -lX11
OBJDIR=.obj

VPATH=../../../3rdparty

# VPATH above takes care of finding the glslang files within the 3rdparty directory
OBJECTS=spirv_common.o \
spirv_compile.o \
spirv_disassemble.o \
glslang/OGLCompilersDLL/InitializeDll.o \
glslang/SPIRV/GlslangToSpv.o \
glslang/SPIRV/SpvBuilder.o \
glslang/glslang/GenericCodeGen/CodeGen.o \
glslang/glslang/GenericCodeGen/Link.o \
glslang/glslang/MachineIndependent/Constant.o \
glslang/glslang/MachineIndependent/glslang_tab.o \
glslang/glslang/MachineIndependent/InfoSink.o \
glslang/glslang/MachineIndependent/Initialize.o \
glslang/glslang/MachineIndependent/Intermediate.o \
glslang/glslang/MachineIndependent/intermOut.o \
glslang/glslang/MachineIndependent/IntermTraverse.o \
glslang/glslang/MachineIndependent/limits.o \
glslang/glslang/MachineIndependent/linkValidate.o \
glslang/glslang/MachineIndependent/parseConst.o \
glslang/glslang/MachineIndependent/ParseHelper.o \
glslang/glslang/MachineIndependent/PoolAlloc.o \
glslang/glslang/MachineIndependent/reflection.o \
glslang/glslang/MachineIndependent/RemoveTree.o \
glslang/glslang/MachineIndependent/Scan.o \
glslang/glslang/MachineIndependent/ShaderLang.o \
glslang/glslang/MachineIndependent/SymbolTable.o \
glslang/glslang/MachineIndependent/Versions.o \
glslang/glslang/MachineIndependent/preprocessor/Pp.o \
glslang/glslang/MachineIndependent/preprocessor/PpAtom.o \
glslang/glslang/MachineIndependent/preprocessor/PpContext.o \
glslang/glslang/MachineIndependent/preprocessor/PpMemory.o \
glslang/glslang/MachineIndependent/preprocessor/PpScanner.o \
glslang/glslang/MachineIndependent/preprocessor/PpSymbols.o \
glslang/glslang/MachineIndependent/preprocessor/PpTokens.o \
glslang/glslang/OSDependent/Unix/ossource.o


.PHONY: all
all: rdoc_spirv.a

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

rdoc_spirv.a: $(OBJDIR_OBJECTS)
	ar rcs rdoc_spirv.a $(OBJDIR_OBJECTS)

.PHONY: clean
clean:
	rm -rf rdoc_spirv.a $(OBJDIR)
