cmake_minimum_required(VERSION 3.10)
project(trackpad_is_too_damn_big C CXX)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra -Wpedantic)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EVDEV REQUIRED libevdev)

add_executable(titdb src/main.cpp src/evdev_helper.c)

target_include_directories(titdb PRIVATE ${EVDEV_INCLUDE_DIRS})
target_link_libraries(titdb ${EVDEV_LIBRARIES})
