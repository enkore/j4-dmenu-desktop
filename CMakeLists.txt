cmake_minimum_required(VERSION 2.8)

PROJECT(j4-dmenu)

INCLUDE_DIRECTORIES(include)
SET(CMAKE_CXX_FLAGS "-std=c++0x -g")

ADD_EXECUTABLE(
    j4-dmenu
    src/util.cc
    src/main.cc
)