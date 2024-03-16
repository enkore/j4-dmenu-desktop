# Packaging j4-dmenu-desktop
j4-dmenu-desktop has added Meson as a additional independent build system in
version r3.0. If the package is using the CMake build system, you should
consider switching to Meson as it is the preferred build system (but CMake isn't
going anywhere, it is still supported).

## Bash completions
Bash completions must be generated during build.
[`complgen`](https://github.com/adaszko/complgen) must be installed on host and
Meson must be used (Bash completion generation isn't supported in
j4-dmenu-desktop's CMake).

Complgen status is displayed in `Generation` section of Meson summary:
```
...
Build targets in project: 4

j4-dmenu-desktop 2.18

  Generation
    complgen Bash completion: YES

  Subprojects
    emilk-loguru            : YES
...
```

Alternatively, you can download the completion from a GitHub release. It is
included as a release artifact.

## Dependencies
j4-dmenu-desktop has two dependencies: Catch2 and loguru[^1]. Both of these are
internal dependencies and are linked statically (unless the build system is
overridden).

Catch2 can be provided externally. `WITH_GIT_CATCH` option must be set to `OFF`
when CMake is used. Meson needs no additional setup.

It uses a not released version of loguru, because proper CMake dependency
support for loguru was added after latest release (see
[this](https://github.com/emilk/loguru/pull/215)).

This can be provided externally. `WITH_GIT_LOGURU` option must be set to `OFF`
when CMake is used. Meson should theoretically be able to use system installed
loguru.

## Support
J4-dmenu-desktop has been tested on glibc and musl, cross compilation has been
tested, both `g++` and `clang++` are able to compile j4-dmenu-desktop without
warnings[^2] and errors and everything has been tested on FreeBSD.
J4-dmenu-desktop should be buildable pretty much everywhere. If not, please
[submit a new issue](https://github.com/emilk/loguru/issues/new).

[^1]: Not to be confused with Python logging library [loguru](https://pypi.org/project/loguru/)
[^2]: Loguru has some warnings on FreeBSD. They should be (hopefully) harmless.
