# - Try to find the LDAP libraries
#
#  LDAP_FOUND - ldap was successfully found
#  LDAP_INCLUDE_DIR - ldap include directory
#  LDAP_LIBRARIES - ldap libraries

if (APPLE)
    # macOS ships a deprecated LDAP.framework whose ldap.h header is broken
    # when included directly (causes "include nested too deeply").
    # Prefer OpenLDAP installed via Homebrew instead, and explicitly
    # disable framework-style lookup for this search.
    find_path(LDAP_INCLUDE_DIR NAMES ldap.h
        HINTS
            /opt/homebrew/opt/openldap/include
            /usr/local/opt/openldap/include
        NO_CMAKE_FIND_ROOT_PATH
        NO_DEFAULT_PATH
    )
    find_library(LDAP_LIBRARY NAMES ldap
        HINTS
            /opt/homebrew/opt/openldap/lib
            /usr/local/opt/openldap/lib
        NO_CMAKE_FIND_ROOT_PATH
        NO_DEFAULT_PATH
    )
else()
    find_path(LDAP_INCLUDE_DIR NAMES ldap.h)
    find_library(LDAP_LIBRARY ldap)
endif()

find_package_handle_standard_args(LDAP REQUIRED_VARS LDAP_LIBRARY LDAP_INCLUDE_DIR)
