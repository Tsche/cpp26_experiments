project(slo)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -v")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")

# add_executable(echo "echo.cpp")
# target_link_libraries(echo PRIVATE experiments)
target_compile_options(experiments PUBLIC "-O3")
add_executable(logging "logging.cpp")
target_link_libraries(logging PRIVATE experiments)
