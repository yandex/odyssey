
set(od_test_binary odyssey_test)
set(od_test_src odyssey_test.c)

include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_BINARY_DIR}/")

add_executable(${od_test_binary} ${od_test_src})
add_dependencies(${od_test_binary} build_libs)

if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET ${od_test_binary} PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET ${od_test_binary} PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()

target_link_libraries(${od_test_binary} ${od_libraries} ${CMAKE_THREAD_LIBS_INIT})
