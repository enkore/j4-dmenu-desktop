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

#ifndef APPMANAGER_DEF
#define APPMANAGER_DEF

#include <cctype>
#include <forward_list>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "Application.hh"
#include "DynamicCompare.hh"
#include "Formatters.hh"
#include "Utilities.hh"

using std::string;
using std::string_view;

// See doc/AppManager.md for a more high level explanation of the function of
// AppManager.

// This class is basically an Application with added info needed for AppManager.
struct Managed_application
{
    Application app;
    string ID;
    int rank;

    template <typename... Args>
    Managed_application(string ID, int rank, Args... forwarded)
        : app(std::forward<Args>(forwarded)...), ID(ID), rank(rank) {}
};

struct Desktop_file_rank
{
    string base_path;
    std::vector<string> files;

    Desktop_file_rank(string b, std::vector<string> f);
};

// This class represents the input to the ctor of AppManager.
// Each element in this vector represents a list of desktop files in a specific
// rank. An element may be empty.
// If vec is a Desktop_file_list, vec[0] contains the zeroth rank, vec[1]
// contains the first rank...
using Desktop_file_list = std::vector<Desktop_file_rank>;

class AppManager
{
public:
    AppManager(const AppManager &) = delete;
    AppManager(AppManager &&) = delete;
    void operator=(const AppManager &) = delete;
    void operator=(AppManager &&) = delete;

    AppManager(Desktop_file_list files, bool generic_names_enabled,
               bool case_insensitive_sort, application_formatter formatter,
               stringlist_t desktopenvs);

    struct lookup_result_type
    {
        const Application *app;
        // This string contains arguments for the desktop app. It doesn't begin
        // with whitespace. It will either contain text (with at least one
        // non-whitespace character) or be empty.
        // If query doesn't refer to a desktop file, lookup() returns empty
        // optional.
        string args;

        enum name_type { NAME, GENERIC_NAME } name;

        lookup_result_type(const Application *a, string args, name_type n);
    };

    std::optional<lookup_result_type> lookup(const string &query) const;
    void remove(const string &filename, const string &base_path);
    // This function accepts path to the desktop file relative to $XDG_DATA_DIRS
    // and its rank within $XDG_DATA_DIRS
    void add(const string &filename, const string &base_path, int rank);
    std::vector<string_view> list_sorted_names() const;
    std::forward_list<Managed_application>::difference_type count() const;
    ~AppManager();

    // This function should be used only for debugging.
    void check_inner_state() const;

    // This function will never get called in a typical j4dd session. It is used
    // only for converting the old history format to the new onw.
    std::optional<std::reference_wrapper<const Application>>
    lookup_by_ID(const string &ID) const;

private:
    static string trim_surrounding_whitespace(string_view str);

    enum class NameType { name, generic_name };

    // Cleanly remove a name mapping from name_lookup. Collisions are handled
    // properly.
    // Removing a name and a generic_name is practically the same operation.
    // This is why this function exists. remove_name_mapping<NameType::name>
    // removes a Name and remove_name_mapping<NameType::generic_name> removes a
    // GenericName.
    template <NameType N>
    void remove_name_mapping(Managed_application &to_remove) {
        string &name = N == NameType::name ? to_remove.app.name
                                           : to_remove.app.generic_name;

        if (name.empty())
            return;

        auto name_lookup_iter = name_lookup.find(name);
        if (name_lookup_iter == name_lookup.end()) {
            fprintf(stderr,
                    "AppManager has reached a inconsistent state.\n"
                    "Tried to remove application name '%s' which isn't saved!",
                    name.c_str());
            abort();
        }

        // The order of insertions and removals is crucial here. The keys of
        // name_lookup are a string_view which are basically a pointer to
        // applications. The application which currently owns the name
        // (it's pointer is associated with the name in name_lookup) must also
        // own the key to maintain the lifetime of the key.
        if (name_lookup_iter->second == &to_remove) {
            name_lookup.erase(name_lookup_iter);
            // If we have generic names enabled, we need to search for them.
            for (auto iter = this->applications.begin();
                 iter != this->applications.end(); ++iter) {
                if (this->generic_names_enabled) {
                    if (iter->app.name == name ||
                        iter->app.generic_name == name) {
                        if (&*iter == &to_remove)
                            continue;

                        this->name_lookup.try_emplace(
                            N == NameType::name ? iter->app.name
                                                : iter->app.generic_name,
                            &*iter);
                    }
                } else {
                    if (iter->app.name == name) {
                        if (&*iter == &to_remove)
                            continue;
                        this->name_lookup.try_emplace(
                            N == NameType::name ? iter->app.name
                                                : iter->app.generic_name,
                            &*iter);
                    }
                }
            }
        }
    }

    // Add a name mapping, possibly replacing a colliding one if a collision
    // exists and the new managed app has a lower rank.
    template <NameType N>
    void replace_name_mapping(Managed_application &to_add) {
        string &name =
            N == NameType::name ? to_add.app.name : to_add.app.generic_name;

        if (name.empty())
            return;

        auto result = this->name_lookup.try_emplace(name, &to_add);
        if (result.second)
            return;

        int &old_rank = result.first->second->rank;
        if (to_add.rank < old_rank) {
            // We must remove and readd the element if it must be replaced.
            // We can't just change the value of name_lookup's element because
            // the key of the element is a string_view. A replacement of the
            // pointer would mess up the lifetime of the key.
            this->name_lookup.erase(result.first);
            this->name_lookup.try_emplace(name, &to_add);
        }
    }

    using applications_iter_type =
        std::forward_list<Managed_application>::iterator;

    struct app_ID_search_result
    {
        applications_iter_type before_begin, begin;

        app_ID_search_result(applications_iter_type a,
                             applications_iter_type b);
    };

    std::optional<app_ID_search_result> find_app_by_ID(const string &ID);

    // This contains the actual data. All other containers depend on this
    // list. This list should be modified first when adding something and it
    // should be modified last when removing something for lifetime reasons.
    // desktop_IDs depends on this because its string_view points to here and
    // name_lookup's value also points here. A std::vector isn't used here
    // because if a reallocation to it would occur, these two pointers would be
    // invalidated.
    std::forward_list<Managed_application> applications;
    // Set of all registered desktop IDs so far.
    std::unordered_set<string_view> desktop_IDs;
    // Map used for lookup.
    std::map<string_view, Managed_application *, DynamicCompare> name_lookup;

    bool generic_names_enabled;

    // Things needed to construct Application:
    char *linep = nullptr;
    size_t linesz = 0;
    application_formatter formatter;
    LocaleSuffixes suffixes;
    stringlist_t desktopenvs;
};

#endif
