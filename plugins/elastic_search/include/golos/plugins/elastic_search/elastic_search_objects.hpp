#pragma once

#include <golos/protocol/steem_operations.hpp>
#include <golos/protocol/operations.hpp>
#include <golos/chain/steem_object_types.hpp>


namespace golos {
namespace plugins {
namespace elastic_search {

    using namespace golos::protocol;
    using namespace golos::chain;

    class operation_history_object
            : public object<operation_history_object_type, operation_history_object>
    {
    public:
        operation_history_object() {}
        operation_history_object(operation& o) : op(o) {}

        operation op;
        uint32_t block = 0;
        uint16_t trx_in_block = 0;
        uint16_t op_in_trx = 0;
        uint16_t virtual_op = 0;
    };

    class account_transaction_history_object :
            public object<account_transaction_history_object_type, account_transaction_history_object>
    {
    public:
        account_name_type account;
        uint32_t sequence;
    };

    struct operation_visitor
    {
        typedef void result_type;

        share_type transfer_amount;
        account_name_type transfer_from;
        account_name_type transfer_to;

        void operator()(const transfer_operation& o )
        {
            transfer_amount = o.amount.amount;
            transfer_from = o.from;
            transfer_to = o.to;
        }
    };

    struct operation_history_struct {
        int trx_in_block;
        int op_in_trx;
        std::string operation_result;
        int virtual_op;
        std::string op;
    };

    struct block_struct {
        int block_num;
        fc::time_point_sec block_time;
        std::string trx_id;
    };

    struct transfer_struct {
        //asset_id_type asset;
        share_type amount;
        account_name_type from;
        account_name_type to;
    };

    struct visitor_struct {
        transfer_struct transfer_data;
    };

    struct bulk_struct {
        account_transaction_history_object account_history;
        operation_history_struct operation_history;
        int operation_type;
        block_struct block_data;
        visitor_struct additional_data;
    };

}}}


FC_REFLECT((golos::plugins::elastic_search::operation_history_struct), (trx_in_block)(op_in_trx)(operation_result)(virtual_op)(op) )
FC_REFLECT((golos::plugins::elastic_search::block_struct), (block_num)(block_time)(trx_id) )
FC_REFLECT((golos::plugins::elastic_search::transfer_struct), (amount)(from)(to) )
FC_REFLECT((golos::plugins::elastic_search::visitor_struct), (transfer_data) )
FC_REFLECT((golos::plugins::elastic_search::bulk_struct), (account_history)(operation_history)(operation_type)(block_data)(additional_data) )

