tests_config_data = configuration_data()
tests_config_data.set_quoted(
  'TEST_FILES',
  meson.project_source_root() / 'tests/test_files/',
)
configure_file(
  input: 'tests_config.hh.in',
  output: 'tests_config.hh',
  configuration: tests_config_data,
  format: 'cmake@'
)
