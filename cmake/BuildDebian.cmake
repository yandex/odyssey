set(CPACK_PACKAGE_NAME "${PROJECT_NAME}"
    CACHE STRING "The resulting package name"
)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Scalable PostgreSQL connection pooler."
    CACHE STRING "Package description for the package metadata"
)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Advanced multi-threaded PostgreSQL connection pooler and request router."
    CACHE STRING "Package description for the package metadata"
)
set(CPACK_PACKAGE_VENDOR "YANDEX LLC")
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_SOURCE_DIR}/packages")
set(CPACK_PACKAGE_CONTACT "Roman Khapov <r.khapov@ya.ru>")
set(CPACK_PACKAGE_VERSION "${OD_VERSION}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Roman Khapov <r.khapov@ya.ru>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/yandex/odyssey")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "make, cmake, libssl-dev (>= 1.0.1), libpq-dev, libpam-dev, postgresql-server-dev-all, libzstd-dev, zlib1g-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "pgbouncer")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(
    CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)

# dbgsym generation
set(CPACK_DEB_COMPONENT_INSTALL YES)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)

# Generate and insert changelog
execute_process(COMMAND date -u -R OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CHANGELOG_RENDERED_PATH "${CMAKE_CURRENT_BINARY_DIR}/debian/changelog")
set(CHANGELOG_GZ_PATH "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian.gz")
configure_file("${CMAKE_SOURCE_DIR}/debian/changelog@" "${CHANGELOG_RENDERED_PATH}" @ONLY)

include(GNUInstallDirs)
add_custom_command(
    OUTPUT "${CHANGELOG_GZ_PATH}"
    COMMAND gzip -cn9 "${CHANGELOG_RENDERED_PATH}" > "${CHANGELOG_GZ_PATH}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS "${CHANGELOG_RENDERED_PATH}"
    COMMENT "Compressing changelog"
)
add_custom_target(changelog ALL DEPENDS "${CHANGELOG_GZ_PATH}")
install(FILES "${CHANGELOG_GZ_PATH}" DESTINATION "${CMAKE_INSTALL_DOCDIR}")

# For .changes file
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/CVS/;/\\.svn/;\\.swp$;\\.#;/#;.*~;cscope.*")

include(CPack)
