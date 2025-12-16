# Find cJSON library
# This will define:
#  CJSON_FOUND - System has cJSON
#  CJSON_INCLUDE_DIR - The cJSON include directory
#  CJSON_LIBRARY - The library needed to use cJSON

find_path(CJSON_INCLUDE_DIR
    NAMES cjson/cJSON.h
    HINTS
        /usr/include
        /usr/local/include
)

find_library(CJSON_LIBRARY
    NAMES cjson
    HINTS
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cJSON
    REQUIRED_VARS CJSON_LIBRARY CJSON_INCLUDE_DIR
)

mark_as_advanced(CJSON_INCLUDE_DIR CJSON_LIBRARY)
