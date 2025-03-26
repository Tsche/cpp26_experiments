target_sources(erl PUBLIC dummy.cpp)

add_subdirectory(logging)
add_subdirectory(threading)
add_subdirectory(plugins)
add_subdirectory(platform)

target_sources(erl PUBLIC clock.cpp)