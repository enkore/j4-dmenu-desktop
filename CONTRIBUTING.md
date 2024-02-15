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

J4-dmenu-desktop makes use of `clang-format`.

## Implementation
### IO
Although j4-dmenu-desktop is written in C++, `iostream` isn't used for IO. C's
`stdio` is used instead.

This is done by choice of the creator of j4-dmenu-desktop,
[enkore](https://github.com/enkore). He used it because of performance and
compiler compatibility concerns. None of there concerns are really justified,
j4-dmenu-desktop also isn't a very IO heavy program, so even if there would be a
performance penalty, it wouldn't really matter.

Because of this, j4-dmenu-destkop's IO model might be subject to change in the
future, but `printf()` and other functions should be used for now.

### Logging
Logging is done by [loguru](https://github.com/emilk/loguru). At the time of
writing, j4-dmenu-desktop makes use of four loglevels: `ERROR`, `WARNING`,
`INFO` and `9` (nicknamed `DEBUG` in some places).

```c++
// There are printed by default.
LOG_F(ERROR, "Something really bad happened!");
LOG_F(WARNING, "j4-dmenu-desktop won't terminate because of this, but the "
               "user should be aware of this nonetheless.");
// These have to be enabled with a flag to be shown.
LOG_F(INFO, "Describe the runtime of j4-dmenu-desktop and print some useful "
            "things.");
LOG_F(9, "Print debugging info.");
```

### Commandline arguments
There are six places where arguments have to be specified:
1. in argument handling code of `main()` (done via `getopt`)
2. in `--help`
3. in [`j4-dmenu-destkop.1`](j4-dmenu-destkop.1) manpage
4. in ZSH completion script [`etc/_j4-dmenu-desktop`](etc/_j4-dmenu-desktop)
5. in FISH completion script [`etc/j4-dmenu-desktop.fish`](etc/j4-dmenu-desktop.fish)
6. in complgen completion description [`etc/complgen`](etc/complgen)

When modifying or adding arguments, all six of these have to be updated. This
system isn't ideal and might be subject to change in the future.
