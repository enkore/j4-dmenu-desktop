version_override = get_option('override-version')
if version_override == ''
  # See the contents of meson_git_describe_wrapper.sh for explanation of its use
  version_def_file = vcs_tag(command: 'meson_git_describe_wrapper.sh', input: 'version.cc.in', output: 'version.cc', fallback: meson.project_version())
else
  conf_data = configuration_data()
  conf_data.set('VCS_TAG', version_override)
  version_def_file = configure_file(input: 'version.cc.in', output: 'version.cc', configuration: conf_data)
endif
