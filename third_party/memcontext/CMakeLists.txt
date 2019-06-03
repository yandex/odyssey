cmake_minimum_required(VERSION 3.5)
project(libmemcontext VERSION 1.0.0 LANGUAGES C)

# main library
set(src
	src/memcontext.c
)

add_library(memcontext ${src})
target_compile_options(memcontext PRIVATE -Werror -Wall -Wextra)
target_compile_features(memcontext PRIVATE c_std_11)
option(MCXT_CHECK "add additinal checking fields and valgrind support" OFF)
option(MCXT_TESTS "add additinal checking fields and valgrind support" ON)

if (MCXT_CHECK)
	find_package(PkgConfig)
	pkg_check_modules(VALGRIND valgrind)
endif()

include(CheckTypeSize)
check_type_size("void*" SIZEOF_VOID_P)
configure_file(src/memconsts.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/memconsts.h)

find_package (Threads)
target_link_libraries(memcontext Threads::Threads)


target_include_directories(memcontext
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
		${VALGRIND_INCLUDEDIR}
)

if (MCXT_CHECK)
	message(STATUS "Memory contexts check mode: ON")
else()
	message(STATUS "Memory contexts check mode: OFF")
endif()

if (MCXT_TESTS)
	message(STATUS "Memory contexts build tests: ON")
	find_package(cmocka 1.1.3)

	list(APPEND tests_names "test_memcontext")
	list(APPEND tests_options "-Wall -Werror -Wextra")
	list(APPEND tests_link "-Wl,--wrap,free")

	# declare all tests targets
	list(LENGTH tests_names count)
	math(EXPR count "${count} - 1")
	foreach(i RANGE ${count})
		list(GET tests_names ${i} test_name)
		list(GET tests_options ${i} test_options)
		list(GET tests_link ${i} test_link)
		add_executable(${test_name} src/${test_name}.c)
		target_include_directories(${test_name}
			PUBLIC
				$<INSTALL_INTERFACE:include>
				$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
			PRIVATE
				${CMAKE_CURRENT_SOURCE_DIR}/src
				${VALGRIND_INCLUDEDIR}
		)
		target_link_libraries(
			${test_name}
			cmocka
			-fprofile-arcs
			memcontext
			${test_link}
		)
		target_compile_features(${test_name}
			PRIVATE c_std_11
		)
		add_test(${test_name} ${test_name})
	endforeach()
	enable_testing()
endif()
