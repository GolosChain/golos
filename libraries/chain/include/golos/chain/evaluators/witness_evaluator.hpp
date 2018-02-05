#ifndef GOLOS_WITNESS_EVALUATOR_HPP
#define GOLOS_WITNESS_EVALUATOR_HPP

#include <golos/chain/evaluator.hpp>
#include <golos/protocol/operations/witness_operations.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class witness_update_evaluator : public evaluator<witness_update_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::witness_update_operation<Major, Hardfork, Release> operation_type;

            witness_update_evaluator(database &db) : evaluator<witness_update_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class account_witness_vote_evaluator : public evaluator<
                account_witness_vote_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::account_witness_vote_operation<Major, Hardfork, Release> operation_type;

            account_witness_vote_evaluator(database &db) : evaluator<
                    account_witness_vote_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class account_witness_proxy_evaluator : public evaluator<
                account_witness_proxy_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::account_witness_proxy_operation<Major, Hardfork, Release> operation_type;

            account_witness_proxy_evaluator(database &db) : evaluator<
                    account_witness_proxy_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };
    }
}

#endif //GOLOS_WITNESS_EVALUATOR_HPP
