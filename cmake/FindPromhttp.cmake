# - Try to find the Prometheus C client library
#
#  PROMHTTP_FOUND - promhttp was successfully found
#  PROMHTTP_INCLUDE_DIR - promhttp include directory
#  PROMHTTP_LIBRARY - promhttp library

find_path(PROMHTTP_INCLUDE_DIR NAMES promhttp.h)
find_library(PROMHTTP_LIBRARY promhttp)

find_package_handle_standard_args(Promhttp REQUIRED_VARS PROMHTTP_LIBRARY PROMHTTP_INCLUDE_DIR)
