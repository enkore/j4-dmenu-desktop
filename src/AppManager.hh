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

    Desktop_file_rank(string b, std::vector<string> f)
        : base_path(std::move(b)), files(std::move(f)) {}
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
               stringlist_t desktopenvs)
        : name_lookup(DynamicCompare(case_insensitive_sort)),
          generic_names_enabled(generic_names_enabled), formatter(formatter),
          desktopenvs(desktopenvs) {
        if (files.size() > std::numeric_limits<int>::max()) {
            fprintf(stderr, "Rank overflow in AppManager ctor!");
            abort();
        }
        for (int rank = 0; rank < (int)files.size(); ++rank) {
            auto &rank_files = files[rank].files;
            auto &rank_base_path = files[rank].base_path;

            for (const string &filename : rank_files) {
                string desktop_file_ID =
                    get_desktop_id(filename, rank_base_path);

                // Handle desktop file ID collision.
                if (this->desktop_IDs.count(desktop_file_ID) == 1)
                    continue;

                try {
                    Managed_application &newly_added =
                        this->applications.emplace_front(
                            desktop_file_ID, rank, filename.c_str(),
                            &this->linep, &this->linesz, this->formatter,
                            this->suffixes, this->desktopenvs);

                    this->desktop_IDs.emplace(newly_added.ID);

                    // Add the names.
                    this->name_lookup.try_emplace(newly_added.app.name,
                                                  &newly_added);
                    if (generic_names_enabled &&
                        !newly_added.app.generic_name.empty())
                        this->name_lookup.try_emplace(
                            newly_added.app.generic_name, &newly_added);
                } catch (disabled_error &) {
                    // Skip disabled files.
                    continue;
                }
            }
        }
    }

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

        lookup_result_type(const Application *a, string args, name_type n)
            : app(a), args(std::move(args)), name(n) {}
    };

    std::optional<lookup_result_type> lookup(const string &query) const {
        // This is the most likely path of execution.
        auto iter = this->name_lookup.find(query);
        if (iter != this->name_lookup.end())
            // We need to determine whether the selected entry is a name or a
            // generic name. The key of name_lookup is owned by the
            // Managed_application pointed to by its value. We can just check
            // whether the pointers are the same.
            return lookup_result_type(&iter->second->app, {},
                                      iter->first.data() ==
                                              iter->second->app.name.data()
                                          ? lookup_result_type::NAME
                                          : lookup_result_type::GENERIC_NAME);

        // Linear lookup proceeds. This lookup could be improved.
        for (const auto &[name, ptr] : this->name_lookup) {
            if (name.size() > query.size())
                continue;
            // Argument handling of desktop apps isn't very ideal now.
            if (query.compare(0, name.size(), name) == 0) {
                if (name.size() == query.size()) {
                    fprintf(
                        stderr,
                        "AppManager has reached a inconsistent state.\n"
                        "Search for program '%s' returned inconsistent "
                        "results (a match with desktop file '%s' was found).",
                        query.c_str(), ptr->ID.c_str());
                    abort();
                } else {
                    return lookup_result_type(
                        &ptr->app,
                        trim_surrounding_whitespace(
                            string_view(query.data() + name.size(),
                                        query.size() - name.size())),
                        name.data() == ptr->app.name.data()
                            ? lookup_result_type::NAME
                            : lookup_result_type::GENERIC_NAME);
                }
            }
        }

        return {};
    }

    void remove(const string &filename, const string &base_path) {
        // Desktop file ID must be relative to $XDG_DATA_DIRS. We need the base
        // path to determine it. Another solution would be to accept a relative
        // path as the filename.
        string ID = get_desktop_id(filename, base_path);
        auto desktop_id_iter = this->desktop_IDs.find(ID);
        if (desktop_id_iter == this->desktop_IDs.end()) {
            fprintf(stderr,
                    "AppManager has reached a inconsistent state.\n"
                    "Removal of desktop file '%s' has been requested (desktop "
                    "id: %s). Desktop id couldn't be found.\n",
                    filename.c_str(), ID.c_str());
            abort();
        }

        auto app_search = find_app_by_ID(ID);

        if (!app_search) {
            fprintf(stderr,
                    "AppManager has reached a inconsistent state.\n"
                    "Removal of desktop file '%s' has been requested (desktop "
                    "id: %s). Desktop id couldn't be found.\n",
                    filename.c_str(), ID.c_str());
            abort();
        }

        Managed_application &app = *app_search->begin;
        remove_name_mapping<NameType::name>(app);
        if (this->generic_names_enabled)
            remove_name_mapping<NameType::generic_name>(app);

        this->desktop_IDs.erase(desktop_id_iter);
        this->applications.erase_after(app_search->before_begin);
    }

    // This function accepts path to the desktop file relative to $XDG_DATA_DIRS
    // and its rank within $XDG_DATA_DIRS
    void add(const string &filename, const string &base_path, int rank) {
        string ID = get_desktop_id(filename, base_path);

        // If Application ctor throws, AppManager's state must remain
        // consistent.
        if (this->desktop_IDs.count(ID)) {
            // Find the colliding app by its ID.
            auto app_iter = std::find_if(
                this->applications.begin(), this->applications.end(),
                [&ID](const Managed_application &app) { return app.ID == ID; });
            if (app_iter == applications.end()) {
                fprintf(stderr,
                        "AppManager has reached a inconsistent state.\n"
                        "Addition of desktop file '%s' has been requested "
                        "(desktop id: %s). Desktop id couldn't be found.\n",
                        filename.c_str(), ID.c_str());
                abort();
            }
            Managed_application &managed_app = *app_iter;

            // NOTE: This behaviour is different from the constructor! Read
            // doc/AppManager.md collisions.
            if (managed_app.rank < rank)
                return;

            // We can't overwrite the old app directly because we'll need it
            // later. We first try to construct Application in a std::optional.
            std::optional<Application> new_app;
            try {
                new_app.emplace(filename.c_str(), &this->linep, &this->linesz,
                                this->formatter, this->suffixes,
                                this->desktopenvs);
            } catch (disabled_error &) {
                // Skip disabled files.
                return;
            }

            remove_name_mapping<NameType::name>(managed_app);
            if (this->generic_names_enabled)
                remove_name_mapping<NameType::generic_name>(managed_app);

            managed_app.rank = rank;
            managed_app.app = std::move(*new_app);

            replace_name_mapping<NameType::name>(managed_app);
            if (this->generic_names_enabled)
                replace_name_mapping<NameType::generic_name>(managed_app);
        } else {
            Managed_application *app_ptr;
            try {
                app_ptr = &this->applications.emplace_front(
                    ID, rank, filename.c_str(), &this->linep, &this->linesz,
                    this->formatter, this->suffixes, this->desktopenvs);
            } catch (disabled_error &) {
                // Skip disabled files.
                return;
            }

            Managed_application &app = *app_ptr;

            desktop_IDs.emplace(app.ID);

            replace_name_mapping<NameType::name>(app);
            if (this->generic_names_enabled)
                replace_name_mapping<NameType::generic_name>(app);
        }
    }

    std::vector<string_view> list_sorted_names() const {
        std::vector<string_view> result;
        result.reserve(this->name_lookup.size());

        for (auto elem = this->name_lookup.begin();
             elem != this->name_lookup.end(); ++elem)
            result.emplace_back(elem->first);

        return result;
    }

    std::forward_list<Managed_application>::difference_type count() const {
        // This has linear complexity.
        return std::distance(this->applications.cbegin(),
                             this->applications.cend());
    }

    ~AppManager() {
        if (this->linep)
            free(this->linep);
    }

    // This function should be used only for debugging.
    void check_inner_state() const {
        if (count() != (int)this->desktop_IDs.size()) {
            fprintf(stderr, "AppManager check error:\n"
                            "The number of elements in applications doesn't "
                            "match the number of elements in desktop_IDs!");
            abort();
        }

        for (string_view desktop_ID : this->desktop_IDs) {
            // The lifetimes in this class are kinda funky because the lifetime
            // of everything indirectly depends on applications.
            // An example of such error is having an element in desktop_IDs
            // whose value (remember that the value of desktop_IDs is
            // string_view which has its lifetime tied to the corresponding
            // element in applications) is corrupted because the desktop file in
            // applications has been removed and the desktop ID in desktop_IDs
            // has stayed. If that happens, the element in desktop_IDs will
            // contain garbage data.
            // If AppManager is in a consistent state, all desktop IDs in
            // destkop_IDs should be null terminated because they point to
            // std::string which is null terminated. We can use this fact to
            // detect whether there is garbage data. If everything is
            // implemented well, this condition will never be true.
            // Note that this detection of faulty memory access isn't perfect.
            // I (meator) run all unit tests with AddressSanitizer and with gnu
            // libstdc++ debug mode enabled which should catch all errors.
            // Even if this function detects no errors itself, it will try to
            // access all relevant data which will trigger asan if anything bad
            // happened.
            // By the way, we can't just do desktop_ID[desktop_ID.size()]
            // because that is undefined behavior. desktop_ID.data() +
            // desktop_ID.size() is still undefined behavior, but it "fixes"
            // _GLIBCXX_DEBUG errors. All string_views point to std::string
            // which are terminated by \0 so we aren't accessing bad memory.
            if (desktop_ID.data()[desktop_ID.size()] != '\0') {
                fprintf(stderr,
                        "AppManager check error:\n"
                        "A desktop ID in desktop_IDs is likely corrupted!");
                abort();
            }

            if (desktop_ID.empty()) {
                fprintf(stderr, "AppManager check error:\n"
                                "A desktop ID in desktop_IDs is empty!");
                abort();
            }

            if (std::all_of(desktop_ID.cbegin(), desktop_ID.cend(), isspace)) {
                fprintf(stderr,
                        "AppManager check error:\n"
                        "A desktop ID contains no printable characters!");
                abort();
            }

            if (std::find_if(this->applications.cbegin(),
                             this->applications.cend(),
                             [desktop_ID](const Managed_application &app) {
                                 return app.ID == desktop_ID;
                             }) == this->applications.cend()) {
                fprintf(stderr, "AppManager check error:\n"
                                "A desktop file ID marked in desktop_IDs "
                                "couldn't be found in applications!");
                abort();
            }
        }

        for (const Managed_application &app : this->applications) {
            if (app.rank < 0) {
                fprintf(stderr, "AppManager check error:\n"
                                "A managed application in applications has a "
                                "negative rank!");
                abort();
            }
            if (app.app.exec.empty() ||
                app.app.exec[app.app.exec.size()] != '\0') {
                fprintf(stderr, "AppManager check error:\n"
                                "A managed application in applications might "
                                "not have been constructed!");
                abort();
            }
        }

        for (const auto &[name, ptr] : this->name_lookup) {
            if (name.empty()) {
                fprintf(stderr, "AppManager check error:\n"
                                "A name in name_lookup is empty!");
                abort();
            }
            // See above for explanation of .data()[]
            if (name.data()[name.size()] != '\0') {
                fprintf(stderr, "AppManager check error:\n"
                                "A name in name_lookup is likely corrupted!");
                abort();
            }
            if (std::find_if(applications.cbegin(), applications.cend(),
                             [&name = name](const Managed_application &app) {
                                 return app.app.name.data() == name.data() ||
                                        app.app.generic_name.data() ==
                                            name.data();
                             }) == applications.cend()) {
                fprintf(stderr, "AppManager check error:\n"
                                "A name in name_lookup points to an unknown "
                                "location not in applications!");
                abort();
            }
            if (std::find_if(applications.cbegin(), applications.cend(),
                             [&ptr = ptr](const Managed_application &app) {
                                 return &app == ptr;
                             }) == applications.cend()) {
                fprintf(stderr, "AppManager check error:\n"
                                "An managed application pointer in name_lookup "
                                "points to an unknown managed application not "
                                "in applications!");
                abort();
            }
        }
    }

private:
    static string trim_surrounding_whitespace(string_view str) {
        auto begin = str.cbegin();
        for (; begin != str.cend(); ++begin) {
            if (!isspace(*begin))
                break;
        }
        if (begin == str.cend())
            return "";

        auto end = std::prev(str.cend());
        for (; end != begin; --end) {
            if (!isspace(*end))
                break;
        }
        return string(begin, std::next(end));
    }

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

        app_ID_search_result(applications_iter_type a, applications_iter_type b)
            : before_begin(a), begin(b) {}
    };

    std::optional<app_ID_search_result> find_app_by_ID(const string &ID) {
        // We need to alter the search for the ID little because forward_list
        // must have the iterator to the element before the elements being
        // deleted. This std::find() clone saves the previous iterator.
        applications_iter_type before_elem = this->applications.before_begin(),
                               elem;
        for (elem = this->applications.begin();
             elem != this->applications.end(); ++elem) {
            if (elem->ID == ID)
                return app_ID_search_result(before_elem, elem);
            before_elem = elem;
        }
        return std::nullopt;
    }

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
