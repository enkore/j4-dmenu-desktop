project(
  'j4-dmenu-desktop',
  'cpp',
  version: files('version.txt'),
  default_options: [
    'warning_level=3',
    'cpp_std=c++17',
    'unity=on',
    'unity_size=9999',
  ],
  license: 'GPL-3.0-or-later',
  license_files: 'LICENSE',
  meson_version: '>=1.1.0',
)

install_man('j4-dmenu-desktop.1')

subdir('generated')
subdir('src')
subdir('tests')
subdir('etc')
