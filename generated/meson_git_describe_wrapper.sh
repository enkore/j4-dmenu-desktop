#!/bin/sh
# The following command would normally print an error message if not run in a
# git repository (e.g. when run in an extracted release archive, which doesn't
# include git history).
# J4dd's build system is configured to handle git failures well. If git isn't
# available or if we're not in a git repository, version.txt is used to
# determine the version.
# Because this behavior is fully supported, printing an error message would only
# confuse the user, because the error message implies that something is wrong.
exec git describe --dirty --broken 2> /dev/null
