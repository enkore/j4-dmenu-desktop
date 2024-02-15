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

#ifndef HISTORY_DEF
#define HISTORY_DEF

#include "AppManager.hh"
#include "Application.hh"

using std::string;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define J4DDHIST_MAJOR_VERSION 1
#define J4DDHIST_MINOR_VERSION 0
#define J4DDHIST_VERSION                                                       \
    TOSTRING(J4DDHIST_MAJOR_VERSION) "." TOSTRING(J4DDHIST_MINOR_VERSION)
#define J4DDHIST_HEADER "j4dd history v"
#define J4DDHIST_HEADER_LENGTH 14

class v0_version_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// This class employs a version management mechanism. This should simplify
// changing the history format in the future. As of the time of writing, only
// two formats of history file exist, and one one of them employs the versioning
// mechanism. The "v0" version which doesn't have the version header has to be
// handled spacially. If new version of history file should be made, checking
// the versions will involve only comparing the version in the header to the
// current one.

// We need to do these things with the history:
// 1) load it (if it exists)
// 2) increase history count of a single element (when it's selected)
// 3) list the history in order
// 4) save history
class HistoryManager
{
public:
    using history_mmap_type = std::multimap<int, string, std::greater<int>>;
    HistoryManager(const string &path);
    HistoryManager(HistoryManager &&other);
    HistoryManager &operator=(HistoryManager &&other);

    HistoryManager(const HistoryManager &) = delete;
    void operator=(const HistoryManager &) = delete;

    void increment(const string &name);
    void remove_obsolete_entry(history_mmap_type::const_iterator iter);
    const history_mmap_type &view() const;
    static HistoryManager convert_history_from_v0(const string &path,
                                                  const AppManager &appm);

private:
    HistoryManager(FILE *f, std::multimap<int, string, std::greater<int>> hist);

    // This function tests whether file is the "v0.0" version of the history
    // file. This version doesn't contain the header.
    bool is_v0(LineReader &);

    // This is called in the ctor. It is expected that file is open and the
    // header has already been read.
    void read_file(const string &name, LineReader &liner);

    void write();

    std::unique_ptr<FILE, fclose_deleter> file;
    history_mmap_type history;
};

static_assert(std::is_move_constructible_v<HistoryManager>);

#endif
