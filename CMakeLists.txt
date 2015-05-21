cmake_minimum_required (VERSION 2.6)

set (CMAKE_C_COMPILER "/usr/local/bin/clang")
set (CMAKE_CXX_COMPILER "/usr/local/bin/clang++")
#set (CMAKE_C_COMPILER "clang-3.5")
#set (CMAKE_CXX_COMPILER "clang++-3.5")

set (PROJECT_NAME MyProject)
project (${PROJECT_NAME})

# Compile flags
SET (CMAKE_C_FLAGS                "-Wall -std=c11 -Wextra -Werror")
SET (CMAKE_C_FLAGS_DEBUG          "${CMAKE_CFLAGS} -g")
SET (CMAKE_C_FLAGS_MINSIZEREL     "${CMAKE_CFLAGS} -Os -DNDEBUG")
SET (CMAKE_C_FLAGS_RELEASE        "${CMAKE_CFLAGS} -O3 -DNDEBUG")
SET (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_CFLAGS} -O2 -g")

SET (CMAKE_CXX_FLAGS                "-Wall -fno-rtti -std=c++11 -Wextra -Werror")
SET (CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXXFLAGS} -g")
SET (CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXXFLAGS} -Os -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXXFLAGS} -O2 -g")
SET (CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXXFLAGS} -O3 -DNDEBUG")

## Set optional features.  This will show up as a preprocessor variable
option (USE_GLIBCXX_DEBUG 
  "Use the _GLIBCXX_DEBUG macro for nicer debugging and error checking of the c++ standard library.  Warning, it breaks ABI compatibility so don't pass container instantiations between translation units that don't have the same debug mode."
  OFF)

## Support for Clang's CompilationDatabase system
set (CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_executable (${PROJECT_NAME} 
  src/main.cc
  )
target_link_libraries (${PROJECT_NAME} ${LIBS})

### EXTRACT TEMPLATES
string(TOUPPER ${CMAKE_BUILD_TYPE} DEBUG_MODE)
if (DEBUG_MODE MATCHES "DEBUG")
  set (PCH_FLAGS -O0)
elseif (DEBUG_MODE MATCHES "RELEASE")
  set (PCH_FLAGS -O3)
elseif (DEBUG_MODE MATCHES "RELWITHDEBINFO")
  set (PCH_FLAGS -O2 -g)
elseif (DEBUG_MODE MATCHES "MINSIZEREL")
  set (PCH_FLAGS -Os)
endif (DEBUG_MODE MATCHES "DEBUG")

add_custom_command (OUTPUT libtooling_headers.hpp.pch
  COMMAND clang++ 
  ARGS -x c++-header -std=c++11 ${PCH_FLAGS} -fno-rtti
  -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
  ${CMAKE_HOME_DIRECTORY}/src/libtooling_headers.hpp 
  -o libtooling_headers.hpp.pch 
  DEPENDS ${CMAKE_HOME_DIRECTORY}/src/libtooling_headers.hpp
  VERBATIM
  )

add_executable (extract-templates
  src/extract_templates.cc
  )

set_source_files_properties (src/extract_templates.cc
  PROPERTIES 
  COMPILE_FLAGS "-include libtooling_headers.hpp -Wno-unused-parameter -I/usr/local/src/clang-llmv/llvm/include -I/usr/local/src/clang-llmv/build/include -I/usr/local/src/clang-llmv/llvm/tools/clang/include -I/usr/local/src/clang-llmv/build/tools/clang/include -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS"
  OBJECT_DEPENDS libtooling_headers.hpp.pch
  )
set_target_properties (extract-templates
  PROPERTIES
  LINK_FLAGS "-L/usr/local/src/clang/build/lib"
  )

set (UNUSED_LIBS
  clangAnalysis
  clangDriver
  clangEdit
  clangFrontendTool
  clangLex
  clangParse
  clangSema
  clangEdit
  clangRewrite
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangStaticAnalyzerCheckers
  clangStaticAnalyzerCore
  clangSerialization
  clangToolingCore
  LLVMCore
  LLVMMCParser
  LLVMMC
  LLVMOption
  rt dl tinfo pthread z m
)

target_link_libraries (extract-templates 
  clangTooling
  clangBasic
  clangASTMatchers
  clangAST
  clangFrontend
  LLVMSupport
  )


if (USE_GLIBCXX_DEBUG)
  # Prefer using G++ for debug mode because the GLIBCXX_DEBUG flag for pretty
  # printing collections in GDB doesn't seem to work with clang.
  string(TOUPPER ${CMAKE_BUILD_TYPE} DEBUG_MODE)
  if (DEBUG_MODE MATCHES "DEBUG")
    set (CMAKE_CXX_COMPILER "g++")
    set (CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXXFLAGS} -g -D_GLIBCXX_DEBUG")
  elseif (DEBUG_MODE MATCHES "RELWITHDEBINFO")
    set (CMAKE_CXX_COMPILER "g++")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXXFLAGS} -O2 -g -D_GLIBCXX_DEBUG")
  endif (DEBUG_MODE MATCHES "DEBUG")
endif (USE_GLIBCXX_DEBUG)


