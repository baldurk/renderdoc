set(RDOC_HAJACK_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})


set(hajack_sources
  ${RDOC_HAJACK_SOURCE_DIR}/hajack.h
  ${RDOC_HAJACK_SOURCE_DIR}/hajack.cpp
  ${RDOC_HAJACK_SOURCE_DIR}/utils.h
  ${RDOC_HAJACK_SOURCE_DIR}/utils.cpp
  ${RDOC_HAJACK_SOURCE_DIR}/jsoncpp/json/json.h
  ${RDOC_HAJACK_SOURCE_DIR}/jsoncpp/json/json-forwards.h
  ${RDOC_HAJACK_SOURCE_DIR}/jsoncpp/jsoncpp.cpp
)

set(CompilerFlags
  CMAKE_C_FLAGS
  CMAKE_C_FLAGS_DEBUG
  CMAKE_C_FLAGS_RELEASE
  CMAKE_C_FLAGS_MINSIZEREL
  CMAKE_C_FLAGS_RELWITHDEBINFO
  CMAKE_CXX_FLAGS
  CMAKE_CXX_FLAGS_DEBUG
  CMAKE_CXX_FLAGS_RELEASE
  CMAKE_CXX_FLAGS_MINSIZEREL
  CMAKE_CXX_FLAGS_RELWITHDEBINFO
  )
foreach(CompilerFlag ${CompilerFlags})
  SET("${CompilerFlag}" "${${CompilerFlag}} -Wno-unneeded-internal-declaration -Wno-tautological-unsigned-enum-zero-compare")
endforeach()

add_library(rdoc_hajack OBJECT ${hajack_sources})
target_compile_definitions(rdoc_hajack ${RDOC_DEFINITIONS})
target_include_directories(rdoc_hajack ${RDOC_INCLUDES})
