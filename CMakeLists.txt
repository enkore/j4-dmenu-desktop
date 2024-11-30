cmake_minimum_required(VERSION 3.16)

project(j4-dmenu CXX)

include(CMakeDependentOption)

option(WITH_TESTS "Build and run tests" ON)
option(NO_DOWNLOAD "Do not download any dependencies (or anything else); overrides all WITH_GIT_* variables" OFF)
cmake_dependent_option(WITH_GIT_CATCH "Use a Git checkout of Catch to build the tests" ON "NOT NO_DOWNLOAD" OFF)
cmake_dependent_option(WITH_GIT_SPDLOG "Use a Git checkout of spdlog to build" ON "NOT NO_DOWNLOAD" OFF)
cmake_dependent_option(WITH_GIT_FMT "Use a Git checkout of fmt to build" ON "NOT NO_DOWNLOAD" OFF)

set(CMAKE_CXX_STANDARD 17)

# J4dd should have two translation units: one for j4-dmenu-desktop and one for
# j4-dmenu-tests (+ dependencies). This might be changed in the future to a
# more traditional compilation model.
set(CMAKE_UNITY_BUILD TRUE)
# Set basically infinite limit.
set(CMAKE_UNITY_BUILD_BATCH_SIZE 32768)

# _WITH_GETLINE for FreeBSD
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -O2 -D_WITH_GETLINE -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG"
)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL FreeBSD)
  option(USE_KQUEUE
         "Use the kqueue event notification mechanism instead of Inotify" ON)
else()
  option(USE_KQUEUE
         "Use the kqueue event notification mechanism instead of Inotify" OFF)
endif()

SET(SOURCE AppManager.cc Application.cc FieldCodes.cc Dmenu.cc FileFinder.cc Formatters.cc HistoryManager.cc I3Exec.cc LocaleSuffixes.cc SearchPath.cc Utilities.cc LineReader.cc CMDLineAssembler.cc CMDLineTerm.cc)
list(TRANSFORM SOURCE PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/")

SET(OVERRIDE_VERSION "" CACHE STRING "Override version")

if(OVERRIDE_VERSION STREQUAL "")
  # Version handling
  file(STRINGS "version.txt" VCS_TAG)

  find_package(Git)

  if(GIT_EXECUTABLE)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --dirty --broken
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
      RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT GIT_DESCRIBE_ERROR_CODE)
      set(VCS_TAG ${GIT_DESCRIBE_VERSION})
    endif()
  endif()
else()
  set(VCS_TAG ${OVERRIDE_VERSION})
endif()

configure_file(generated/version.cc.in generated/version.cc @ONLY)

if(USE_KQUEUE)
  find_package(Threads REQUIRED)
  add_compile_definitions(USE_KQUEUE)
  list(APPEND SOURCE src/NotifyKqueue.cc)
else()
  list(APPEND SOURCE src/NotifyInotify.cc)
endif()

include_directories("${PROJECT_BINARY_DIR}")

if(WITH_GIT_SPDLOG)
  include(FetchContent)
  FetchContent_Declare(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    GIT_TAG        "v1.14.1"
    GIT_SHALLOW    true
  )
  FetchContent_MakeAvailable(spdlog)
else()
  # Use system-installed version of Catch
  find_package(spdlog REQUIRED)
endif(WITH_GIT_SPDLOG)

if(WITH_GIT_FMT)
  include(FetchContent)
  set(FMT_INSTALL OFF CACHE INTERNAL "Disable fmt install target")
  FetchContent_Declare(fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG        "10.2.0"
    GIT_SHALLOW    true
  )
  FetchContent_MakeAvailable(fmt)
else()
  find_package(fmt REQUIRED)
endif(WITH_GIT_FMT)

add_executable(j4-dmenu-desktop ${SOURCE} "${CMAKE_CURRENT_BINARY_DIR}/generated/version.cc" src/main.cc)
target_link_libraries(j4-dmenu-desktop PRIVATE spdlog::spdlog PRIVATE fmt::fmt)
target_include_directories(j4-dmenu-desktop PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/")

if(WITH_TESTS)
  file(GLOB test_src_files tests/*.cc)

  set(TEST_FILES "\"${CMAKE_CURRENT_SOURCE_DIR}/tests/test_files/\"")
  configure_file(tests/generated/tests_config.hh.in tests/generated/tests_config.hh @ONLY)

  add_executable(j4-dmenu-tests ${test_src_files} ${SOURCE})
  target_include_directories(j4-dmenu-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/")
  target_include_directories(j4-dmenu-tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/tests/")
  target_link_libraries(j4-dmenu-tests PRIVATE spdlog::spdlog PRIVATE fmt::fmt)

  if(WITH_GIT_CATCH)
    include(FetchContent)

    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.6.0
      GIT_SHALLOW    true
    )

    FetchContent_MakeAvailable(Catch2)
  else()
    # Use system-installed version of Catch
    find_package(Catch2 3 REQUIRED)
  endif(WITH_GIT_CATCH)

  # Include Catch in the project
  target_link_libraries(j4-dmenu-tests PRIVATE Catch2::Catch2)

  include(CTest)
  include(Catch)
  catch_discover_tests(j4-dmenu-tests)

  execute_process(COMMAND pytest --version OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE pytest_error)
  if(pytest_error EQUAL 0)
    add_test(NAME pytest-system-tests COMMAND pytest "${CMAKE_SOURCE_DIR}/tests/system_tests/system_test.py" --j4dd-executable "${CMAKE_CURRENT_BINARY_DIR}/j4-dmenu-desktop")
  else()
    message(WARNING "pytest executable is not available. Skipping pytest tests...")
  endif()
endif(WITH_TESTS)

if(USE_KQUEUE)
  if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(j4-dmenu-desktop PUBLIC "-pthread")
    target_compile_options(j4-dmenu-tests PUBLIC "-pthread")
  endif()
  if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(j4-dmenu-desktop PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
    target_link_libraries(j4-dmenu-tests PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
  endif()
endif()

install(TARGETS j4-dmenu-desktop RUNTIME DESTINATION bin)
INSTALL(FILES j4-dmenu-desktop.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1/)
INSTALL(FILES etc/completions/_j4-dmenu-desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/zsh/site-functions)
INSTALL(FILES etc/completions/j4-dmenu-desktop.fish DESTINATION ${CMAKE_INSTALL_PREFIX}/share/fish/vendor_completions.d)
INSTALL(FILES etc/completions/j4-dmenu-desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions)
