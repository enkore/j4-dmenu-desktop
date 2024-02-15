# Determine the notify implementation

comp = meson.get_compiler('cpp')

# Implement semi-automatic detection of kqueue/inotify.
if get_option('notify-implementation') == 'auto'
  if target_machine.system() in ['freebsd', 'dragonfly', 'netbsd', 'openbsd']
    if comp.check_header('sys/event.h')
      inotify = false
    else
      error(
        'Couldn\'t determine notify implementation. Please set it with ',
        '-Dnotify-implementation=<inotify or kqueue> when creating the builddir.',
      )
    endif
  elif target_machine.system() == 'linux'
    # Using kqueue on Linux is very unlikely.
    if comp.check_header('sys/inotify.h')
      inotify = true
    else
      error(
        'Couldn\'t determine notify implementation. Please report this to ',
        'https://github.com/enkore/j4-dmenu-desktop/issues/new',
      )
    endif
  else
    if comp.check_header('sys/inotify.h')
      inotify = true
    elif comp.check_header('sys/event.h')
      inotify = false
    else
      error(
        'Couldn\'t determine notify implementation. Please set it with ',
        '-Dnotify-implementation=<inotify or kqueue> when creating the builddir.',
      )
    endif
  endif
elif get_option('notify-implementation') == 'inotify'
  if target_machine.system() in ['freebsd', 'dragonfly', 'netbsd', 'openbsd']
    warning(
      '#################################################################\n',
      '# WARNING: Meson has detected usage of inotify notify backend   #\n',
      '#          on a BSD. If you\'re using libinotify-kqueue, be      #\n',
      '#          aware that j4-dmenu-desktop isn\'t fully compatible   #\n',
      '#          with it. You should use kqueue if possible.          #\n',
      '#################################################################',
    )
  endif
  inotify = true
elif get_option('notify-implementation') == 'kqueue'
  inotify = false
endif

flags = []

if not inotify
  flags += '-DUSE_KQUEUE'
endif

# Actual build definitions begin here.

loguru = dependency('loguru', static: true)

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
  'ApplicationRunner.cc',
  'Dmenu.cc',
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

# If enabled, src won't have to be compiled twice (once for j4-dmenu-desktop and once for j4-dmenu-tests).
if get_option('split-source')
  source_lib = static_library(
    'source_lib',
    src,
    cpp_args: flags,
    dependencies: loguru,
  )

  executable(
    'j4-dmenu-desktop',
    'main.cc',
    dependencies: loguru,
    cpp_args: flags,
    link_with: source_lib,
    install: true,
  )
else
  executable(
    'j4-dmenu-desktop',
    src,
    'main.cc',
    cpp_args: flags,
    dependencies: loguru,
    install: true,
  )
endif

inc = include_directories('.')
