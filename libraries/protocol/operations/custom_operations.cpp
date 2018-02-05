#include <golos/protocol/operations/custom_operations.hpp>

#include <fc/utf8.hpp>
#include <fc/io/json.hpp>

namespace golos {
    namespace protocol {
        void custom_operation::validate() const {
            /// required auth accounts are the ones whose bandwidth is consumed
            FC_ASSERT(required_auths.size() > 0, "at least on account must be specified");
        }

        void custom_json_operation::validate() const {
            /// required auth accounts are the ones whose bandwidth is consumed
            FC_ASSERT((required_auths.size() + required_posting_auths.size()) > 0,
                      "at least on account must be specified");
            FC_ASSERT(id.size() <= 32, "id is too long");
            FC_ASSERT(fc::is_utf8(json), "JSON Metadata not formatted in UTF8");
            FC_ASSERT(fc::json::is_valid(json), "JSON Metadata not valid JSON");
        }

        void custom_binary_operation::validate() const {
            /// required auth accounts are the ones whose bandwidth is consumed
            FC_ASSERT((required_owner_auths.size() + required_active_auths.size() + required_posting_auths.size()) > 0,
                      "at least on account must be specified");
            FC_ASSERT(id.size() <= 32, "id is too long");
            for (const auto &a : required_auths) {
                a.validate();
            }
        }
    }
}