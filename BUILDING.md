# Building j4-dmenu-desktop

Since version r3.0, j4-dmenu-desktop provides two independent build systems:
CMake and Meson. CMake is the original build system, Meson has been added in
version r3.0.

Both build systems are configured with **unity builds** by default (see
[unity builds in Meson](#unity-builds-in-meson) for more info about Meson). This
is done to speed up release builds, but developers may wish to turn this off.

Although both systems will be supported for compatibility, **Meson should be
preferred**. Meson provides overall better development and packaging experience.

> [!WARNING]
> Meson build system requires Meson >=1.1.0 (as marked in the `meson_version`
field in `meson.build`). If that isn't available in your build environment,
CMake can be used instead.

## Dependencies
j4-dmenu-desktop has three dependencies:
[Catch2](https://github.com/catchorg/Catch2),
[spdlog](https://github.com/gabime/spdlog) and
[fmt](https://github.com/fmtlib/fmt). fmt is both a dependency of spdlog and of
j4-dmenu-desktop. All of these are internal dependencies and are linked
statically (unless the build system is overridden or dependencies are provided
externally).

J4-dmenu-desktop also depends on
[pytest](https://docs.pytest.org/en/stable/). It is an **optional
external** dependency. It is used for system testing to supplement Catch2
tests. If pytest is not available, pytest tests will be marked as skipped.

The pytest testsuite includes tests for various supported terminal emulators.
See
[tests/system_tests/README.md](tests/system_tests/README.md#terminal-emulators)
for more info.

J4-dmenu-desktop won't depend on Catch2 or pytest if tests are disabled.

J4-dmenu-desktop also depends on
[crazy-complete](https://github.com/crazy-complete/crazy-complete). It is an
**optional external** dependency. This is true for Meson only,
j4-dmenu-desktop's CMake build system currently doesn't support generating
completions. See [`etc/README.md`](etc/) for more info.

## Building with CMake
Out-of-source builds are fully supported.

CMake will automatically download Catch2, spdlog and fmt dependencies. If you
want to use system installed dependencies instead, can override the mechanism
using the following flags:

| dependency | CMake flag              |
| ---------- | ----------------------- |
| Catch2     | `-DWITH_GIT_CATCH=OFF`  |
| spdlog     | `-DWITH_GIT_SPDLOG=OFF` |
| fmt        | `-DWITH_GIT_FMT=OFF`    |

If you want to disable dependency fetching altogether, you can use
`-DNO_DOWNLOAD=ON`. This is more future-proof, because it prevents the download
of potential new dependencies added to j4-dmenu-desktop.

Tests can be disabled using the `-DWITH_TESTS=OFF` flag.

## Building with Meson
J4-dmenu-desktop provides a simple setup helper
[`meson-setup.sh`](meson-setup.sh). It calls `meson setup` with official [native
files](https://mesonbuild.com/Native-environments.html) present in
`meson-native/` directory. You can run this script with the `-d` flag to do a
dry run, which just prints the commands it would have executed to stdout.

You can also directly source the native files with `meson setup --native-file
meson-native/...`, bypassing [`meson-setup.sh`](meson-setup.sh). This allows you
to further override the options.

Meson, like CMake, provides several options that tweak the build. In particular,
developers may want to set `split-source` to reduce unnecessary recompilation
when building `j4-dmenu-desktop` and `j4-dmenu-tests` at the same time[^1]
(`meson-setup.sh` sets this in its debugging build styles).

Tests can be disabled using the `-Denable-tests=false` flag.

Although it is recommended to use `meson-setup.sh`, you can completely ignore
its flags and invoke meson manually.

### Unity builds in Meson
Unity builds are fully configurable in Meson. They are enabled by default.
The `meson-setup.sh` script disables unity builds for `debug` and `sanitize`
build styles. You can use Meson's `--unity=off` flag to turn off unity builds
when configuring the project manually.

## Support
J4-dmenu-desktop has been tested on glibc and musl, cross compilation has been
tested, both `g++` and `clang++` are able to compile j4-dmenu-desktop without
warnings and errors and everything has been tested on FreeBSD.
J4-dmenu-desktop should be buildable pretty much everywhere. If not, please
[submit a new issue](https://github.com/enkore/j4-dmenu-desktop/issues/new).

[^1]: `ccache` can be used instead
