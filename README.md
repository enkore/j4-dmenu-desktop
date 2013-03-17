
# j4-dmenu

j4-dmenu is a replacement for i3-dmenu-desktop. It's purpose is to find .desktop files
and offer you a menu to start an application using dmenu.

## Build requirements

* Compiler with basic C++11 support
* CMake

    cmake .
    make
    sudo make install

## Invocation

If you specifify arguments to j4-dmenu they will be concatenated and used as the
command to start dmenu.

    j4-dmenu

would use `dmenu` executed with your standard shell (`$SHELL` if set, `/bin/sh` otherwise)
to start dmenu.

    j4-dmenu "dmenu -fn 'DejaVu Sans-10' -l 20"

would start dmenu with the -fn and -l arguments to create a vertical menu.
