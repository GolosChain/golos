#pragma once

#include <golos/protocol/operations/operations.hpp>

#include <golos/chain/steem_object_types.hpp>
#include <golos/chain/objects/history_object.hpp>

namespace golos {
    namespace application {

        struct applied_operation {
            applied_operation();

            applied_operation(const golos::chain::operation_object &op_obj);

            golos::protocol::transaction_id_type trx_id;
            uint32_t block = 0;
            uint32_t trx_in_block = 0;
            uint16_t op_in_trx = 0;
            uint64_t virtual_op = 0;
            fc::time_point_sec timestamp;
            golos::protocol::operation op;
        };

    }
}

FC_REFLECT((golos::application::applied_operation),
           (trx_id)(block)(trx_in_block)(op_in_trx)(virtual_op)(timestamp)(op))
