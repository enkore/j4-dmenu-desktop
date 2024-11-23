# Contributing to j4-dmenu-destkop
Thank you for considering contributing to j4-dmenu-desktop!

Here are some tips for contributing:

## General info
### Styleguide
Type names (class names, struct names, using & typedef declarations, enums) use
`PascalCase`. Macros use `CAPS_LOCK`. Pretty much everything else (function
names, variables etc.) use `snake_case`.

`public` comes before `private` in classes.

All members of class must be accessed with `this->member` inside member
functions. This helps disambiguate which variables are local variables and which
are member variables.

Header file includes come in this order:

1. "" include to appropriate header if it's in an implementation file (`#include
   "Dmenu.hh"` in `Dmenu.cc`)
2. <> includes to external dependencies (Catch2, spdlog...)
3. <> system includes
4. "" local includes

They should ideally be separated by a blank line.

J4-dmenu-desktop makes use of `clang-format`.

## Implementation
### IO
[{fmt}](https://fmt.dev/latest/index.html) is used where possible. If that isn't
possible, C style IO is used. C++ style IO (iostream) isn't used in j4-dmenu-desktop.

### Logging
Logging is done by [spdlog](https://github.com/gabime/spdlog). At the time of
writing, j4-dmenu-desktop makes use of four loglevels: `ERROR`, `WARNING` (called `WARN` in some places),
`INFO` and `DEBUG`.

```c++
// There are printed by default.
SPDLOG_ERROR("Something really bad happened!");
SPDLOG_WARN("j4-dmenu-desktop won't terminate because of this, but the "
            "user should be aware of this nonetheless.");
// These have to be enabled with a flag to be shown.
SPDLOG_INFO("Describe the runtime of j4-dmenu-desktop and print some useful "
            "things.");
SPDLOG_DEBUG("Print debugging info.");
```

### Commandline arguments
There are four places where arguments have to be specified:
1. in argument handling code of `main()` (done via `getopt`)
2. in `--help`
3. in [`j4-dmenu-destkop.1`](j4-dmenu-destkop.1) manpage
4. in crazy-complete shell completion definition file [`etc/j4-dmenu-desktop.yaml`](etc/j4-dmenu-desktop.yaml)

When modifying or adding arguments, all four of these have to be updated.
