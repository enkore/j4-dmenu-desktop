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

#include "AppManager.hh"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdlib.h>

std::string get_desktop_id(std::string filename) {
    std::string result(std::move(filename));
    replace(result.begin(), result.end(), '/', '-');
    return result;
}

std::string get_desktop_id(const std::string &filename, std::string_view base) {
#ifdef DEBUG
    if (!startswith(filename, std::string(base))) {
        SPDLOG_ERROR("Filename '{}' must begin with base path '{}'!", filename,
                     base);
        abort();
    }
#endif
    return get_desktop_id(filename.substr(base.size()));
}

Desktop_file_rank::Desktop_file_rank(string b, std::vector<string> f)
    : base_path(std::move(b)), files(std::move(f)) {}

Resolved_application::Resolved_application(const Application *app,
                                           bool is_generic)
    : app(app), is_generic(is_generic) {}

AppManager::AppManager(Desktop_file_list files, stringlist_t desktopenvs,
                       LocaleSuffixes suffixes)
    : suffixes(std::move(suffixes)), desktopenvs(desktopenvs) {
    SPDLOG_DEBUG("AppManager: Entered AppManager");
    if (files.size() > std::numeric_limits<int>::max()) {
        SPDLOG_ERROR("Rank overflow in AppManager ctor!");
        exit(EXIT_FAILURE);
    }
    for (int rank = 0; rank < (int)files.size(); ++rank) {
        auto &rank_files = files[rank].files;
        auto &rank_base_path = files[rank].base_path;

        SPDLOG_DEBUG("AppManager: Processing rank -> {} <- (base: {})", rank,
                     rank_base_path);

        for (const string &filename : rank_files) {
            string desktop_file_ID = get_desktop_id(filename, rank_base_path);

            SPDLOG_DEBUG("AppManager:   Handling file '{}' ID: {}", filename,
                         desktop_file_ID);

            try {
                auto try_add = this->applications.try_emplace(
                    desktop_file_ID, rank, filename.c_str(), this->liner,
                    this->suffixes, this->desktopenvs);

                // Handle desktop file ID collision.
                if (!try_add.second) {
                    SPDLOG_DEBUG(
                        "AppManager:     Collision detected, skipping!");
                    continue;
                }

                Managed_application &newly_added = try_add.first->second;

                // Add the names.
                auto add_result = this->name_app_mapping.try_emplace(
                    newly_added.app.name, &newly_added.app, false);
                if (!add_result.second)
                    SPDLOG_DEBUG(
                        "AppManager:     Name '{}' is already taken! Not "
                        "registering.",
                        newly_added.app.name);
                if (!newly_added.app.generic_name.empty()) {
                    auto add_result2 = this->name_app_mapping.try_emplace(
                        newly_added.app.generic_name, &newly_added.app, true);
                    if (!add_result2.second)
                        SPDLOG_DEBUG(
                            "AppManager:     GenericName '{}' is already "
                            "taken! Not registering.",
                            newly_added.app.generic_name);
                }
            } catch (disabled_error &e) {
                SPDLOG_DEBUG("AppManager:     Desktop file is skipped: {}",
                             e.what());
                // Skip disabled files.
                continue;
            } catch (open_error &e) {
                SPDLOG_WARN("Couldn't open file '{}': {}", filename, e.what());
                continue;
            }
        }
    }
}

void AppManager::remove(const string &filename, const string &base_path) {
    // Desktop file ID must be relative to $XDG_DATA_DIRS. We need the base
    // path to determine it. Another solution would be to accept a relative
    // path as the filename.
    string ID = get_desktop_id(filename, base_path);
    SPDLOG_INFO("AppManager: Removing file '{}' (ID: {}, base path: {})",
                filename, ID, base_path);
    auto app_iter = this->applications.find(ID);
    if (app_iter == this->applications.end()) {
        SPDLOG_INFO("Removal of desktop file '{}' has been requested (desktop "
                    "id: {}). Desktop id couldn't be found, ignoring...",
                    filename, ID);
        return;
    }

    Managed_application &app = app_iter->second;
    remove_name_mapping<NameType::name>(app);
    if (!app.app.generic_name.empty())
        remove_name_mapping<NameType::generic_name>(app);

    this->applications.erase(app_iter);
}

void AppManager::add(const string &filename, const string &base_path,
                     int rank) {
    string ID = get_desktop_id(filename, base_path);

    SPDLOG_INFO(
        "AppManager: Adding file '{}' (ID: {}, base path: {}, rank: {})",
        filename, ID, base_path, rank);

    // If Application ctor throws, AppManager's state must remain
    // consistent.

    // Find a colliding app by its ID if there is a collision.
    auto app_iter = this->applications.find(ID);
    if (app_iter != this->applications.end()) {
        SPDLOG_DEBUG("AppManager:   File '{}' is in ID collision.", filename);

        Managed_application &managed_app = app_iter->second;

        // NOTE: This behaviour is different from the constructor! Read
        // doc/AppManager.md collisions.
        if (managed_app.rank < rank) {
            SPDLOG_DEBUG("AppManager:     Older app takes precedence, skipping "
                         "addition.");
            return;
        }

        // We can't overwrite the old app directly because we'll need it
        // later. We first try to construct Application in a std::optional.
        std::optional<Application> new_app;
        try {
            new_app.emplace(filename.c_str(), this->liner, this->suffixes,
                            this->desktopenvs);
        } catch (disabled_error &e) {
            SPDLOG_DEBUG("AppManager:     App is disabled: {}", e.what());
            // Skip disabled files.
            return;
        } catch (open_error &e) {
            SPDLOG_WARN("Couldn't open newly added file '{}': {}", filename,
                        e.what());
            return;
        }

        remove_name_mapping<NameType::name>(managed_app);
        if (!managed_app.app.generic_name.empty())
            remove_name_mapping<NameType::generic_name>(managed_app);

        managed_app.rank = rank;
        managed_app.app = std::move(*new_app);

        replace_name_mapping<NameType::name>(managed_app);
        if (!managed_app.app.generic_name.empty())
            replace_name_mapping<NameType::generic_name>(managed_app);
    } else {
        SPDLOG_DEBUG("AppManager:   File '{}' has no ID collision.", filename);
        Managed_application *app_ptr;
        try {
            app_ptr = &this->applications
                           .try_emplace(ID, rank, filename.c_str(), this->liner,
                                        this->suffixes, this->desktopenvs)
                           .first->second;
        } catch (disabled_error &e) {
            SPDLOG_DEBUG("AppManager:     App is disabled: {}", e.what());
            // Skip disabled files.
            return;
        } catch (open_error &e) {
            SPDLOG_WARN("Couldn't open newly added file '{}': {}", filename,
                        e.what());
            return;
        }

        Managed_application &app = *app_ptr;

        replace_name_mapping<NameType::name>(app);
        if (!app.app.generic_name.empty())
            replace_name_mapping<NameType::generic_name>(app);
    }
}

const AppManager::name_app_mapping_type &
AppManager::view_name_app_mapping() const {
    return this->name_app_mapping;
}

std::forward_list<Managed_application>::difference_type
AppManager::count() const {
    return this->applications.size();
}

// This function should be used only for debugging.
void AppManager::check_inner_state() const {
    // The lifetimes in this class are kinda funky because the lifetime
    // of everything indirectly depends on applications.
    // An example of such error is having an element in name_app_mapping
    // whose key (remember that the key of name_app_mapping is
    // string_view which has its lifetime tied to the corresponding
    // element in applications) is corrupted because the desktop file in
    // applications has been removed and the name in name_app_mapping
    // has stayed. If that happens, the element in name_app_mapping will
    // contain garbage data.
    // If AppManager is in a consistent state, all desktop names in
    // name_app_mapping should be null terminated because they point to
    // std::string which is null terminated. We can use this fact to
    // detect whether there is garbage data. If everything is
    // implemented well, this condition will never be true.
    // Note that this detection of faulty memory access isn't perfect.
    // I (meator) run all unit tests with AddressSanitizer and with gnu
    // libstdc++ debug mode enabled which should catch all errors.
    // Even if this function detects no errors itself, it will try to
    // access all relevant data which will trigger ASAN if anything bad
    // happened.
    // By the way, we can't just do desktop_ID[desktop_ID.size()]
    // because that is undefined behavior. desktop_ID.data() +
    // desktop_ID.size() is still undefined behavior, but it "fixes"
    // _GLIBCXX_DEBUG errors. All string_views point to std::string
    // which are terminated by \0 so we aren't accessing bad memory.
    for (const auto &[ID, app] : this->applications) {
        if (ID.empty()) {
            SPDLOG_ERROR("AppManager check error: A managed application in "
                         "applications has a empty desktop file ID!");
            abort();
        }
        if (app.rank < 0) {
            SPDLOG_ERROR("AppManager check error: A managed application in "
                         "applications has a negative rank!");
            abort();
        }
        if (app.app.exec.empty() || app.app.exec[app.app.exec.size()] != '\0') {
            SPDLOG_ERROR("AppManager check error: A managed application in "
                         "applications might not have been constructed!");
            abort();
        }
    }

    for (const auto &[name, resolved] : this->name_app_mapping) {
        if (name.empty()) {
            SPDLOG_ERROR(
                "AppManager check error: A name in name_app_mapping is empty!");
            abort();
        }
        // See above for explanation of .data()[]
        if (name.data()[name.size()] != '\0') {
            SPDLOG_ERROR(
                "AppManager check error: A name in name_app_mapping is "
                "likely corrupted!");
            abort();
        }
        using value_type = decltype(this->applications)::value_type;
        if (std::find_if(applications.cbegin(), applications.cend(),
                         [&name = name](const value_type &val) {
                             return val.second.app.name.data() == name.data() ||
                                    val.second.app.generic_name.data() ==
                                        name.data();
                         }) == applications.cend()) {
            SPDLOG_ERROR(
                "AppManager check error: A name in name_app_mapping points "
                "to an unknown location not in applications!");
            abort();
        }
        if (std::find_if(applications.cbegin(), applications.cend(),
                         [&ptr = resolved.app](const value_type &val) {
                             return &val.second.app == ptr;
                         }) == applications.cend()) {
            SPDLOG_ERROR(
                "AppManager check error: An managed application pointer in "
                "name_app_mapping points to an unknown managed application "
                "not in applications!");
            abort();
        }
    }
}

std::optional<std::reference_wrapper<const Application>>
AppManager::lookup_by_ID(const string &ID) const {
    auto result = this->applications.find(ID);
    if (result == this->applications.end())
        return {};
    else
        return result->second.app;
}
