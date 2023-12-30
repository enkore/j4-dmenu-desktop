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

constexpr int compare_versions(unsigned int major, unsigned int minor) {
    auto major_diff =
        (major > J4DDHIST_MAJOR_VERSION) - (J4DDHIST_MAJOR_VERSION > major);
    if (major_diff == 0) {
        // Compiler complains that (J4DDHIST_MINOR_VERSION > minor) is always
        // false because J4DDHIST_MINOR_VERSION is 0, but the macro might have a
        // different value in the future.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
        return (minor > J4DDHIST_MINOR_VERSION) -
               (J4DDHIST_MINOR_VERSION > minor);
#pragma GCC diagnostic pop
    } else
        return major_diff;
}

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
    HistoryManager(const string &path)
        : file(std::fopen(path.c_str(), "r+"), std::fclose) {
        // We first try to open the file in the initializer list. If that
        // doesn't work, we go for fallback.
        if (!this->file) {
            this->file.reset(fopen(path.c_str(), "w"));
            if (!this->file)
                throw std::runtime_error("Couldn't open file '" + path +
                                         "': " + strerror(errno));
            return;
        }

        FILE *f = this->file.get();

        // Check whether the header is there. If not, the history file is either
        // invalid or it's using the old version which didn't have the history
        // header yet.
        char start[J4DDHIST_HEADER_LENGTH];
        auto rcount = std::fread(start, J4DDHIST_HEADER_LENGTH, 1, f);
        if (rcount != 1 ||
            memcmp(start, J4DDHIST_HEADER, J4DDHIST_HEADER_LENGTH) != 0) {
            if (is_v0())
                throw v0_version_error("History file '" + path +
                                       "' is outdated!");
            else
                throw std::runtime_error("History file '" + path +
                                         "' is malformed!");
        }

        unsigned int major, minor;
        auto err = std::fscanf(f, "%u.%u", &major, &minor);
        if (err == EOF)
            throw std::runtime_error("Couldn't read history file version of '" +
                                     path + "': " + strerror(errno));
        else if (err != 2)
            // This is probably impossible to happen, but there are never enough
            // checks.
            throw std::runtime_error("Couldn't read history file version of '" +
                                     path + "'!");
        // Get rid of the newline. This completes the reading of the header.
        // Actual data will follow.
        if (std::fgetc(f) != '\n')
            throw std::runtime_error("Format error in history file '" + path +
                                     "'!");

        auto cmp = compare_versions(major, minor);
        if (cmp != 0) {
            throw std::runtime_error(
                (string) "History file is incompatible with the current build "
                         "of j4-demnu-desktop! History file format is too " +
                (cmp < 0 ? "old" : "new") + "! Expected version " +
                std::to_string(J4DDHIST_MAJOR_VERSION) + '.' +
                std::to_string(J4DDHIST_MINOR_VERSION) + ", got version " +
                std::to_string(major) + '.' + std::to_string(minor));
        }

        read_file(path);
    }

    HistoryManager(const HistoryManager &) = delete;
    void operator=(const HistoryManager &) = delete;

    HistoryManager(HistoryManager &&other)
        : file(std::move(other.file)), history(std::move(other.history)) {}

    HistoryManager &operator=(HistoryManager &&other) {
        if (this != &other) {
            this->file = std::move(other.file);
            this->history = std::move(other.history);
        }
        return *this;
    }

    void increment(const string &name) {
        auto result = std::find_if(this->history.begin(), this->history.end(),
                                   [&name](const std::pair<int, string> &val) {
                                       return val.second == name;
                                   });
        if (result == this->history.end())
            history.emplace(std::piecewise_construct, std::forward_as_tuple(1),
                            std::forward_as_tuple(name));
        else {
            history.emplace(std::piecewise_construct,
                            std::forward_as_tuple(result->first + 1),
                            std::forward_as_tuple(name));
            history.erase(result);
        }
    }

    void write() {
        FILE *f = this->file.get();

        std::rewind(f);
        ftruncate(fileno(f), 0);
        std::fputs(
            J4DDHIST_HEADER TOSTRING(J4DDHIST_MAJOR_VERSION) "." TOSTRING(
                J4DDHIST_MINOR_VERSION) "\n",
            f);
        for (const auto &[hist, name] : this->history)
            std::fprintf(f, "%u,%s\n", hist, name.c_str());
        std::fflush(f);
    }

    const std::multimap<int, string, std::greater<int>> &view() const {
        return this->history;
    }

    static HistoryManager convert_history_from_v0(const string &path,
                                                  const AppManager &appm) {
        std::unique_ptr<FILE, decltype(&std::fclose)> f(
            std::fopen(path.c_str(), "r"), std::fclose);
        if (!f)
            throw std::runtime_error("Couldn't open file '" + path +
                                     "' for conversion: " + strerror(errno));

        std::multimap<int, string, std::greater<int>> result;

        size_t linesz = 0;
        char *lineptr = nullptr;

        OnExit free_lineptr = [&lineptr]() { std::free(lineptr); };

        unsigned int hist_count;
        while (fscanf(f.get(), "%u,", &hist_count) == 1) {
            auto rsize = getline(&lineptr, &linesz, f.get());
            if (rsize == -1)
                throw std::runtime_error((std::string) "Read error: " +
                                         strerror(errno));
            lineptr[rsize - 1] = '\0'; // Get rid of \n
            try {
                const Application &app = appm.lookup_by_ID(lineptr).value();
                result.emplace(std::piecewise_construct,
                               std::forward_as_tuple(hist_count),
                               std::forward_as_tuple(app.name));
                if (!app.generic_name.empty())
                    result.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(hist_count),
                                   std::forward_as_tuple(app.generic_name));
            } catch (std::bad_optional_access &) {
                fprintf(stderr,
                        "WARNING: While converting history file '%s' to format "
                        "1.0, desktop file ID '%s' couldn't be resolved. This "
                        "destkop file will be omited from the history.\n",
                        path.c_str(), lineptr);
            }
        }

        f.reset();
        // This file won't be actually used for appeding, write() will rewind it
        // to the beginning and then truncate it.
        FILE *newf = fopen(path.c_str(), "a");

        return HistoryManager(newf, result);
    }

    ~HistoryManager() {
        if (this->file)
            write();
    }

private:
    HistoryManager(FILE *f, std::multimap<int, string, std::greater<int>> hist)
        : file(f, std::fclose), history(std::move(hist)) {}

    // This function tests whether file is the "v0.0" version of the history
    // file. This version doesn't contain the header.
    bool is_v0() {
        FILE *f = this->file.get();
        std::rewind(f);

        enum { COUNT, FILENAME } state = COUNT;

        size_t linesz = 0;
        char *lineptr = nullptr;

        OnExit free_lineptr = [&lineptr]() { std::free(lineptr); };

        // The file format is: [number],[filename which ends in .desktop]\n
        char c;
        while ((c = std::fgetc(f)) != EOF) {
            if (state == COUNT) {
                if (std::isdigit(c))
                    continue;
                else if (c == ',') {
                    state = FILENAME;
                    continue;
                } else {
                    return false;
                }
            } else {
                auto rsize = getline(&lineptr, &linesz, f);
                if (rsize == -1) {
                    return false;
                }

                if (rsize < 9) {
                    return false;
                }

                if (memcmp(lineptr + rsize - 9, ".desktop\n", 9) != 0) {
                    return false;
                }
                state = COUNT;
            }
        }

        return true;
    }

    // This is called in the ctor. It is expected that file is open and the
    // header has already been read.
    void read_file(const string &name) {
        FILE *f = this->file.get();

        size_t linesz = 0;
        char *lineptr = nullptr;

        OnExit free_lineptr = [&lineptr]() { std::free(lineptr); };

        unsigned int history_count;

        int result;
        while ((result = std::fscanf(f, "%u,", &history_count) == 1)) {
            auto read_size = getline(&lineptr, &linesz, f);
            if (read_size < 0) {
                throw std::runtime_error("Error while reading history file '" +
                                         name + "': " + strerror(errno));
            }

            history.emplace(std::piecewise_construct,
                            std::forward_as_tuple(history_count),
                            std::forward_as_tuple(lineptr, read_size - 1));
        }

        if (result == -1)
            throw std::runtime_error("Error while reading history file '" +
                                     name + "': " + strerror(errno));
    }

    std::unique_ptr<FILE, decltype(&std::fclose)> file;
    std::multimap<int, string, std::greater<int>> history;
};

#endif
