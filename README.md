# j4-dmenu-desktop

j4-dmenu-desktop is a fast desktop menu using `dmenu` (or compatible programs).
It scans .desktop files in $XDG_DATA_HOME and $XDG_DATA_DIRS, providing a menu
for launching applications.

It is inspired by i3-dmenu-desktop. Like i3-dmenu-desktop, j4-dmenu-desktop
offers optional integration with i3wm and Sway (but j4-dmenu-desktop should work
just fine on about any desktop environment).

You can also execute shell commands using it.

https://github.com/user-attachments/assets/311779a5-4ebf-41db-b942-cb90747649f9

## Features

- speed
- conformance to the [Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/1.5/)[^1]
- daemon mode with `--wait-on` which parses desktop files ahead of time
- support for history sorted by usage frequency using `--usage-log`
- automatic desktop file loading/removal in daemon mode using inotify/kqueue
- support for any dmenu-like program (j4-dmenu-desktop is independent of any
  desktop environment + it works with both Xorg and [Wayland](#examples))
- (optional) i3/Sway IPC integration
- multiple formatters available (program name, program name with executable
  path...)
- completions for all major shells + a manpage
- and more!

## Build requirements

* Compiler with C++17 support
* CMake or Meson

Building with Meson:

    ./meson-setup.sh build
    cd build
    meson compile
    sudo meson install

Building with CMake:

    mkdir build
    cd build
    cmake ..
    make j4-dmenu-desktop
    sudo make install

See [BUILDING](BUILDING.md) for more info.

## Distribution packages

### Archlinux <a href="https://repology.org/project/j4-dmenu-desktop/versions"><img src="https://repology.org/badge/vertical-allrepos/j4-dmenu-desktop.svg" alt="Packaging status" align="right"></a>

The package is provided in the Arch Linux extra repository. You can install it via

    sudo pacman -S j4-dmenu-desktop

### FreeBSD

j4-dmenu-desktop is now available in the FreeBSD Ports Collection. A prebuilt package can be installed via

    pkg install j4-dmenu-desktop

### Gentoo

j4-dmenu-desktop is available in Portage for the `amd64` and `x86` architectures. You can install it via

    echo "x11-misc/j4-dmenu-desktop ~amd64 ~x86" >> /etc/portage/package.accept_keywords
    emerge --ask x11-misc/j4-dmenu-desktop

The package is also provided by the `gentoo-el` overlay. You can install it with the following commands as root. (you need to have `layman` installed and configured)

    layman -a gentoo-el
    echo "=x11-misc/j4-dmenu-desktop-9999 **" >> /etc/portage/package.accept_keywords
    emerge x11-misc/j4-dmenu-desktop

### Ubuntu

The package is now in the apt repository. You can install it via

    sudo apt-get install j4-dmenu-desktop

### Debian

j4-dmenu-desktop is in Debian stable:

    sudo apt install j4-dmenu-desktop

### Nix / NixOS

j4-dmenu-desktop is in [nixpkgs](https://github.com/NixOS/nixpkgs/blob/master/pkgs/applications/misc/j4-dmenu-desktop/default.nix):

    nix-env --install j4-dmenu-desktop
    # Or use pkgs attribute of the same name in NixOS configuration

### Void Linux

j4-dmenu-desktop is packaged in Void Linux:

    sudo xbps-install -S j4-dmenu-desktop

This package is maintained my me, [meator](https://github.com/meator). As a
collaborator of j4-dmenu-desktop and an active Void Linux user, I ensure a
higher standard of quality for this package. This package is officially
supported, feel free to [open
issues](https://github.com/enkore/j4-dmenu-desktop/issues/new) or ask for
support related to the Void Linux package here.

## Examples

Run j4-dmenu-desktop:

    j4-dmenu-desktop

Specify custom dmenu (useful on Wayland, [tofi](https://github.com/philj56/tofi)
is used as example):

    j4-dmenu-desktop --dmenu=tofi

Display 5 entries vertically at once:

    j4-dmenu-desktop --dmenu="dmenu -l5"

display binary name alongside name (for example `Chromium` will become `Chromium
(/usr/bin/chromium)`):

    j4-dmenu-desktop --display-binary

Don't display generic names and use `alacritty` as terminal emulator:

    j4-dmenu-desktop --no-generic --term alacritty

Enable I3/Sway IPC mode:

    j4-dmenu-desktop --i3-ipc

## FAQ / RAQ / RMR

### I want it to display normal binaries, too, yes?

You can put this in a script file and use it instead of calling j4dd directly:

    j4-dmenu-desktop --dmenu="(cat ; (stest -flx $(echo $PATH | tr : ' ') | sort -u)) | dmenu"

Exchanging the `cat` and `(stest ... sort -u)` parts will swap the two parts (j4dd's output and the list of binaries).

### Get the output into a pipe / launching a program by arbitrary user input

[GOTO](https://github.com/enkore/j4-dmenu-desktop/issues/39#issuecomment-177164865)

### How much faster is it?

    % time i3-dmenu-desktop --dmenu="cat"
    [{"success":true}]
    i3-dmenu-desktop --dmenu="cat"  0.37s user 0.02s system 96% cpu 0.404 total
    % time ./j4-dmenu-desktop --dmenu=cat
    ./j4-dmenu-desktop --dmenu=cat  0.01s user 0.01s system 107% cpu 0.015 total

More than 25 times faster :)

[^1]: Some minor features like [handling of `TryExec`
      key](https://github.com/enkore/j4-dmenu-desktop/issues/165) are missing.
      If there are any problems with compliance to the Desktop entry
      specification, please [submit an
      issue](https://github.com/enkore/j4-dmenu-desktop/issues/new).
