# This build file depends on several variables defined in src/meson.build
if get_option('enable-tests')
  # The oldest version of Catch2 I was able to verify is v3.3.0. v2 versions
  # won't likely work. version isn't set to >3.3.0 because 3.3.0 is the oldest
  # Catch2 release I was able to compile (compilation errors are likely caused
  # by newer version of gcc which wasn't used at that time).
  catch2 = dependency('catch2', version: '>3.0.0', default_options: 'tests=false')
else
  catch2 = disabler()
endif

test_files = [
  'FSUtils.cc',
  'TestAppManager.cc',
  'TestApplication.cc',
  'TestHistoryManager.cc',
  'TestDynamicCompare.cc',
  'TestApplicationRunner.cc',
  'TestFileFinder.cc',
  'TestFormatters.cc',
  'TestLocaleSuffixes.cc',
  'TestNotify.cc',
  'TestSearchPath.cc',
  'TestI3Exec.cc',
  'TestUtilities.cc',
  'MainTest.cc',
]

if get_option('split-source')
  test_exe = executable(
    'j4-dmenu-tests',
    test_files,
    dependencies: [catch2, spdlog, fmt],
    link_with: source_lib,
    include_directories: inc,
    cpp_args: ['-DTEST_FILES="' + meson.project_source_root() / 'tests/test_files/"', flags],
  )
else
  test_exe = executable(
    'j4-dmenu-tests',
    test_files,
    src,
    dependencies: [catch2, spdlog, fmt],
    include_directories: inc,
    cpp_args: ['-DTEST_FILES="' + meson.project_source_root() / 'tests/test_files/"', flags],
  )
endif

test('j4-dmenu-tests', test_exe)
