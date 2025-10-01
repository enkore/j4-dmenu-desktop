# This build file depends on several variables defined in src/meson.build

if not get_option('enable-tests')
  summary(
    'unit tests',
    false,
    section: 'Optional build components',
    bool_yn: true,
  )
  subdir_done()
endif

summary('unit tests', true, section: 'Optional build components', bool_yn: true)

# The oldest version of Catch2 I was able to verify is v3.3.0. v2 versions
# won't likely work. version isn't set to >3.3.0 because 3.3.0 is the oldest
# Catch2 release I was able to compile (compilation errors are likely caused
# by newer version of gcc which wasn't used at that time).
catch2 = dependency(
  'catch2',
  version: '>3.0.0',
  default_options: [
    'tests=false',
    'install=false',
  ],
)

test_files = [
  'FSUtils.cc',
  'ShellUnquote.cc',
  'TestAppManager.cc',
  'TestApplication.cc',
  'TestHistoryManager.cc',
  'TestDynamicCompare.cc',
  'TestFieldCodes.cc',
  'TestFileFinder.cc',
  'TestFormatters.cc',
  'TestLocaleSuffixes.cc',
  'TestNotify.cc',
  'TestSearchPath.cc',
  'TestI3Exec.cc',
  'TestCMDLineTerm.cc',
  'TestUtilities.cc',
  'TestCMDLineAssembler.cc',
  'MainTest.cc',
]

subdir('generated')

test_exe = executable(
  'j4-dmenu-tests',
  test_files,
  dependencies: [catch2, spdlog, fmt, source_dep],
  cpp_args: flags,
)

# NotifyKqueue checks for changes every minute. This means that the default timeout
# of 30 seconds is not sufficient on BSDs, because one of the tests uses Notify.
test('j4-dmenu-tests', test_exe, timeout: 90)

subdir('system_tests')
