cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

project(erl LANGUAGES CXX)

add_library(erl SHARED "")
target_compile_options(erl PUBLIC 
  "-fexperimental-library"
  "-stdlib=libc++"
  "-freflection"
  "-fannotation-attributes"
  "-fparameter-reflection"
  "-fexpansion-statements"
  "-ftime-trace"
  "-Wno-c++26-extensions"
  "-O0"
)
target_link_options(erl PUBLIC "-fexperimental-library" "-stdlib=libc++" "-lc++abi")

include(GenerateExportHeader)
generate_export_header(erl EXPORT_FILE_NAME ${CMAKE_CURRENT_LIST_DIR}/include/erl/_impl/export.h)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(erl PUBLIC Threads::Threads)

add_subdirectory(modules)
add_subdirectory(src)

target_include_directories(erl PUBLIC 
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

add_subdirectory(example)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/ DESTINATION include)

## binaries
install(TARGETS erl
        ARCHIVE DESTINATION lib # Windows import libraries (.lib)
        RUNTIME DESTINATION bin # Windows DLLs
        LIBRARY DESTINATION lib) # shared libraries (ie Linux .so)