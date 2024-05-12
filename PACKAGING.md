# Packaging j4-dmenu-desktop
j4-dmenu-desktop has added Meson as a additional independent build system in
version r3.0. If your package is using the CMake build system, you should
consider switching to Meson as it is the preferred build system (but CMake isn't
going anywhere, it is still supported).

## Dependencies
j4-dmenu-desktop has three dependencies:
[Catch2](https://github.com/catchorg/Catch2),
[spdlog](https://github.com/gabime/spdlog) and
[fmt](https://github.com/fmtlib/fmt). fmt is both a dependency of spdlog and of
j4-dmenu-desktop. All of these are internal dependencies and are linked
statically (unless the build system is overridden or dependencies are provided
externally).

All of these dependencies can be provided externally. This requires setting the
following flags in CMake:

| dependency | CMake flag              |
| ---------- | ----------------------- |
| Catch2     | `-DWITH_GIT_CATCH=OFF`  |
| spdlog     | `-DWITH_GIT_SPDLOG=OFF` |
| fmt        | `-DWITH_GIT_FMT=OFF`    |

Meson prefers external dependencies by default. J4-dmenu-desktop uses Meson's
Wrap dependency system, which is fully configurable using standard Meson flags.

Unit tests may be disabled using the `-DWITH_TESTS=OFF` CMake flag or with the
`-Denable-tests=false` Meson option. Disabling unit tests removes the Catch2
dependency from j4-dmenu-desktop.

## Meson setup
The recommended way of setting up a Meson build directory is using the bundled
[`meson-setup.sh`](https://github.com/enkore/j4-dmenu-desktop/blob/develop/meson-setup.sh)
script. If you must run `meson` manually (to for example let your packaging
system control Meson), you can run it with the `-d` flag to see what flags it
sets:

```
./meson-setup.sh -d
```

The flags look like this as of 12-05-2024:

```
meson setup --buildtype=release -Db_lto=true --unity on --unity-size 9999 builddir
```

As you can see, unity builds are used by default.

You may ignore the recommended flags and use your own.

## Support
J4-dmenu-desktop has been tested on glibc and musl, cross compilation has been
tested, both `g++` and `clang++` are able to compile j4-dmenu-desktop without
warnings and errors and everything has been tested on FreeBSD.
J4-dmenu-desktop should be buildable pretty much everywhere. If not, please
[submit a new issue](https://github.com/enkore/j4-dmenu-desktop/issues/new).
