
macro(build_soprano)
	set(SOPRANO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/lib/soprano/lib)
	set(SOPRANO_OPTS CFLAGS="${CMAKE_C_FLAGS}" LDFLAGS="${CMAKE_SHARED_LINKER_FLAGS}")
	separate_arguments(SOPRANO_OPTS)
	if (${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/soprano/lib/libsoprano${CMAKE_SHARED_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}/lib/soprano
			COMMAND ${CMAKE_MAKE_PROGRAM} ${SOPRANO_OPTS} -C ${PROJECT_BINARY_DIR}/lib/soprano
			WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/lib/soprano
		)
	else()
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/soprano/lib/libsoprano${CMAKE_SHARED_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/lib/soprano
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib/soprano ${PROJECT_BINARY_DIR}/lib/soprano
			COMMAND cd ${PROJECT_BINARY_DIR}/lib/soprano && ${CMAKE_COMMAND} .
			COMMAND ${CMAKE_MAKE_PROGRAM} ${SOPRANO_OPTS} -C ${PROJECT_BINARY_DIR}/lib/soprano
		)
	endif()
	add_custom_target(libsoprano ALL
		DEPENDS ${PROJECT_BINARY_DIR}/lib/soprano/lib/libsoprano${CMAKE_SHARED_LIBRARY_SUFFIX}
	)
	message(STATUS "Use shipped libsoprano: ${PROJECT_SOURCE_DIR}/lib/soprano")
	set (SOPRANO_LIBRARIES "${PROJECT_BINARY_DIR}/lib/soprano/lib/libsoprano${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set (SOPRANO_FOUND 1)
	add_dependencies(build_libs libsoprano)
endmacro(build_soprano)
