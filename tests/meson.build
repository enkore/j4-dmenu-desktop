# This build file depends on several variables defined in src/meson.build
if get_variable('cpp_debugstl', false)
  # System library can't be used because of
  # https://github.com/catchorg/Catch2/issues/2811
  warning('System installed Catch2 can\'t be used!')
  catch2_sub = subproject('catch2', default_options: 'tests=false')
  catch2 = catch2_sub.get_variable('catch2_dep')
else
  catch2 = dependency('catch2', default_options: 'tests=false')
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
    dependencies: [catch2, loguru],
    link_with: source_lib,
    include_directories: inc,
    cpp_args: ['-DTEST_FILES="' + meson.project_source_root() / 'tests/test_files/"', flags],
  )
else
  test_exe = executable(
    'j4-dmenu-tests',
    test_files,
    src,
    dependencies: [catch2, loguru],
    include_directories: inc,
    cpp_args: ['-DTEST_FILES="' + meson.project_source_root() / 'tests/test_files/"', flags],
  )
endif

test('j4-dmenu-tests', test_exe)
