#pragma once

#include <golos/protocol/base.hpp>
#include <golos/protocol/types.hpp>

#include <fc/reflect/reflect.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace golos {
    namespace private_message {

        struct private_message_operation : public golos::protocol::base_operation<0, 17, 0> {
            protocol::account_name_type from;
            protocol::account_name_type to;
            protocol::public_key_type from_memo_key;
            protocol::public_key_type to_memo_key;
            uint64_t sent_time = 0; /// used as seed to secret generation
            uint32_t checksum = 0;
            std::vector<char> encrypted_message;
        };

        typedef fc::static_variant<private_message_operation> private_message_plugin_operation;

    }
}

FC_REFLECT((golos::private_message::private_message_operation),
           (from)(to)(from_memo_key)(to_memo_key)(sent_time)(checksum)(encrypted_message))

namespace fc {

    void to_variant(const golos::private_message::private_message_plugin_operation &, fc::variant &);

    void from_variant(const fc::variant &, golos::private_message::private_message_plugin_operation &);

} /* fc */

namespace golos {
    namespace protocol {

        void operation_validate(const private_message::private_message_plugin_operation &o);

        void operation_get_required_authorities(const private_message::private_message_plugin_operation &op,
                                                flat_set <account_name_type> &active,
                                                flat_set <account_name_type> &owner,
                                                flat_set <account_name_type> &posting, vector <authority> &other);

    }
}

FC_REFLECT_TYPENAME((golos::private_message::private_message_plugin_operation))
