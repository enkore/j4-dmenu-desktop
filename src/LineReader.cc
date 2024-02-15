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

#include <stdlib.h>
#include <stdio.h>

#include "LineReader.hh"

LineReader::LineReader() {}

LineReader::LineReader(LineReader &&other)
    : lineptr(other.lineptr), linesz(other.linesz) {
    other.lineptr = NULL;
}

LineReader &LineReader::operator=(LineReader &&other) {
    if (&other == this)
        return *this;
    this->lineptr = other.lineptr;
    this->linesz = other.linesz;

    other.lineptr = NULL;
    return *this;
}

LineReader::~LineReader() {
    // lineptr is either valid or NULL; free() can be used in both states.
    free(this->lineptr);
}

ssize_t LineReader::getline(FILE *f) {
    return ::getline(&this->lineptr, &this->linesz, f);
}

char *LineReader::get_lineptr() {
    return this->lineptr;
}
