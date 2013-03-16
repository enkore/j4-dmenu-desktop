
#include "util.hh"

int main(int argc, char **argv)
{
    stringlist_t search_path;

    build_search_path(search_path);

    for(auto path : search_path)
        std::printf("%s\n", path.c_str());

}
