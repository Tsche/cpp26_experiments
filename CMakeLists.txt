cmake_minimum_required(VERSION 3.30)
project(slo)

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)

add_library(experiments SHARED "")
target_compile_options(experiments PUBLIC "-stdlib=libc++" "-freflection" "-ftime-trace" "-O3")
target_link_options(experiments PUBLIC "-stdlib=libc++" "-lc++abi")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(experiments PUBLIC Threads::Threads)

add_subdirectory(src)



target_include_directories(experiments PUBLIC 
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

add_subdirectory(example)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/ DESTINATION include)

## binaries
install(TARGETS experiments
        ARCHIVE DESTINATION lib # Windows import libraries (.lib)
        RUNTIME DESTINATION bin # Windows DLLs
        LIBRARY DESTINATION lib) # shared libraries (ie Linux .so)
