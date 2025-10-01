# Determine the notify implementation

comp = meson.get_compiler('cpp')

# Implement semi-automatic detection of kqueue/inotify.
if get_option('notify-implementation') == 'auto'
  if target_machine.system() in ['freebsd', 'dragonfly', 'netbsd', 'openbsd']
    if comp.check_header('sys/event.h', prefix: '#include <sys/types.h>')
      inotify = false
    else
      error(
        'Couldn\'t determine notify implementation. Please set it with',
        '-Dnotify-implementation=<inotify or kqueue> when creating the builddir.',
      )
    endif
  elif target_machine.system() == 'linux'
    # Using kqueue on Linux is very unlikely.
    if comp.check_header('sys/inotify.h')
      inotify = true
    else
      error(
        'Couldn\'t determine notify implementation. Please report this to',
        'https://github.com/enkore/j4-dmenu-desktop/issues/new',
      )
    endif
  else
    if comp.check_header('sys/inotify.h')
      inotify = true
    elif comp.check_header('sys/event.h', prefix: '#include <sys/types.h>')
      inotify = false
    else
      error(
        'Couldn\'t determine notify implementation. Please set it with',
        '-Dnotify-implementation=<inotify or kqueue> when creating the builddir.',
      )
    endif
  endif
elif get_option('notify-implementation') == 'inotify'
  if target_machine.system() in ['freebsd', 'dragonfly', 'netbsd', 'openbsd']
    warning('''
  #################################################################
  # WARNING: Meson has detected usage of inotify notify backend   #
  #          on a BSD. If you're using libinotify-kqueue, be      #
  #          aware that j4-dmenu-desktop isn't fully compatible   #
  #          with it. You should use kqueue if possible.          #
  #################################################################
'''.substring(0, -1) # Strip the last \n
    )
  endif
  inotify = true
elif get_option('notify-implementation') == 'kqueue'
  inotify = false
endif

flags = ['-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG']

if not inotify
  flags += '-DUSE_KQUEUE'
endif

# Actual build definitions begin here.

fmt = dependency('fmt', default_options: ['default_library=static'])
spdlog = dependency(
  'spdlog',
  default_options: [
    'tests=disabled',
    'compile_library=true',
    'default_library=static',
  ],
)

if get_option('set-debug') == 'auto'
  if get_option('debug')
    flags += '-DDEBUG'
  endif
else
  if get_option('set-debug') == 'on'
    flags += '-DDEBUG'
  endif
endif

src = files(
  'AppManager.cc',
  'Application.cc',
  'CMDLineAssembler.cc',
  'CMDLineTerm.cc',
  'Dmenu.cc',
  'FieldCodes.cc',
  'FileFinder.cc',
  'Formatters.cc',
  'HistoryManager.cc',
  'I3Exec.cc',
  'LineReader.cc',
  'LocaleSuffixes.cc',
  'SearchPath.cc',
  'Utilities.cc',
)

if inotify
  src += files('NotifyInotify.cc')
else
  src += files('NotifyKqueue.cc')
endif

if get_option('dev-fix-coverage') == true
  main_flags = '-DFIX_COVERAGE'
else
  main_flags = []
endif

# If enabled, src won't have to be compiled twice (once for j4-dmenu-desktop and once for j4-dmenu-tests).
if get_option('split-source')
  source_lib = static_library(
    'source_lib',
    src,
    cpp_args: flags,
    dependencies: [spdlog, fmt],
  )

  source_dep = declare_dependency(
    dependencies: [spdlog, fmt],
    include_directories: include_directories('.'),
    link_with: source_lib,
  )
else
  source_dep = declare_dependency(
    dependencies: [spdlog, fmt],
    include_directories: include_directories('.'),
    sources: src,
  )
endif

j4dd_exe = executable(
  'j4-dmenu-desktop',
  'main.cc',
  version_def_file,
  cpp_args: [flags, main_flags],
  dependencies: [spdlog, fmt, source_dep],
  install: true,
)
