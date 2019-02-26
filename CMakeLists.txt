cmake_minimum_required(VERSION 2.8)

project(j4-dmenu)

option(WITH_TESTS "Build and run tests" ON)
option(WITH_GIT_CATCH "Use a Git checkout of Catch to build the tests" ON)

if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.7")
  set(CXX_OPT "-std=c++0x")
else()
  set(CXX_OPT "-std=c++11")
endif()

# _WITH_GETLINE for FreeBSD
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_OPT} -Wall -Wextra -pedantic -O2 -D_WITH_GETLINE")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if(WITH_TESTS)
  enable_testing(true)
  add_test(
          NAME j4-dmenu-tests
          COMMAND j4-dmenu-tests
  )
  add_executable(
          j4-dmenu-tests
          src/Test.cc
  )
  add_definitions(-DTEST_FILES="${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  if(WITH_GIT_CATCH)
    include(ExternalProject)
    ExternalProject_Add(
      catch
      PREFIX ${CMAKE_BINARY_DIR}/catch
      GIT_TAG Catch1.x
      GIT_REPOSITORY https://github.com/catchorg/Catch2
      TIMEOUT 10
      UPDATE_COMMAND git pull
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
      LOG_DOWNLOAD ON
    )
    add_dependencies(j4-dmenu-tests catch)

    # Expose required variable (CATCH_INCLUDE_DIR) to parent scope
    ExternalProject_Get_Property(catch source_dir)
    set(CATCH_INCLUDE_DIR ${source_dir}/include CACHE INTERNAL "Path to include folder for Catch")
  else()
    # Use system-installed version of Catch
    find_path(CATCH_INCLUDE_DIR catch.hpp PATH_SUFFIXES catch)
    if(NOT CATCH_INCLUDE_DIR)
      message(FATAL_ERROR "Catch include directory not found")
    endif()
  endif(WITH_GIT_CATCH)

  # Include Catch in the project
  include_directories(${CATCH_INCLUDE_DIR} ${COMMON_INCLUDES})
endif(WITH_TESTS)

add_executable(
    j4-dmenu-desktop
    src/main.cc
)

install (
	TARGETS j4-dmenu-desktop
        RUNTIME DESTINATION bin
	)

