
macro(build_machinarium)
	set(MACHINARIUM_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/lib/machinarium/lib)
	set(MACHINARIUM_OPTS CFLAGS="${CMAKE_C_FLAGS}" LDFLAGS="${CMAKE_SHARED_LINKER_FLAGS}")
	separate_arguments(MACHINARIUM_OPTS)
	if (${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/machinarium/lib/libmachinarium${CMAKE_SHARED_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}/lib/machinarium
			COMMAND ${CMAKE_MAKE_PROGRAM} ${MACHINARIUM_OPTS} -C ${PROJECT_BINARY_DIR}/lib/machinarium
			WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/lib/machinarium
		)
	else()
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/machinarium/lib/libmachinarium${CMAKE_SHARED_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/lib/machinarium
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib/machinarium ${PROJECT_BINARY_DIR}/lib/machinarium
			COMMAND cd ${PROJECT_BINARY_DIR}/lib/machinarium && ${CMAKE_COMMAND} .
			COMMAND ${CMAKE_MAKE_PROGRAM} ${MACHINARIUM_OPTS} -C ${PROJECT_BINARY_DIR}/lib/machinarium
		)
	endif()
	add_custom_target(libmachinarium ALL
		DEPENDS ${PROJECT_BINARY_DIR}/lib/machinarium/lib/libmachinarium${CMAKE_SHARED_LIBRARY_SUFFIX}
	)
	message(STATUS "Use shipped libmachinarium: ${PROJECT_SOURCE_DIR}/lib/machinarium")
	set (MACHINARIUM_LIBRARIES "${PROJECT_BINARY_DIR}/lib/machinarium/lib/libmachinarium${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set (MACHINARIUM_FOUND 1)
	add_dependencies(build_libs libmachinarium)
endmacro(build_machinarium)
