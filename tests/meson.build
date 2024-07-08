# This build file depends on several variables defined in src/meson.build

if not get_option('enable-tests')
  subdir_done()
endif

# The oldest version of Catch2 I was able to verify is v3.3.0. v2 versions
# won't likely work. version isn't set to >3.3.0 because 3.3.0 is the oldest
# Catch2 release I was able to compile (compilation errors are likely caused
# by newer version of gcc which wasn't used at that time).
catch2 = dependency('catch2', version: '>3.0.0', default_options: 'tests=false')

test_files = [
  'FSUtils.cc',
  'ShellUnquote.cc',
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
  'TestCMDLineTerm.cc',
  'TestUtilities.cc',
  'TestCMDLineAssembler.cc',
  'MainTest.cc',
]

test_files_location = meson.project_source_root() / 'tests/test_files/"'

if get_option('split-source')
  test_exe = executable(
    'j4-dmenu-tests',
    test_files,
    dependencies: [catch2, spdlog, fmt],
    link_with: source_lib,
    include_directories: inc,
    cpp_args: ['-DTEST_FILES="' + test_files_location, flags],
  )
else
  test_exe = executable(
    'j4-dmenu-tests',
    test_files,
    src,
    dependencies: [catch2, spdlog, fmt],
    include_directories: inc,
    cpp_args: ['-DTEST_FILES="' + test_files_location, flags],
  )
endif

# NotifyKqueue checks for changes every minute. This means that the default timeout
# of 30 seconds is not sufficient on BSDs, because one of the tests uses Notify.
test('j4-dmenu-tests', test_exe, timeout: 90)

bats_exe = find_program(
  'bats',
  native: true,
  required: false,
  version: '>=1.2.1',
)

if bats_exe.found()
  show_passing_tests = []

  # Be verbose if Bats allows it
  if bats_exe.version().version_compare('>=1.5.0')
    show_passing_tests = '--show-output-of-passing-tests'
  endif

  test(
    'bats-tests',
    files('bats_runner.sh'),
    args: [
      '-b',
      bats_exe.full_path(),
      j4dd_exe,
      '--pretty',
      show_passing_tests,
      files('bats.bats'),
    ],
    depends: j4dd_exe,
  )
else
  warning('Couldn\'t find bats executable, bats tests will be skipped.')
  test('bats-tests', files('skipped_test.sh'))
endif
