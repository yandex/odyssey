
macro(build_machinarium)
	set(MACHINARIUM_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/lib/machinarium/src)
	set(MACHINARIUM_OPTS CFLAGS="${CMAKE_C_FLAGS}" LDFLAGS="${CMAKE_STATIC_LINKER_FLAGS}")
	separate_arguments(MACHINARIUM_OPTS)
	if (${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/machinarium/src/libmachinarium${CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}/lib/machinarium -DBUILD_STATIC=ON
			COMMAND ${CMAKE_MAKE_PROGRAM} ${MACHINARIUM_OPTS} -C ${PROJECT_BINARY_DIR}/lib/machinarium
			WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/lib/machinarium
		)
	else()
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/machinarium/src/libmachinarium${CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/lib/machinarium
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib/machinarium ${PROJECT_BINARY_DIR}/lib/machinarium
			COMMAND cd ${PROJECT_BINARY_DIR}/lib/machinarium && ${CMAKE_COMMAND} -DBUILD_STATIC=ON .
			COMMAND ${CMAKE_MAKE_PROGRAM} ${MACHINARIUM_OPTS} -C ${PROJECT_BINARY_DIR}/lib/machinarium
		)
	endif()
	add_custom_target(libmachinarium ALL
		DEPENDS ${PROJECT_BINARY_DIR}/lib/machinarium/src/libmachinarium${CMAKE_STATIC_LIBRARY_SUFFIX}
	)
	message(STATUS "Use shipped libmachinarium: ${PROJECT_SOURCE_DIR}/lib/machinarium")
	set (MACHINARIUM_LIBRARIES "${PROJECT_BINARY_DIR}/lib/machinarium/src/libmachinarium${CMAKE_STATIC_LIBRARY_SUFFIX}")
	set (MACHINARIUM_LIBRARIES_LIBUV "${PROJECT_BINARY_DIR}/lib/machinarium/lib/libuv/.libs/libuv.a")
	set (MACHINARIUM_FOUND 1)
	add_dependencies(build_libs libmachinarium)
endmacro(build_machinarium)
