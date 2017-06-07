
macro(build_shapito)
	set(SHAPITO_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/lib/shapito/src)
	if (${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/shapito/src/libshapito${CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}/lib/shapito -DBUILD_STATIC=ON -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			COMMAND ${CMAKE_MAKE_PROGRAM} -C ${PROJECT_BINARY_DIR}/lib/shapito
			WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/lib/shapito
		)
	else()
		add_custom_command(
			OUTPUT  ${PROJECT_BINARY_DIR}/lib/shapito/src/libshapito{CMAKE_STATIC_LIBRARY_SUFFIX}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/lib/shapito
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib/shapito ${PROJECT_BINARY_DIR}/lib/shapito
			COMMAND cd ${PROJECT_BINARY_DIR}/lib/shapito && ${CMAKE_COMMAND} -DBUILD_STATIC=ON -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .
			COMMAND ${CMAKE_MAKE_PROGRAM} -C ${PROJECT_BINARY_DIR}/lib/shapito
		)
	endif()
	add_custom_target(libshapito ALL
		DEPENDS ${PROJECT_BINARY_DIR}/lib/shapito/src/libshapito${CMAKE_STATIC_LIBRARY_SUFFIX}
	)
	message(STATUS "Use shipped libshapito: ${PROJECT_SOURCE_DIR}/lib/shapito")
	set (SHAPITO_LIBRARIES "${PROJECT_BINARY_DIR}/lib/shapito/src/libshapito${CMAKE_STATIC_LIBRARY_SUFFIX}")
	set (SHAPITO_FOUND 1)
	add_dependencies(build_libs libshapito)
endmacro(build_shapito)
