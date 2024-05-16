# Building j4-dmenu-desktop

> [!NOTE]
> Trying to package j4-dmenu-desktop? Take a look at [PACKAGING](PACKAGING.md).

Since version r3.0, j4-dmenu-desktop provides two independent build systems:
CMake and Meson. CMake is the original build system, Meson has been added in
version r3.0.

Both build systems are configured with **unity builds** by default (see
[unity builds in Meson](#unity-builds-in-meson) for more info about Meson). This
is done to speed up release builds, but developers may wish to turn this off.

Although both systems will be supported for compatibility, **Meson should be
preferred**. Meson provides overall better development and packaging experience.

## Building with CMake
Out-of-source builds are fully supported.

CMake will automatically download Catch2, spdlog and fmt dependencies. You can
override Catch2 behaviour with `WITH_TESTS` to turn off tests and
`WITH_GIT_CATCH`, `WITH_GIT_SPDLOG` and `WITH_GIT_FMT` to manage fetching of
dependencies. This is useful if you want to use system installed dependencies.

## Building with Meson
J4-dmenu-desktop provides a simple setup helper
[`meson-setup.sh`](meson-setup.sh). It calls `meson setup` with some preset
flags. You can run this script with the `-d` flag to do a dry run, which just
prints the commands it would have executed to stdout. You can then tweak it to
your needs using standard Meson flags.

Meson, like CMake, provide several options that tweak the build. In particular,
developers may want to set `split-source` to reduce unnecessary recompilation
when building `j4-dmenu-desktop` and `j4-dmenu-tests` at the same time[^1]
(`meson-setup.sh` sets this in its debugging build styles).

### Unity builds in Meson
Unity builds can't be set by default without disrupting developer workflow. The
`meson-setup.sh` script uses unity builds for release build style (which is the
default). This is also the recommended setup in
[`README.md`](README.md#build-requirements).

[^1]: `ccache` can be used instead
