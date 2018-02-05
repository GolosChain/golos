#include <string>

namespace fc {
    std::string name_from_type(const std::string &type_name) {
        auto start = type_name.find_last_of(':') + 1;
        return type_name.substr(start, type_name.size() - start);
    }
} // fc
