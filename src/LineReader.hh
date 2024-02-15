//
// This file is part of j4-dmenu-desktop.
//
// j4-dmenu-desktop is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// j4-dmenu-desktop is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with j4-dmenu-desktop.  If not, see <http://www.gnu.org/licenses/>.
//

// j4-dmenu-desktop needs to use getline() a lot because of it's reliance on C's
// IO. Here it is wrapped in a class to C++ify it a bit and to handle free()
// properly in the dtor.

#ifndef LINEREADER_DEF
#define LINEREADER_DEF

#include <sys/types.h>
#include <stdio.h>

class LineReader
{
public:
    LineReader();

    // These operations are implementable but don't make much sense.
    LineReader(const LineReader &) = delete;
    void operator=(const LineReader &) = delete;

    LineReader(LineReader &&);
    LineReader& operator=(LineReader &&);

    ~LineReader();

    // This calls C's getline() under the hood, it has same error checking
    // procedures (errno).
    ssize_t getline(FILE *f);

    char * get_lineptr();

private:
    char * lineptr = NULL;
    size_t linesz;
};

#endif
