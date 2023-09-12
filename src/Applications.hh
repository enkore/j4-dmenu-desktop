#ifndef APPLICATIONS_DEF
#define APPLICATIONS_DEF

#include "Application.hh"
#include "Formatters.hh"
#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>

// a container friendly version of const std::string &
struct string_ref : public std::reference_wrapper<const std::string>
{
    using std::reference_wrapper<const std::string>::reference_wrapper;

    bool operator<(const string_ref &other) const {
        return get() < other.get();
    }

    bool operator==(const string_ref &other) const {
        return get() == other.get();
    }
};

template <> struct std::hash<string_ref>
{
    std::size_t operator()(const string_ref &tohash) const noexcept {
        return std::hash<remove_const<string_ref::type>::type>{}(tohash.get());
    }
};

#include "Application.hh"

// a helper function that safely gets an iterator from container.insert() and
// container.emplace()
template <typename iterator>
iterator getiter(const std::pair<iterator, bool> &p) {
    if (!p.second) {
        fprintf(stderr, "Tried to insert an element when it was already "
                        "present, aborting...\n");
        abort();
    }
    return p.first;
}

class Applications
{
private:
    // This struct holds iterators to sortedentrymap members and a reference to
    // desktop file ID which is useful in update_log().
    struct histentry
    {
        // These iterators point to a valid element only if the name is
        // currently displayed i.e. isdisplayed() returns true.
        std::set<string_ref>::const_iterator iter;
        std::set<string_ref>::const_iterator generic_iter;
        const std::string &id;

        histentry(const std::string &s) : id(s) {}

        bool operator==(const histentry &other) const {
            return iter == other.iter && generic_iter == other.generic_iter &&
                   id == other.id;
        }
    };

    struct appcontainer;

    struct namelistentrytype
    {
        appcontainer *const app;
        bool isgeneric;

        namelistentrytype(appcontainer *a, bool i) : app(a), isgeneric(i) {}

        bool operator<(const namelistentrytype &other) const {
            return app->hist < other.app->hist;
        }

        bool operator==(const namelistentrytype &other) const {
            return app == other.app && isgeneric == other.isgeneric;
        }
    };

    // The primary struct.
    // All data about a desktop file is directly or indirectly accessible
    // through this.
    struct appcontainer
    {
        // Precedence of the desktop file based on the SearchPath order.
        // When two desktop files have the same ID, the one with lower rank
        // value has priority.
        int rank;
        Application app;

        struct
        {
            std::multiset<namelistentrytype>::const_iterator iter;
            std::multiset<namelistentrytype>::const_iterator generic_iter;
        } listcandidate;

        std::list<histentry>::iterator listed;
        unsigned hist = 0;
        bool display = true;

        appcontainer(int r, const Application &a) : rank(r), app(a) {}

        bool operator==(const appcontainer &other) const {
            return rank == other.rank && app == other.app &&
                   hist == other.hist && display == other.display;
        }
    };

    // None of these four containers shall have an empty container as their
    // value, because that would break things. Applications must take care to
    // remove the entire key value pair when it's removing the last entry from
    // value. For example when there are two names belonging to one desktop
    // entry in sortedentrymap with history 1, the sortedentrymap[1] must be
    // removed altogether when the desktop entry's history increases because
    // empty std::set<string_ref> would break sortediter.

    std::unordered_map<std::string /* ID */, appcontainer> idmap;

    // This map contains all Names and GenericNames of all programs as keys and
    // the desktop entries with that name as values. This prevents duplicate
    // names. Each name can have one or more namelistentrytypes attached to it,
    // but only the last one (the one for which isdisplayed() returns true) is
    // active.
    std::unordered_map<std::string, std::multiset<namelistentrytype>> namelist;

    std::map<unsigned, std::list<histentry>> sortedhistmap;
    // This map is used for sorted output into Dmenu.
    // Beware, a desktop entry doesn't have to have both Name and GenericName
    // displayed because there could be a desktop entry with higher history and
    // with the same Name or GenericName. Higher history has higher priority.
    // Both Name and GenericName can be shadowed.
    std::map<unsigned, std::set<string_ref>, std::greater<unsigned>>
        sortedentrymap;

    application_formatter format;
    char *linep = nullptr;
    size_t linesz = 0;
    LocaleSuffixes suffixes;
    stringlist_t desktopenvs;
    bool exclude_generic;
    bool use_history;

    bool isdisplayed(const std::multiset<namelistentrytype> &list,
                     const std::multiset<namelistentrytype>::const_iterator
                         &listiter) const {
        return std::prev(list.end()) == listiter;
    }

    bool isdisplayed(const appcontainer &app, bool isgeneric) const {
        if (isgeneric)
            return isdisplayed(namelist.at(app.app.generic_name),
                               app.listcandidate.generic_iter);
        else
            return isdisplayed(namelist.at(app.app.name),
                               app.listcandidate.iter);
    }

    void add_generic(appcontainer &appc, bool isgeneric) {
        const std::string &name =
            isgeneric ? appc.app.generic_name : appc.app.name;
        std::set<string_ref>::const_iterator &histiter =
            isgeneric ? appc.listed->generic_iter : appc.listed->iter;
        std::multiset<namelistentrytype>::const_iterator &namelistentry =
            isgeneric ? appc.listcandidate.generic_iter
                      : appc.listcandidate.iter;

        auto listentries = namelist.find(name);
        if (listentries == namelist.end()) {
            namelistentry = namelist[name].emplace(&appc, isgeneric);
            histiter = getiter(sortedentrymap[appc.hist].emplace(name));
        } else {
            namelistentry = listentries->second.emplace(&appc, isgeneric);
            if (isdisplayed(appc, isgeneric)) { // we are currently displayed,
                                                // undisplay the old entry
                auto &orig = *std::prev(namelistentry);
                if (orig.isgeneric)
                    sortedentrymap.at(orig.app->hist)
                        .erase(orig.app->listed->generic_iter);
                else
                    sortedentrymap.at(orig.app->hist)
                        .erase(orig.app->listed->iter);
                if (sortedentrymap.at(orig.app->hist).empty())
                    sortedentrymap.erase(orig.app->hist);
            }
            histiter = getiter(sortedentrymap[appc.hist].emplace(name));
        }
    }

    void deletename_generic(appcontainer &appc, bool isgeneric) {
        const std::string &name =
            isgeneric ? appc.app.generic_name : appc.app.name;
        std::set<string_ref>::const_iterator &sortediter =
            isgeneric ? appc.listed->generic_iter : appc.listed->iter;
        const std::multiset<namelistentrytype>::const_iterator &listiter =
            isgeneric ? appc.listcandidate.generic_iter
                      : appc.listcandidate.iter;

        if (namelist.at(name).size() == 1) {
            sortedentrymap.at(appc.hist).erase(sortediter);
            if (sortedentrymap.at(appc.hist).empty())
                sortedentrymap.erase(appc.hist);
            namelist.erase(name); // delete the entire namelist entry rather
                                  // than having it with an empty list
        } else if (isdisplayed(namelist.at(name), listiter)) {
            sortedentrymap.at(appc.hist).erase(
                sortediter); // delete listed entry of name
            if (sortedentrymap.at(appc.hist).empty())
                sortedentrymap.erase(appc.hist);
            namelist.at(name).erase(listiter);
            auto &newentry = *std::prev(
                namelist.at(name)
                    .end()); // replace it with the next suitable entry
            if (!exclude_generic && newentry.isgeneric)
                newentry.app->listed->generic_iter =
                    getiter(sortedentrymap[newentry.app->hist].insert(
                        newentry.app->app.generic_name));
            else
                newentry.app->listed->iter =
                    getiter(sortedentrymap[newentry.app->hist].insert(
                        newentry.app->app.name));
        } else
            namelist.at(name).erase(listiter);
    }

    void deletename(appcontainer &todelete) {
        deletename_generic(todelete, false);
        if (!exclude_generic && !todelete.app.generic_name.empty())
            deletename_generic(todelete, true);
        sortedhistmap.at(todelete.hist).erase(todelete.listed);
        if (sortedhistmap.at(todelete.hist).empty())
            sortedhistmap.erase(todelete.hist);
    }

    void sethistory_generic(appcontainer &appc, bool isgeneric) {
        std::set<string_ref>::const_iterator &sortediter =
            isgeneric ? appc.listed->generic_iter : appc.listed->iter;
        std::multiset<namelistentrytype>::const_iterator &nameiter =
            isgeneric ? appc.listcandidate.generic_iter
                      : appc.listcandidate.iter;
        const std::string &name =
            isgeneric ? appc.app.generic_name : appc.app.name;

        nameiter = namelist[name].emplace(&appc, isgeneric);
        if (isdisplayed(appc, isgeneric)) {
            if (namelist.at(name).size() != 1) {
                const namelistentrytype &undisplay = *std::prev(nameiter);
                if (!exclude_generic && undisplay.isgeneric)
                    sortedentrymap.at(undisplay.app->hist)
                        .erase(undisplay.app->listed->generic_iter);
                else
                    sortedentrymap.at(undisplay.app->hist)
                        .erase(undisplay.app->listed->iter);
                if (sortedentrymap.at(undisplay.app->hist).empty())
                    sortedentrymap.erase(undisplay.app->hist);
            }

            sortediter = getiter(sortedentrymap[appc.hist].emplace(name));
        }
    }

    void sethistory(appcontainer &appc, unsigned newval) {
        deletename(appc);

        appc.hist = newval;

        appc.listed = sortedhistmap[newval].insert(sortedhistmap[newval].end(),
                                                   appc.app.id);

        sethistory_generic(appc, false);
        if (!exclude_generic && !appc.app.generic_name.empty())
            sethistory_generic(appc, true);
    }

    // This iterator crawls through sortedentrymap and mapped_type and presents
    // it continuously.
    class sortediter
    {
    private:
        decltype(sortedentrymap)::const_iterator topiter;
        std::set<string_ref>::const_iterator nameiter;
        // When topiter is incremented, we can't just set nameiter to
        // topiter->second.begin() because the newly incremented topiter might
        // be the past-the-end iterator and therefore undeferencable. When
        // topiter is incremented, isunknown is set to true and the next time
        // someone deferences or increments sortediter, we assume that topiter
        // is deferencable and set nameiter.
        bool isunknown = false;

    public:
        typedef std::ptrdiff_t difference_type;
        typedef std::string value_type;
        typedef const std::string *pointer;
        typedef const std::string &reference;
        typedef std::forward_iterator_tag iterator_category;

        reference operator*() const {
            if (isunknown)
                return topiter->second.begin()->get();
            return nameiter->get();
        }

        sortediter &operator++() {
            if (isunknown) {
                isunknown = false;
                nameiter = topiter->second.begin();
            }
            ++nameiter;
            if (topiter->second.end() == nameiter) {
                ++topiter;
                isunknown = true;
            }
            return *this;
        }

        bool operator==(const sortediter &other) const {
            return topiter == other.topiter &&
                   (isunknown == other.isunknown &&
                    (isunknown || nameiter == other.nameiter));
        }

        bool operator!=(const sortediter &other) const {
            return !(*this == other);
        }

        pointer operator->() const {
            return &operator*();
        }

        sortediter operator++(int) {
            sortediter ret = *this;
            operator++();
            return ret;
        }

        sortediter() = default;

        sortediter(decltype(topiter) t, decltype(nameiter) n)
            : topiter(t), nameiter(n) {}

        // This is the constructor for the past-the-end iterator of sortediter
        sortediter(decltype(topiter) t) : topiter(t), isunknown(true) {}
    };

public:
    Applications(application_formatter f, const stringlist_t &denvs,
                 const LocaleSuffixes &suff, bool exclude, bool history)
        : format(f), suffixes(suff), desktopenvs(denvs),
          exclude_generic(exclude), use_history(history) {}

    /// These functions and operators don't have much use in j4dd, but are
    /// useful for tests.

    Applications(const Applications &other)
        : format(other.format), suffixes(other.suffixes),
          desktopenvs(other.desktopenvs),
          exclude_generic(other.exclude_generic),
          use_history(other.use_history) {
        for (const auto &i : other.idmap) {
            auto &curr = *getiter(idmap.emplace(i));
            auto &appc = curr.second;

            namelist[appc.app.name].emplace(&appc, false);
            appc.listed = sortedhistmap[appc.hist].insert(
                sortedhistmap[appc.hist].end(), curr.first);
            if (other.isdisplayed(i.second, false))
                appc.listed->iter =
                    getiter(sortedentrymap[appc.hist].insert(appc.app.name));
            if (!exclude_generic && !appc.app.generic_name.empty()) {
                namelist[appc.app.generic_name].emplace(&appc, true);
                if (other.isdisplayed(i.second, true))
                    appc.listed->generic_iter =
                        getiter(sortedentrymap[appc.hist].insert(
                            appc.app.generic_name));
            }
        }
    }

    friend void swap(Applications &a, Applications &b) {
        using std::swap;

        swap(a.format, b.format);
        swap(a.suffixes, b.suffixes);
        swap(a.desktopenvs, b.desktopenvs);
        swap(a.exclude_generic, b.exclude_generic);
        swap(a.use_history, b.use_history);
        swap(a.idmap, b.idmap);
        swap(a.namelist, b.namelist);
        swap(a.sortedhistmap, b.sortedhistmap);
        swap(a.sortedentrymap, b.sortedentrymap);
    }

    Applications &operator=(Applications other) {
        swap(*this, other);

        return *this;
    }

    bool operator==(const Applications &other) const {
        return format == other.format && suffixes == other.suffixes &&
               desktopenvs == other.desktopenvs &&
               exclude_generic == other.exclude_generic &&
               use_history == other.use_history && idmap == other.idmap &&
               sortedentrymap == other.sortedentrymap;
    }

    ///

    ~Applications() {
        if (linesz != 0)
            free(linep);
    }

    sortediter begin() const {
        if (sortedentrymap.empty())
            return end();
        else
            return sortediter(sortedentrymap.cbegin(),
                              sortedentrymap.cbegin()->second.cbegin());
    }

    sortediter end() const {
        return sortediter(sortedentrymap.cend());
    }

    using size_type = decltype(idmap)::size_type;

    size_type size() const {
        return idmap.size();
    }

    void remove(const std::string &filename, int rank) {
        std::string id;
        id.reserve(filename.size());
        std::replace_copy(filename.cbegin(), filename.cend(),
                          std::back_inserter(id), '/', '-');

        auto appiter = idmap.find(id);
        if (appiter == idmap.end())
            throw std::runtime_error("Couldn't remove nonexistent item.");
        auto &app = appiter->second;
        if (app.rank != rank)
            return;

        deletename(app);
        idmap.erase(appiter);
    }

    // add or overwrite the file filename
    // providing a path and a filename instead of the absolute path is done to
    // be able to distinguish the desktop file ID
    void add(const std::string &filename, int rank, const std::string &path) {
        std::string id;
        id.reserve(filename.size());
        std::replace_copy(filename.cbegin(), filename.cend(),
                          std::back_inserter(id), '/', '-');

        auto tryfind = idmap.find(id);
        try {
            if (tryfind != idmap.end()) // application already exists
            {
                if (rank > tryfind->second.rank)
                    return;

                deletename(tryfind->second);

                tryfind->second.rank = rank;
                tryfind->second.app =
                    Application((path + filename).c_str(), &linep, &linesz,
                                format, suffixes, desktopenvs);
            } else
                tryfind = getiter(idmap.emplace(
                    std::piecewise_construct, std::forward_as_tuple(id),
                    std::forward_as_tuple(rank,
                                          Application((path + filename).c_str(),
                                                      &linep, &linesz, format,
                                                      suffixes, desktopenvs))));
        } catch (
            disabled_error &) // if the desktop file is disabled, do nothing
        {
            return;
        } catch (escape_error &) {
            return;
        }

        auto &appc = tryfind->second;

        appc.app.id = id;
        appc.listed =
            sortedhistmap[0].insert(sortedhistmap[0].end(), appc.app.id);

        add_generic(appc, false);
        if (!exclude_generic && !appc.app.generic_name.empty())
            add_generic(appc, true);
    }

    std::pair<Application *, std::string> get(const std::string &choice) {
        appcontainer *app;
        size_t match_length = 0;

        // Find longest match amongst apps
        for (const auto &current_app : namelist) {
            const auto &name = current_app.first;
            auto &currapp = std::prev(current_app.second.end())->app;

            if (name.size() > match_length && startswith(choice, name)) {
                app = currapp;
                match_length = name.length();
            }
        }

        if (match_length) {
            if (use_history)
                sethistory(*app, app->hist + 1);
            // +1 b/c there must be whitespace we add back later...
            return std::make_pair(
                &app->app, choice.substr(match_length, choice.length() - 1));
        }

        // No matching app found, return user dmenu choice
        return std::make_pair(nullptr, choice);
    }

    // If the log doesn't exist, than do nothing. A next update_log() call
    // should create it.
    void load_log(const char *log) {
        if (!use_history)
            return;
        FILE *fp = fopen(log, "r");
        if (!fp)
            return;

        unsigned count;
        char name[256];
        while (fscanf(fp, "%u,%255[^\n]\n", &count, name) == 2) {
            auto appc = idmap.find(name);
            if (appc == idmap.end())
                continue;

            sethistory(appc->second, count);
        }

        fclose(fp);
    }

    void update_log(const char *log) {
        if (!use_history)
            return;
        std::stringstream write_file;
        write_file << log << "." << getpid();
        FILE *fp = fopen(write_file.str().c_str(), "w");
        if (!fp) {
            fprintf(stderr, "Can't write usage log '%s': %s\n", log,
                    strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (auto x = sortedhistmap.crbegin();
             x != sortedhistmap.crend() && x->first != 0; ++x) {
            for (const auto &y : x->second) {
                if (fprintf(fp, "%u,%s\n", x->first, y.id.c_str()) < 0) {
                    perror("Write error");
                    fclose(fp);
                    return;
                }
            }
        }

        fclose(fp);

        if (rename(write_file.str().c_str(), log)) {
            perror("rename failed");
        }
    }
};

#endif
