# j4-dmenu-desktop

j4-dmenu-desktop is a replacement for i3-dmenu-desktop. It's purpose
is to find .desktop files and offer you a menu to start an application
using dmenu. Since r2.7 j4-dmenu-desktop doesn't require i3wm anymore
and should work just fine on about any desktop environment.

## Build requirements

* Compiler with basic C++11 support (GCC 4.6 is probably just fine)
* CMake

Building is the usual cmake/make thingy:

    cmake .
    make
    sudo make install

## Invocation

Usage:

    j4-dmenu-desktop [--dmenu="dmenu -i"] [--term="i3-sensible-terminal"]
    j4-dmenu-desktop --help

Options:

    --dmenu=<command>
        Determines the command used to invoke dmenu
        Executed with your shell ($SHELL) or /bin/sh
    --display-binary
        Display binary name after each entry (off by default)
    --term=<command>
        Sets the terminal emulator used to start terminal apps
    --help
        Display this help message


## FAQ

### Case insensitivity?

Add the `-i` option to the dmenu command

### How much faster is it?

    % time i3-dmenu-desktop --dmenu="cat"
    [{"success":true}]
    i3-dmenu-desktop --dmenu="cat"  0.37s user 0.02s system 96% cpu 0.404 total
    % time ./j4-dmenu-desktop --dmenu=cat
    ./j4-dmenu-desktop --dmenu=cat  0.01s user 0.01s system 107% cpu 0.015 total


More than 25 times faster :)

