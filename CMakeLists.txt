cmake_minimum_required(VERSION 2.8)

PROJECT(j4-dmenu)

INCLUDE_DIRECTORIES(include)
SET(CMAKE_CXX_FLAGS "-std=c++11 -Wall -pedantic -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

ADD_EXECUTABLE(
    j4-dmenu-desktop
    src/util.cc
    src/main.cc
    src/desktop.cc
    src/locale.cc
)

INSTALL (TARGETS j4-dmenu-desktop
         RUNTIME DESTINATION bin)
