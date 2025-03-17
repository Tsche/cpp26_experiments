project(slo C CXX)

function(DEFINE_EXAMPLE TARGET)
  add_executable(${TARGET} "${TARGET}.cpp")
  target_link_libraries(${TARGET} PRIVATE erl)
endfunction()

define_example(logging)
define_example(args)
define_example(rpc)

add_library(demo_plugin SHARED "plugins/demo.cpp")
target_link_libraries(demo_plugin PRIVATE erl)

define_example(plugin)