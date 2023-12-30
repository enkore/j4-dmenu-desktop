# AppManager
The AppManager class is a specialized container for the Application. The container supports the following specialized operations:

1. Initialize all Applications.

   The initialization happens in the constructor. It receives a list of .desktop file paths divided into [ranks](#ranks). It then traverses the list from the lowest rank to the highest rank to avoid collisions (this is further described in [collisions](#collisions)).
   1. The list of application names must not contain duplicates.

      Multiple different desktop files can have colliding names. For example many browsers have "Web Browser" set as their `GenericName`. Dmenu can not differentiate multiple entries which have the same text and the user can't too. This means that AppManager must handle colliding names. Note that this is not specified in the desktop entry specification.
2. Return a sorted list of application names.

   The list includes both `Name` and `GenericName` (if the user hasn't disabled generic names).

   The list can be sorted both case sensitively and insensitively. More options could be added in the future. The type of sorting can not be changed on the fly.
3. Additional "runtime" addition and removal of a desktop file.

   This feature is used in the inotify/kqueue runtime detection of new desktop files. See [optimisation](#optimisation).

   An application that is already in AppManager can be also "added". This operation is equivalent to removing and adding the desktop file.
4. Searching for a `Application` using (a part) of its name.

   This is one of the main features of AppManager. When the user selects the application name using a dmenu prompt, the name has to be resolved to the actual `Application` instance. This is a "reverse lookup".

   J4dd also supports running "raw" programs that do not have a desktop file. If the user specifies an app that is not in the list that is given to dmenu, j4dd tries to execute it. This means that j4dd can execute programs that do not have a desktop entry. But this isn't the main way j4dd is used. Note that this is **not** handled by AppManager. The caller handles this.

   J4dd supports specifying arguments to the desktop entry. The arguments correspond to the `%f, %F, %u` and `%U` field codes in the `Exec` key of the desktop file. Application has to decide where the name of the app ends and where the arguments begin.
5. Return the number of loaded desktop files.

   Due to collisions, not all desktop files supplied to the constructor can be loaded.

   This feature is used primarily for verbose output.

AppManager also supports looking up Application by its desktop file ID. This is used only for converting the old format of the history file to the new one. This operation will not be needed in a typical j4dd session.

# Collisions
There are two main types of collisions:

1. Desktop file ID collisions:
   1. When two colliding desktop files have a different rank, the one with a lower rank takes precedence.
   2. When two colliding desktop files have the same rank, the behaviour is implementation defined. J4dd chooses to leave the desktop file already present be and it omits the newer colliding desktop file.

      Exception: To facilitate the overwriting nature of the runtime addition operation (described in 3.), a new colliding desktop file **will** overwrite the already present one even when they have the same rank (**not** when the newer one has a higher rank). This is true **only** for the runtime addition operation, not for the constructor.
2. Desktop name collisions:

   Note that desktop name collisions are universal, a `Name` can collide with a `Name` of some other desktop file, `Name` can collide with `GenericName` etc.

   1. When two colliding names belong to desktop files with different ranks, the name whose desktop file has the lower rank takes precedence.
   1. When two colliding names belong to desktop files with the same rank, name which is already in the list takes precedence. This means that the newer name won't get added.

When j4dd has the choice, it chooses the "lazy" approach to handling collisions. This means that when j4dd has to choose between an app that is already in place and a new app, the old app is chosen.

Desktop file ID collisions are mandated by the standard. Desktop name collisions are specific to j4dd.
# Ranks
The standard specifies the following handling of desktop file ID collisions:

> Each desktop entry representing an application is identified by its _desktop file ID_, which is based on its filename.
>
> [...]
>
> If multiple files have the same desktop file ID, the first one in the `$XDG_DATA_DIRS` precedence order is used.

This means that the "parent" directory of the desktop file is important for determining precedence.

J4dd uses a "rank" to store this precedence information. If the desktop file is found in the first (zeroth) directory in `$XDG_DATA_DIRS`, it's rank is 0. If it's in the second directory in `$XDG_DATA_DIRS`, it's rank is 1.

The lowest rank is 0. Lower ranks take precedence. The zeroth rank has the highest priority.

# Optimisation
J4dd should be optimised for operations which are the most critical for the user. These are the specialized operations 1, 2 and 4. The operation 3, the runtime addition of desktop files, is not the primary target for optimisation. In the current implementation, data structures and algorithm have been chosen according to this.

# History
History management is handled outside of AppManager.