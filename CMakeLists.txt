cmake_minimum_required(VERSION 3.16)

project(j4-dmenu)

option(WITH_TESTS "Build and run tests" ON)
option(WITH_GIT_CATCH "Use a Git checkout of Catch to build the tests" ON)

set(CMAKE_CXX_STANDARD 17)

# _WITH_GETLINE for FreeBSD
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-missing-field-initializers -O2 -D_WITH_GETLINE"
)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL FreeBSD)
  option(USE_KQUEUE
         "Use the kqueue event notification mechanism instead of Inotify" ON)
else()
  option(USE_KQUEUE
         "Use the kqueue event notification mechanism instead of Inotify" OFF)
endif()

if(USE_KQUEUE)
  find_package(Threads REQUIRED)
  add_compile_definitions(USE_KQUEUE)
endif()

include_directories("${PROJECT_BINARY_DIR}")

if(WITH_TESTS)
  add_executable(j4-dmenu-tests src/Test.cc)
  target_compile_definitions(
    j4-dmenu-tests PRIVATE TEST_FILES="${CMAKE_CURRENT_SOURCE_DIR}/test_files/")

  if(WITH_GIT_CATCH)
    include(FetchContent)

    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.4.0)

    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  else()
    # Use system-installed version of Catch
    find_package(Catch2 3 REQUIRED)
  endif(WITH_GIT_CATCH)

  # Include Catch in the project
  target_link_libraries(j4-dmenu-tests PRIVATE Catch2::Catch2WithMain)

  include(CTest)
  include(Catch)
  catch_discover_tests(j4-dmenu-tests)
endif(WITH_TESTS)

add_executable(j4-dmenu-desktop src/main.cc)

if(USE_KQUEUE)
  if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(j4-dmenu-desktop PUBLIC "-pthread")
    target_compile_options(j4-dmenu-tests PUBLIC "-pthread")
  endif()
  if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(j4-dmenu-desktop "${CMAKE_THREAD_LIBS_INIT}")
    target_link_libraries(j4-dmenu-tests "${CMAKE_THREAD_LIBS_INIT}")
  endif()
endif()

install(TARGETS j4-dmenu-desktop RUNTIME DESTINATION bin)
