install_data('_j4-dmenu-desktop', install_dir: get_option('datadir') / 'zsh/site-functions')
install_data(
    'j4-dmenu-desktop.fish',
    install_dir: get_option('datadir') / 'fish/vendor_completions.d',
)

complgen_exe = find_program(
    'complgen',
    native: true,
    disabler: true,
    required: false
)

custom_target(
    'generate_bash_compl',
    command: [complgen_exe, 'aot', '--bash-script', '@OUTPUT@', '@INPUT@'],
    input: 'complgen',
    output: 'j4-dmenu-desktop',
    install: true,
    install_dir: get_option('datadir') / 'bash-completion/completions',
)
