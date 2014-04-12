# j4-dmenu-desktop

[![Travis Build](https://api.travis-ci.org/enkore/j4-dmenu-desktop.png)](https://travis-ci.org/enkore/j4-dmenu-desktop/)

j4-dmenu-desktop is a replacement for i3-dmenu-desktop. It's purpose
is to find .desktop files and offer you a menu to start an application
using dmenu. Since r2.7 j4-dmenu-desktop doesn't require i3wm anymore
and should work just fine on about any desktop environment.

You can also execute shell commands using it.

## Build requirements

* Compiler with basic C++11 support (GCC 4.77 or later required, Clang works, too)
* CMake

Building is the usual cmake/make thingy:

    cmake .
    make
    sudo make install

## Distribution packages

### Archlinux

The package is provided by the AUR. You can install it with an AUR helper of your choice: `j4-dmenu-desktop-git` or manually by invoking the following commands as a regular user. (to build packages from the AUR, the `base-devel` package group is assumed to be installed)

    wget https://aur.archlinux.org/packages/j4/j4-dmenu-desktop-git/j4-dmenu-desktop-git.tar.gz
    tar xf j4-dmenu-desktop-git.tar.gz
    cd j4-dmenu-desktop-git
    makepkg -si

### Gentoo

The package is provided by the `gentoo-el` overlay. You can install it with the following commands as root. (you need to have `layman` installed and configured)

    layman -a gentoo-el
    echo "=x11-misc/j4-dmenu-desktop-9999 **" >> /etc/portage/package.accept_keywords
    emerge x11-misc/j4-dmenu-desktop

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

