# j4-dmenu-desktop

j4-dmenu-desktop is a replacement for i3-dmenu-desktop. It's purpose
is to find .desktop files and offer you a menu to start an application
using dmenu. Since r2.7 j4-dmenu-desktop doesn't require i3wm anymore
and should work just fine on about any desktop environment.

You can also execute shell commands using it.

## Build requirements

* Compiler with C++17 support
* CMake or Meson

Building with Meson:

    ./meson-setup.sh build
    cd build
    ninja src/j4-dmenu-desktop
    sudo meson install

Building with CMake:

    mkdir build
    cd build
    cmake ..
    make
    sudo make install

## Distribution packages

### Archlinux <a href="https://repology.org/project/j4-dmenu-desktop/versions"><img src="https://repology.org/badge/vertical-allrepos/j4-dmenu-desktop.svg" alt="Packaging status" align="right"></a>


The package is provided by the AUR. You can install it with an AUR helper of your choice: `j4-dmenu-desktop-git` or `j4-dmenu-desktop`. Else, you may install it manually by invoking the following commands as a regular user. (to build packages from the AUR, the `base-devel` package group is assumed to be installed)

    git clone https://aur.archlinux.org/j4-dmenu-desktop.git
    cd j4-dmenu-desktop
    makepkg -si

or for the latest:

    git clone https://aur.archlinux.org/j4-dmenu-desktop-git.git
    cd j4-dmenu-desktop-git
    makepkg -si

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
