version_override = get_option('override-version')
if version_override == ''
  # See the contents of meson_git_describe_wrapper.sh for explanation of its use
  version_header = vcs_tag(command: 'meson_git_describe_wrapper.sh', input: 'version.hh.in', output: 'version.hh', fallback: meson.project_version())
else
  conf_data = configuration_data()
  conf_data.set('VCS_TAG', version_override)
  version_header = configure_file(input: 'version.hh.in', output: 'version.hh', configuration: conf_data)
endif

version_header_inc = include_directories('.')
