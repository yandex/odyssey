#
#  LIBUV_FOUND - system has libuv
#  LIBUV_INCLUDE_DIR - the libuv include directory
#  LIBUV_LIBRARIES - libuv library

set (LIBUV_LIBRARY_DIRS "")

find_library(LIBUV_LIBRARIES NAMES uv)
find_path(LIBUV_INCLUDE_DIRS NAMES uv.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBUV DEFAULT_MSG LIBUV_LIBRARIES LIBUV_INCLUDE_DIRS)
