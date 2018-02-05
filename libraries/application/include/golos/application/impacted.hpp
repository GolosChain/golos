#pragma once

#include <fc/container/flat.hpp>
#include <golos/protocol/operations/operations.hpp>
#include <golos/protocol/transaction.hpp>
#include <golos/chain/steem_object_types.hpp>

#include <fc/string.hpp>

namespace golos {
    namespace application {

        using namespace fc;

        void operation_get_impacted_accounts(
                const golos::protocol::operation &op,
                fc::flat_set<protocol::account_name_type> &result);

        void transaction_get_impacted_accounts(
                const golos::protocol::transaction &tx,
                fc::flat_set<protocol::account_name_type> &result
        );

    }
} // golos::application
