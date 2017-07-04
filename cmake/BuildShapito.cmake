
macro(build_shapito)
	set(SHAPITO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/third_party/shapito/sources)
	if (${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/third_party/shapito/sources/libshapito${CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}/third_party/shapito -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			COMMAND ${CMAKE_MAKE_PROGRAM} -C ${PROJECT_BINARY_DIR}/third_party/shapito
			WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/third_party/shapito
		)
	else()
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/third_party/shapito/sources/libshapito{CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/third_party/shapito
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/third_party/shapito ${PROJECT_BINARY_DIR}/third_party/shapito
			COMMAND cd ${PROJECT_BINARY_DIR}/third_party/shapito && ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .
			COMMAND ${CMAKE_MAKE_PROGRAM} -C ${PROJECT_BINARY_DIR}/third_party/shapito
		)
	endif()
	add_custom_target(libshapito ALL
		DEPENDS ${PROJECT_BINARY_DIR}/third_party/shapito/sources/libshapito${CMAKE_STATIC_LIBRARY_SUFFIX}
	)
	message(STATUS "Use shipped libshapito: ${PROJECT_SOURCE_DIR}/third_party/shapito")
	set (SHAPITO_LIBRARIES "${PROJECT_BINARY_DIR}/third_party/shapito/sources/libshapito${CMAKE_STATIC_LIBRARY_SUFFIX}")
	set (SHAPITO_FOUND 1)
	add_dependencies(build_libs libshapito)
endmacro(build_shapito)
