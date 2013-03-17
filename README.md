# j4-dmenu

j4-dmenu is a replacement for i3-dmenu-desktop. It's purpose is to find .desktop files
and offer you a menu to start an application using dmenu.

## Build requirements

* Compiler with basic C++11 support (GCC 4.6 is probably just fine)
* CMake

Building is the usual cmake/make thingy:

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

## Perfomance

    % time i3-dmenu-desktop --dmenu="cat"
    [{"success":true}]
    i3-dmenu-desktop --dmenu="cat"  0.37s user 0.02s system 96% cpu 0.404 total
    % time j4-dmenu cat
    Command line: exec "/usr/bin/0ad"
    [{"success":true}]
    j4-dmenu cat  0.06s user 0.00s system 77% cpu 0.078 total

About six times faster :) I guess it could be even faster if I would've avoided
using `std::string` all over.

But I achieved my target anyway; with i3-dmenu-desktop I always had a very noticable
latency between hitting my keyboard shortcut and the menu popping up which is
gone with j4-dmenu...
