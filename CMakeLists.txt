cmake_minimum_required(VERSION 3.16)

project(j4-dmenu CXX)

option(WITH_TESTS "Build and run tests" ON)
option(WITH_GIT_CATCH "Use a Git checkout of Catch to build the tests" ON)

set(CMAKE_CXX_STANDARD 17)

# J4dd should have two translation units: one for j4-dmenu-desktop and one for
# j4-dmenu-tests (+ dependencies). This might be changed in the future to a
# more traditional compilation model.
set(CMAKE_UNITY_BUILD TRUE)
# Set basically infinite limit.
set(CMAKE_UNITY_BUILD_BATCH_SIZE 32768)

# _WITH_GETLINE for FreeBSD
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -O2 -D_WITH_GETLINE"
)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL FreeBSD)
  option(USE_KQUEUE
         "Use the kqueue event notification mechanism instead of Inotify" ON)
else()
  option(USE_KQUEUE
         "Use the kqueue event notification mechanism instead of Inotify" OFF)
endif()

SET(SOURCE AppManager.cc Application.cc ApplicationRunner.cc Dmenu.cc FileFinder.cc Formatters.cc HistoryManager.cc I3Exec.cc LocaleSuffixes.cc SearchPath.cc Utilities.cc)
list(TRANSFORM SOURCE PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/")

if(USE_KQUEUE)
  find_package(Threads REQUIRED)
  add_compile_definitions(USE_KQUEUE)
  list(APPEND SOURCE src/NotifyKqueue.cc)
else()
  list(APPEND SOURCE src/NotifyInotify.cc)
endif()

include_directories("${PROJECT_BINARY_DIR}")

include(FetchContent)
FetchContent_Declare(LoguruGitRepo
	GIT_REPOSITORY "https://github.com/emilk/loguru"
  # Version v2.1.0 unfortunatelly doesn't work. Use Meson if you want versioned dependencies.
	GIT_TAG        "master"
  GIT_SHALLOW    true
)
FetchContent_MakeAvailable(LoguruGitRepo)

add_executable(j4-dmenu-desktop ${SOURCE} src/main.cc)
target_link_libraries(j4-dmenu-desktop PRIVATE loguru::loguru)

if(WITH_TESTS)
  file(GLOB_RECURSE test_src_files tests/*.cc)

  add_executable(j4-dmenu-tests ${test_src_files} ${SOURCE})
  target_compile_definitions(
    j4-dmenu-tests PRIVATE TEST_FILES="${CMAKE_CURRENT_SOURCE_DIR}/tests/test_files/")
  target_include_directories(j4-dmenu-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/")
  target_link_libraries(j4-dmenu-tests PRIVATE loguru::loguru)

  if(WITH_GIT_CATCH)
    include(FetchContent)

    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.4.0
      GIT_SHALLOW     true
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
endif(WITH_TESTS)

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
INSTALL(FILES j4-dmenu-desktop.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1/)
INSTALL(FILES etc/_j4-dmenu-desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/zsh/site-functions)
INSTALL(FILES etc/j4-dmenu-desktop.fish DESTINATION ${CMAKE_INSTALL_PREFIX}/share/fish/vendor_completions.d)
