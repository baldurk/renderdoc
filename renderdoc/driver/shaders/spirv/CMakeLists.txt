set(glslang_dir ${RDOC_SOURCE_DIR}/3rdparty/glslang)
set(glslang_sources
    ${glslang_dir}/OGLCompilersDLL/InitializeDll.cpp
    ${glslang_dir}/OGLCompilersDLL/InitializeDll.h
    ${glslang_dir}/SPIRV/GlslangToSpv.cpp
    ${glslang_dir}/SPIRV/GlslangToSpv.h
    ${glslang_dir}/SPIRV/GLSL.std.450.h
    ${glslang_dir}/SPIRV/GLSL.ext.AMD.h
    ${glslang_dir}/SPIRV/GLSL.ext.EXT.h
    ${glslang_dir}/SPIRV/GLSL.ext.KHR.h
    ${glslang_dir}/SPIRV/GLSL.ext.NV.h
    ${glslang_dir}/SPIRV/hex_float.h
    ${glslang_dir}/SPIRV/InReadableOrder.cpp
    ${glslang_dir}/SPIRV/Logger.cpp
    ${glslang_dir}/SPIRV/Logger.h
    ${glslang_dir}/SPIRV/SpvBuilder.cpp
    ${glslang_dir}/SPIRV/SpvBuilder.h
    ${glslang_dir}/SPIRV/SpvTools.cpp
    ${glslang_dir}/SPIRV/SpvTools.h
    ${glslang_dir}/SPIRV/SpvPostProcess.cpp
    ${glslang_dir}/SPIRV/spvIR.h
    ${glslang_dir}/glslang/GenericCodeGen/CodeGen.cpp
    ${glslang_dir}/glslang/GenericCodeGen/Link.cpp
    ${glslang_dir}/glslang/Include/arrays.h
    ${glslang_dir}/glslang/Include/BaseTypes.h
    ${glslang_dir}/glslang/Include/Common.h
    ${glslang_dir}/glslang/Include/ConstantUnion.h
    ${glslang_dir}/glslang/Include/InfoSink.h
    ${glslang_dir}/glslang/Include/InitializeGlobals.h
    ${glslang_dir}/glslang/Include/intermediate.h
    ${glslang_dir}/glslang/Include/PoolAlloc.h
    ${glslang_dir}/glslang/Include/ResourceLimits.h
    ${glslang_dir}/glslang/Include/revision.h
    ${glslang_dir}/glslang/Include/ShHandle.h
    ${glslang_dir}/glslang/Include/Types.h
    ${glslang_dir}/glslang/MachineIndependent/Constant.cpp
    ${glslang_dir}/glslang/MachineIndependent/glslang_tab.cpp
    ${glslang_dir}/glslang/MachineIndependent/glslang_tab.cpp.h
    ${glslang_dir}/glslang/MachineIndependent/gl_types.h
    ${glslang_dir}/glslang/MachineIndependent/iomapper.cpp
    ${glslang_dir}/glslang/MachineIndependent/iomapper.h
    ${glslang_dir}/glslang/MachineIndependent/gl_types.h
    ${glslang_dir}/glslang/MachineIndependent/InfoSink.cpp
    ${glslang_dir}/glslang/MachineIndependent/Initialize.cpp
    ${glslang_dir}/glslang/MachineIndependent/Initialize.h
    ${glslang_dir}/glslang/MachineIndependent/Intermediate.cpp
    ${glslang_dir}/glslang/MachineIndependent/intermOut.cpp
    ${glslang_dir}/glslang/MachineIndependent/IntermTraverse.cpp
    ${glslang_dir}/glslang/MachineIndependent/limits.cpp
    ${glslang_dir}/glslang/MachineIndependent/linkValidate.cpp
    ${glslang_dir}/glslang/MachineIndependent/LiveTraverser.h
    ${glslang_dir}/glslang/MachineIndependent/localintermediate.h
    ${glslang_dir}/glslang/MachineIndependent/parseConst.cpp
    ${glslang_dir}/glslang/MachineIndependent/ParseContextBase.cpp
    ${glslang_dir}/glslang/MachineIndependent/ParseHelper.cpp
    ${glslang_dir}/glslang/MachineIndependent/ParseHelper.h
    ${glslang_dir}/glslang/MachineIndependent/PoolAlloc.cpp
    ${glslang_dir}/glslang/MachineIndependent/propagateNoContraction.cpp
    ${glslang_dir}/glslang/MachineIndependent/propagateNoContraction.h
    ${glslang_dir}/glslang/MachineIndependent/preprocessor/PpAtom.cpp
    ${glslang_dir}/glslang/MachineIndependent/preprocessor/PpContext.cpp
    ${glslang_dir}/glslang/MachineIndependent/preprocessor/PpContext.h
    ${glslang_dir}/glslang/MachineIndependent/preprocessor/Pp.cpp
    ${glslang_dir}/glslang/MachineIndependent/preprocessor/PpScanner.cpp
    ${glslang_dir}/glslang/MachineIndependent/preprocessor/PpTokens.cpp
    ${glslang_dir}/glslang/MachineIndependent/preprocessor/PpTokens.h
    ${glslang_dir}/glslang/MachineIndependent/reflection.cpp
    ${glslang_dir}/glslang/MachineIndependent/reflection.h
    ${glslang_dir}/glslang/MachineIndependent/RemoveTree.cpp
    ${glslang_dir}/glslang/MachineIndependent/RemoveTree.h
    ${glslang_dir}/glslang/MachineIndependent/ScanContext.h
    ${glslang_dir}/glslang/MachineIndependent/Scan.cpp
    ${glslang_dir}/glslang/MachineIndependent/Scan.h
    ${glslang_dir}/glslang/MachineIndependent/ShaderLang.cpp
    ${glslang_dir}/glslang/MachineIndependent/SymbolTable.cpp
    ${glslang_dir}/glslang/MachineIndependent/SymbolTable.h
    ${glslang_dir}/glslang/MachineIndependent/Versions.cpp
    ${glslang_dir}/glslang/MachineIndependent/Versions.h
    ${glslang_dir}/glslang/MachineIndependent/attribute.cpp
    ${glslang_dir}/glslang/MachineIndependent/attribute.h
    ${glslang_dir}/glslang/OSDependent/osinclude.h
    ${glslang_dir}/glslang/Public/ShaderLang.h)

if(UNIX)
    list(APPEND glslang_sources
        ${glslang_dir}/glslang/OSDependent/Unix/ossource.cpp)
endif()

set(sources
    glslang_compile.cpp
    glslang_compile.h
    spirv_common.cpp
    spirv_common.h
    spirv_editor.cpp
    spirv_editor.h
    spirv_gen.cpp
    spirv_gen.h
    spirv_op_helpers.h
    spirv_compile.cpp
    spirv_compile.h
    spirv_debug_setup.cpp
    spirv_debug_glsl450.cpp
    spirv_debug.cpp
    spirv_debug.h
    spirv_reflect.cpp
    spirv_reflect.h
    spirv_processor.cpp
    spirv_processor.h
    spirv_disassemble.cpp
    spirv_stringise.cpp
    var_dispatch_helpers.h
    ${glslang_sources})

add_definitions(-DAMD_EXTENSIONS)
add_definitions(-DNV_EXTENSIONS)

set_property(SOURCE ${glslang_sources}
    PROPERTY COMPILE_FLAGS "-Wno-ignored-qualifiers -Wno-strict-aliasing -Wno-unreachable-code-break")

# GCC 7.0 and above needs -Wno-implicit-fallthrough on these files
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.9)
    set_property(SOURCE
        ${glslang_dir}/glslang/MachineIndependent/reflection.cpp
        ${glslang_dir}/glslang/MachineIndependent/Intermediate.cpp
        ${glslang_dir}/glslang/MachineIndependent/ParseHelper.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-implicit-fallthrough")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_property(SOURCE ${glslang_sources}
        APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-unknown-warning-option -Wno-shadow")

    set_property(SOURCE 
        spirv_debug.cpp
        spirv_debug_glsl450.cpp
        spirv_debug_setup.cpp
        spirv_processor.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-shadow -Wno-shorten-64-to-32")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 9.9)
        set_property(SOURCE ${glslang_dir}/glslang/MachineIndependent/iomapper.cpp
            APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-deprecated-copy")
    endif()
endif()

add_library(rdoc_spirv OBJECT ${sources})
target_compile_definitions(rdoc_spirv ${RDOC_DEFINITIONS})
target_include_directories(rdoc_spirv ${RDOC_INCLUDES} ${glslang_dir})
