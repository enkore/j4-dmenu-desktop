# This build file depends on j4dd_exe defined in src/meson.build

pytest_exe = find_program(
  ['pytest', 'py.test3', 'py.test-3'],
  native: true,
  required: false,
)

if pytest_exe.found()
  test(
    'pytest-system-tests',
    pytest_exe,
    args: [files('system_test.py'), '--j4dd-executable', j4dd_exe, '--verbose'],
    depends: j4dd_exe,
    timeout: 45,
  )
else
  warning('Couldn\'t find \'pytest\' executable, pytest tests will be skipped.')
  test('pytest-system-tests', files('skipped_test.sh'), args: ['Meson'])
endif
