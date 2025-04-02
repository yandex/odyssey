set(CPACK_PACKAGE_NAME "${PROJECT_NAME}"
    CACHE STRING "The resulting package name"
)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Scalable PostgreSQL connection pooler."
    CACHE STRING "Package description for the package metadata"
)
set(CPACK_PACKAGE_VENDOR "YANDEX LLC")
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_SOURCE_DIR}/packages")
set(CPACK_PACKAGE_CONTACT "opensource-support@yandex-team.ru")
set(CPACK_PACKAGE_VERSION "${OD_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "YANDEX LLC")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/yandex/odyssey")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "make, cmake, libssl-dev (>= 1.0.1), libpq-dev, libpam-dev, postgresql-server-dev-all, libzstd-dev, zlib1g-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "pgbouncer")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_DEB_COMPONENT_INSTALL YES)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)

include(CPack)
