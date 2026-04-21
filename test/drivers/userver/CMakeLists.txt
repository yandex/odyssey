cmake_minimum_required(VERSION 3.16)
project(userver_notes CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(userver REQUIRED COMPONENTS core postgresql)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/create_note.cpp
    src/get_note.cpp
    src/ping.cpp
    src/portal_fill.cpp
    src/portal_scan.cpp
)

target_link_libraries(${PROJECT_NAME}
    userver::core
    userver::postgresql
)
