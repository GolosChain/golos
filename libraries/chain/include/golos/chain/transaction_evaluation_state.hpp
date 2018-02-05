#pragma once

#include <golos/protocol/operations/operations.hpp>
#include <golos/protocol/transaction.hpp>

namespace golos {
    namespace chain {
        class database;

        /**
         *  Place holder for state tracked while processing a transaction. This class provides helper methods that are
         *  common to many different operations and also tracks which keys have signed the transaction
         */
        class transaction_evaluation_state {
        public:
            transaction_evaluation_state(database *db = nullptr)
                    : _db(db) {
            }


            database &db() const {
                assert(_db);
                return *_db;
            }

            const protocol::signed_transaction *_trx = nullptr;
            database *_db = nullptr;
            bool _is_proposed_trx = false;
            bool skip_fee = false;
            bool skip_fee_schedule_check = false;
        };
    }
} // namespace golos::chain