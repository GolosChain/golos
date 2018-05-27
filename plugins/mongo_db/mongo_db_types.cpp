#include <golos/plugins/mongo_db/mongo_db_types.hpp>

namespace golos {
namespace plugins {
namespace mongo_db {

    void bmi_insert_or_replace(db_map& bmi, named_document doc) {
        auto it = bmi.get<hashed_idx>().find(std::make_tuple<std::string, std::string, std::string, bool>(
            std::string(doc.collection_name),
            std::string(doc.key), std::string(doc.keyval), bool(doc.is_removal)));
        if (it != bmi.get<hashed_idx>().end())
            bmi.get<hashed_idx>().erase(it);
        bmi.push_back(doc);
    }

    void bmi_merge(db_map& bmi_what, const db_map& bmi_with) {
        for (auto it = bmi_with.begin(); it != bmi_with.end(); ++it) {
            // Erasing
            auto it2 = bmi_what.get<hashed_idx>().find(std::make_tuple<std::string, std::string, std::string, bool>(
                std::string(it->collection_name),
                std::string(it->key), std::string(it->keyval), bool(it->is_removal)));
            if (it2 != bmi_what.get<hashed_idx>().end())
                bmi_what.get<hashed_idx>().erase(it2);
            // Adding
            bmi_what.push_back(*it);
        }
    }
}
}
}