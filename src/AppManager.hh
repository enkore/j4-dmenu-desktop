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

#include <loguru.hpp>

#include "Application.hh"
#include "Formatters.hh"
#include "Utilities.hh"

using std::string;
using std::string_view;

// See doc/AppManager.md for a more high level explanation of the function of
// AppManager.

std::string get_desktop_id(std::string filename);
std::string get_desktop_id(const std::string &filename, std::string_view base);

// This class is basically an Application with added info needed for AppManager.
struct Managed_application
{
    Application app;
    int rank;

    template <typename... Args>
    Managed_application(int rank, Args... forwarded)
        : app(std::forward<Args>(forwarded)...), rank(rank) {}
};

struct Desktop_file_rank
{
    string base_path;
    std::vector<string> files;

    Desktop_file_rank(string b, std::vector<string> f);
};

struct Resolved_application
{
    const Application *app;
    bool is_generic;

    Resolved_application(const Application *app, bool is_generic);
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
    using name_app_mapping_type =
        std::unordered_map<string_view /*(Generic)Name*/, Resolved_application>;

    AppManager(const AppManager &) = delete;
    AppManager(AppManager &&) = delete;
    void operator=(const AppManager &) = delete;
    void operator=(AppManager &&) = delete;

    AppManager(Desktop_file_list files, stringlist_t desktopenvs,
               LocaleSuffixes suffixes);

    void remove(const string &filename, const string &base_path);
    // This function accepts path to the desktop file relative to $XDG_DATA_DIRS
    // and its rank within $XDG_DATA_DIRS
    void add(const string &filename, const string &base_path, int rank);
    std::forward_list<Managed_application>::difference_type count() const;
    const name_app_mapping_type &view_name_app_mapping() const;
    ~AppManager();

    // This function should be used only for debugging.
    void check_inner_state() const;

    // This function will never get called in a typical j4dd session. It is used
    // only for converting the old history format to the new onw.
    std::optional<std::reference_wrapper<const Application>>
    lookup_by_ID(const string &ID) const;

private:
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

        auto name_lookup_iter = name_app_mapping.find(name);
        if (name_lookup_iter == name_app_mapping.end()) {
            ABORT_F("AppManager has reached a inconsistent state. Tried to "
                    "remove application name '%s' which isn't saved!",
                    name.c_str());
        }

        // The order of insertions and removals is crucial here. The keys of
        // name_lookup are a string_view which are basically a pointer to
        // applications. The application which currently owns the name
        // (it's pointer is associated with the name in name_lookup) must also
        // own the key to maintain the lifetime of the key.
        if (name_lookup_iter->second.app == &to_remove.app) {
            name_app_mapping.erase(name_lookup_iter);
            // We will look through all applications to find one with the same
            // (Generic)Name to replace the current one. The match with the
            // lowest rank wins.
            string_view best_match_name;
            const Application *best_match_app;
            bool best_match_is_generic;
            int best_match_rank = std::numeric_limits<int>::max();

            for (auto iter = this->applications.begin();
                 iter != this->applications.end(); ++iter) {
                Managed_application &managed_app = iter->second;
                Application &app = managed_app.app;

                if (app.name == name || app.generic_name == name) {
                    if (&managed_app == &to_remove)
                        continue;
                    // When there are multiple candidates in the same rank,
                    // the replacement isn't chosen "deterministically", it
                    // isn't sorted so the choice depends on the ordering of
                    // unordered_map.

                    // We are looking for the match with the lowest rank.
                    // This match has a higher rank (or the same), we aren't
                    // interested in it.
                    if (managed_app.rank >= best_match_rank)
                        continue;

                    // We have found a better match. We have to know whether we
                    // have matched a Name or a GenericName.
                    best_match_app = &app;
                    best_match_rank = managed_app.rank;
                    if (app.name == name) {
                        best_match_name = string_view(app.name);
                        best_match_is_generic = false;
                    } else {
                        best_match_name = string_view(app.generic_name);
                        best_match_is_generic = true;
                    }
                }
            }

            if (best_match_rank != std::numeric_limits<int>::max()) {
                this->name_app_mapping.try_emplace(
                    best_match_name, best_match_app, best_match_is_generic);
            }
        }
    }

    // Add a name mapping, possibly replacing a colliding one if a collision
    // exists and the new managed app has a lower rank.
    template <NameType N>
    void replace_name_mapping(Managed_application &to_add) {
        string &name =
            N == NameType::name ? to_add.app.name : to_add.app.generic_name;

        auto result = this->name_app_mapping.try_emplace(
            name, &to_add.app, N == NameType::generic_name);
        if (result.second)
            return;

        auto colliding_app_ptr = result.first->second.app;

        using value_type = typename decltype(this->applications)::value_type;
        auto colliding_iter =
            std::find_if(this->applications.begin(), this->applications.end(),
                         [&colliding_app_ptr](const value_type &val) {
                             return &val.second.app == colliding_app_ptr;
                         });
        if (colliding_iter == this->applications.end()) {
            ABORT_F("AppManager has reached a inconsistent state. Couldn't "
                    "find Application* for name '%s' when there should be one.",
                    name.c_str());
        }
        Managed_application &colliding_managed_app = colliding_iter->second;

        int &old_rank = colliding_managed_app.rank;
        if (to_add.rank < old_rank) {
            // We must remove and readd the element if it must be replaced.
            // We can't just change the value of name_lookup's element because
            // the key of the element is a string_view. A replacement of the
            // pointer would mess up the lifetime of the key.
            this->name_app_mapping.erase(result.first);
            this->name_app_mapping.try_emplace(name, &to_add.app,
                                               N == NameType::generic_name);
        }
    }

    // This contains the actual data. All other containers depend on this
    // unordered_map. This list should be modified first when adding something
    // and it should be modified last when removing something for lifetime
    // reasons.
    std::unordered_map<string /*desktop ID*/, Managed_application> applications;
    // Map used for lookup and name listing.
    name_app_mapping_type name_app_mapping;

    // Things needed to construct Application:
    char *linep = nullptr;
    size_t linesz = 0;
    LocaleSuffixes suffixes;
    stringlist_t desktopenvs;
};

#endif
