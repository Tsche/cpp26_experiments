project(slo)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -v")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")

# add_executable(echo "echo.cpp")
# target_link_libraries(echo PRIVATE erl)

function(DEFINE_EXAMPLE TARGET)
  add_executable(${TARGET} "${TARGET}.cpp")
  target_link_libraries(${TARGET} PRIVATE erl)
endfunction()

define_example(logging)
define_example(args)
define_example(rpc)
