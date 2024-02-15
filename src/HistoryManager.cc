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

#include "HistoryManager.hh"

constexpr static int compare_versions(unsigned int major, unsigned int minor) {
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

HistoryManager::HistoryManager(const string &path)
    : file(std::fopen(path.c_str(), "r+")), filename(path) {
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
    LineReader liner;

    // Check whether the header is there. If not, the history file is either
    // invalid or it's using the old version which didn't have the history
    // header yet.
    char start[J4DDHIST_HEADER_LENGTH];
    auto rcount = std::fread(start, J4DDHIST_HEADER_LENGTH, 1, f);
    if (rcount != 1 ||
        memcmp(start, J4DDHIST_HEADER, J4DDHIST_HEADER_LENGTH) != 0) {
        if (is_v0(liner))
            throw v0_version_error("History file '" + path + "' is outdated!");
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
                     "of j4-dmenu-desktop! History file format is too " +
            (cmp < 0 ? "old" : "new") + "! Expected version " +
            std::to_string(J4DDHIST_MAJOR_VERSION) + '.' +
            std::to_string(J4DDHIST_MINOR_VERSION) + ", got version " +
            std::to_string(major) + '.' + std::to_string(minor));
    }

    read_file(path, liner);
}

HistoryManager::HistoryManager(HistoryManager &&other)
    : file(std::move(other.file)), history(std::move(other.history)),
      filename(other.filename) {}

HistoryManager &HistoryManager::operator=(HistoryManager &&other) {
    if (this != &other) {
        this->file = std::move(other.file);
        this->history = std::move(other.history);
        this->filename = std::move(other.filename);
    }
    return *this;
}

void HistoryManager::increment(const string &name) {
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
    write();
}

void HistoryManager::remove_obsolete_entry(
    history_mmap_type::const_iterator iter) {
    this->history.erase(iter);
}

void HistoryManager::write() {
    FILE *f = this->file.get();

    std::rewind(f);
    ftruncate(fileno(f), 0);
    std::fputs(J4DDHIST_HEADER TOSTRING(J4DDHIST_MAJOR_VERSION) "." TOSTRING(
                   J4DDHIST_MINOR_VERSION) "\n",
               f);
    for (const auto &[hist, name] : this->history)
        std::fprintf(f, "%u,%s\n", hist, name.c_str());
    std::fflush(f);
}

const std::multimap<int, string, std::greater<int>> &
HistoryManager::view() const {
    return this->history;
}

HistoryManager HistoryManager::convert_history_from_v0(const string &path,
                                                       const AppManager &appm) {
    std::unique_ptr<FILE, fclose_deleter> f(std::fopen(path.c_str(), "r"));
    if (!f)
        throw std::runtime_error("Couldn't open file '" + path +
                                 "' for conversion: " + strerror(errno));

    std::multimap<int, string, std::greater<int>> result;
    // Since all displayed names are unique (thanks to AppManager), all names in
    // HistoryManager shall be unique too. Conversion from v0 to v1 might yield
    // duplicate names. For example Tor Browser has "Tor Browser" as both Name
    // and GenericName. But collisions can also of course happen between
    // different desktop files.
    // History file is parsed from top to bottom. Both v0 and v1 formats order
    // history entries from highest to lowest. History entries with highest
    // history count will naturally have higher precedence thanks to this
    // parsing.
    std::unordered_set<string_view> ensure_uniqueness;

    LineReader liner;

    unsigned int hist_count;
    while (fscanf(f.get(), "%u,", &hist_count) == 1) {
        auto rsize = liner.getline(f.get());
        if (rsize == -1)
            throw std::runtime_error((std::string) "Read error: " +
                                     strerror(errno));
        char *line = liner.get_lineptr();
        line[rsize - 1] = '\0'; // Get rid of \n
        try {
            auto lookup = appm.lookup_by_ID(line);
            const Application &app = lookup.value();
            if (ensure_uniqueness.emplace(app.name).second)
                result.emplace(std::piecewise_construct,
                               std::forward_as_tuple(hist_count),
                               std::forward_as_tuple(app.name));
            else
                LOG_F(INFO,
                      "History conversion v0 -> v1: Prevented duplicate Name "
                      "'%s' from appearing in history",
                      app.name.c_str());
            if (!app.generic_name.empty()) {
                if (ensure_uniqueness.emplace(app.generic_name).second)
                    result.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(hist_count),
                                   std::forward_as_tuple(app.generic_name));
                else
                    LOG_F(INFO,
                          "History conversion v0 -> v1: Prevented duplicate "
                          "GenericName '%s' from appearing in history",
                          app.name.c_str());
            }
        } catch (std::bad_optional_access &) {
            LOG_F(WARNING,
                  "While converting history file '%s' to format "
                  "1.0, desktop file ID '%s' couldn't be resolved. This "
                  "desktop file will be omitted from the history.\n",
                  path.c_str(), line);
        }
    }

    f.reset();
    // This file won't be actually used for appending, write() will rewind it
    // to the beginning and then truncate it.
    FILE *newf = fopen(path.c_str(), "a");

    auto histm = HistoryManager(newf, result, path);
    histm.write();
    return histm;
}

const std::string &HistoryManager::get_filename() const {
    return this->filename;
}

HistoryManager::HistoryManager(
    FILE *f, std::multimap<int, string, std::greater<int>> hist,
    std::string filename)
    : file(f), history(std::move(hist)), filename(std::move(filename)) {}

bool HistoryManager::is_v0(LineReader &liner) {
    FILE *f = this->file.get();
    std::rewind(f);

    enum { COUNT, FILENAME } state = COUNT;

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
            auto rsize = liner.getline(f);
            if (rsize == -1) {
                return false;
            }

            char *line = liner.get_lineptr();

            if (rsize < 9) {
                return false;
            }

            if (memcmp(line + rsize - 9, ".desktop\n", 9) != 0) {
                return false;
            }
            state = COUNT;
        }
    }

    return true;
}

void HistoryManager::read_file(const string &name, LineReader &liner) {
    FILE *f = this->file.get();

    unsigned int history_count;

    int result;
    while ((result = std::fscanf(f, "%u,", &history_count) == 1)) {
        auto read_size = liner.getline(f);
        if (read_size < 0) {
            throw std::runtime_error("Error while reading history file '" +
                                     name + "': " + strerror(errno));
        }

        if (read_size == 1)
            throw std::runtime_error("Error while reading history file '" +
                                     name + "': Empty history entry present!");

        history.emplace(
            std::piecewise_construct, std::forward_as_tuple(history_count),
            std::forward_as_tuple(liner.get_lineptr(), read_size - 1));
    }

    if (result == -1)
        throw std::runtime_error("Error while reading history file '" + name +
                                 "': " + strerror(errno));
}
