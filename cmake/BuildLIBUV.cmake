
macro(build_libuv)
	set(LIBUV_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/lib/libuv/include)
	set(LIBUV_OPTS CFLAGS="${CMAKE_C_FLAGS}" LDFLAGS="${CMAKE_STATIC_LINKER_FLAGS}")
	separate_arguments(LIBUV_OPTS)
	if (${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/libuv/.libs/libuv${CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ./autogen.sh
			COMMAND ./configure
			COMMAND ${CMAKE_MAKE_PROGRAM} ${LIBUV_OPTS} -C ${PROJECT_BINARY_DIR}/lib/libuv
			WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/lib/libuv
		)
	else()
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/libuv/.libs/libuv${CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/lib/libuv
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib/libuv ${PROJECT_BINARY_DIR}/lib/libuv
			COMMAND cd ${PROJECT_BINARY_DIR}/lib/libuv && ./autogen.sh
			COMMAND cd ${PROJECT_BINARY_DIR}/lib/libuv && ./configure
			COMMAND ${CMAKE_MAKE_PROGRAM} ${LIBUV_OPTS} -C ${PROJECT_BINARY_DIR}/lib/libuv
		)
	endif()
	add_custom_target(libuv ALL
		DEPENDS ${PROJECT_BINARY_DIR}/lib/libuv/.libs/libuv${CMAKE_STATIC_LIBRARY_SUFFIX}
	)
	message(STATUS "Use shipped libuv: ${PROJECT_SOURCE_DIR}/lib/libuv")
	set (LIBUV_LIBRARIES "${PROJECT_BINARY_DIR}/lib/libuv/.libs/libuv${CMAKE_STATIC_LIBRARY_SUFFIX}")
	set (LIBUV_FOUND 1)
	add_dependencies(build_libs libuv)
endmacro(build_libuv)
