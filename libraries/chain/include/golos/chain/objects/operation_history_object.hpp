#pragma once

#include <golos/chain/steem_object_types.hpp>

#include <golos/protocol/operations/operations.hpp>

namespace golos {
    namespace chain {

        /**
         * @brief tracks the history of all logical operations on blockchain state
         * @ingroup object
         * @ingroup implementation
         *
         *  All operations and virtual operations result in the creation of an
         *  operation_history_object that is maintained on disk as a stack.  Each
         *  real or virtual operation is assigned a unique ID / sequence number that
         *  it can be referenced by.
         *
         *  @note  by default these objects are not tracked, the account_history_plugin must
         *  be loaded fore these objects to be maintained.
         *
         *  @note  this object is READ ONLY it can never be modified
         */
        class operation_history_object
                : public object<operation_history_object_type, operation_history_object> {
        public:
            operation_history_object() = delete;

            template<typename Constructor, typename Allocator>
            operation_history_object(Constructor &&c, allocator<Allocator> a) {
                c(*this);
            };

            id_type id;

            protocol::operation op;
            /** the block that caused this operation */
            uint32_t block_num = 0;
            /** the transaction in the block */
            uint16_t trx_in_block = 0;
            /** the operation within the transaction */
            uint16_t op_in_trx = 0;
            /** any virtual operations implied by operation in block */
            uint16_t virtual_op = 0;
        };

        /**
         *  @brief a node in a linked list of operation_history_objects
         *  @ingroup implementation
         *  @ingroup object
         *
         *  Account history is important for users and wallets even though it is
         *  not part of "core validation".   Account history is maintained as
         *  a linked list stored on disk in a stack.  Each account will point to the
         *  most recent account history object by ID.  When a new operation relativent
         *  to that account is processed a new account history object is allcoated at
         *  the end of the stack and intialized to point to the prior object.
         *
         *  This data is never accessed as part of chain validation and therefore
         *  can be kept on disk as a memory mapped file.  Using a memory mapped file
         *  will help the operating system better manage / cache / page files and
         *  also accelerates load time.
         *
         *  When the transaction history for a particular account is requested the
         *  linked list can be traversed with relatively effecient disk access because
         *  of the use of a memory mapped stack.
         */
        class account_transaction_history_object
                : public object<account_transaction_history_object_type, account_transaction_history_object> {
        public:
            account_transaction_history_object() = delete;

            template<typename Constructor, typename Allocator>
            account_transaction_history_object(Constructor &&c, allocator<Allocator> a) {
                c(*this);
            };

            id_type id;

            account_name_type account; /// the account this operation applies to
            operation_history_object::id_type operation_id;
            uint32_t sequence = 0; /// the operation position within the given account
            account_transaction_history_object::id_type next;

            //std::pair<account_name_type,operation_history_object::id_type>  account_op()const  { return std::tie( account, operation_id ); }
            //std::pair<account_name_type,uint32_t>                   account_seq()const { return std::tie( account, sequence );     }
        };

        struct by_id;
        struct by_seq;
        struct by_op;
        struct by_opid;
        typedef multi_index_container<
                account_transaction_history_object,
                indexed_by<
                        ordered_unique<tag<by_id>,
                                member<account_transaction_history_object, account_transaction_history_object::id_type, &account_transaction_history_object::id>>,
                        ordered_unique<tag<by_seq>,
                                composite_key<account_transaction_history_object,
                                        member<
                                                account_transaction_history_object, account_name_type, &account_transaction_history_object::account>,
                                        member<account_transaction_history_object, uint32_t, &account_transaction_history_object::sequence>
                                >
                        >,
                        ordered_unique<tag<by_op>,
                                composite_key<account_transaction_history_object,
                                        member<
                                                account_transaction_history_object, account_name_type, &account_transaction_history_object::account>,
                                        member<account_transaction_history_object, operation_history_object::id_type, &account_transaction_history_object::operation_id>
                                >
                        >,
                        ordered_non_unique<tag<by_opid>,
                                member<account_transaction_history_object, operation_history_object::id_type, &account_transaction_history_object::operation_id>
                        >
                >
        > account_transaction_history_index;
    }
}

FC_REFLECT((golos::chain::operation_history_object), (id)(op)(block_num)(trx_in_block)(op_in_trx)(virtual_op));

FC_REFLECT((golos::chain::account_transaction_history_object), (id)(account)(operation_id)(sequence)(next));
CHAINBASE_SET_INDEX_TYPE(golos::chain::account_transaction_history_object, golos::chain::account_transaction_history_index);
