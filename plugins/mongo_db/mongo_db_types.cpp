#include <golos/plugins/mongo_db/mongo_db_types.hpp>

namespace golos {
namespace plugins {
namespace mongo_db {

    void bmi_insert_or_replace(db_map& bmi, named_document doc) {
        for (auto it = bmi.begin(); it != bmi.end(); ++it)
        {
            if (it->collection_name == doc.collection_name
                && it->key == doc.key
                && it->keyval == doc.keyval
                && it->is_removal == doc.is_removal)
            {
                bmi.erase(it);
                break;
            }
        }
        bmi.push_back(doc);
    }

    void bmi_merge(db_map& bmi_what, const db_map& bmi_with) {
        for (auto it = bmi_with.begin(); it != bmi_with.end(); ++it) {
            // Adding
            for (auto it2 = bmi_what.begin(); it2 != bmi_what.end(); ++it2)
            {
                if (it2->collection_name == it->collection_name
                    && it2->key == it->key
                    && it2->keyval == it->keyval
                    && it2->is_removal == it->is_removal)
                {
                    bmi_what.erase(it2);
                    break;
                }
            }
            bmi_what.push_back(*it);
        }
    }
}
}
}