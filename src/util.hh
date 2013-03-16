
#include <cstdlib>
#include <map>
#include <list>
#include <string>

typedef std::map<std::string, std::string> apps_t;
typedef std::list<std::string> stringlist_t;

stringlist_t &split(const std::string &s, char delimiter, stringlist_t &elems);
std::string &replace(std::string& str, const std::string& substr, const std::string &substitute);

bool is_directory(const std::string &path);
std::string getenv(const std::string &var);

void build_search_path(stringlist_t &search_path);