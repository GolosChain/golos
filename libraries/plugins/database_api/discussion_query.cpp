

#include <golos/plugins/database_api/discussion_query.hpp>

namespace golos {
    namespace plugins {
        namespace database_api {

            void discussion_query::validate() const {
                FC_ASSERT(limit <= 100);

                for (const std::set<std::string>::value_type &iterator : filter_tags) {
                    FC_ASSERT(select_tags.find(iterator) == select_tags.end());
                }

                for (const auto &iterator : filter_languages) {
                    FC_ASSERT(select_languages.find(iterator) == select_languages.end());
                }
            }

        }
    }
}

