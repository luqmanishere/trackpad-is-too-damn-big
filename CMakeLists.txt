cmake_minimum_required(VERSION 3.10)
project(trackpad_is_too_damn_big)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra -Wpedantic)
find_package(evdev)

add_executable(titdb main.cpp)

target_include_directories(titdb PRIVATE /usr/include/libevdev-1.0/)
target_link_libraries(titdb evdev)
