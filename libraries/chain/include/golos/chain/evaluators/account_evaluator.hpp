#ifndef GOLOS_ACCOUNT_EVALUATOR_HPP
#define GOLOS_ACCOUNT_EVALUATOR_HPP

#include <golos/chain/evaluator.hpp>

#include <golos/protocol/operations/account_operations.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class account_create_evaluator : public evaluator<account_create_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::account_create_operation<Major, Hardfork, Release> operation_type;

            account_create_evaluator(database &db) : evaluator<account_create_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::account_create_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class account_create_with_delegation_evaluator : public evaluator<
                account_create_with_delegation_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::account_create_with_delegation_operation<Major, Hardfork, Release> operation_type;

            account_create_with_delegation_evaluator(database &db) : evaluator<
                    account_create_with_delegation_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::account_create_with_delegation_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class account_update_evaluator : public evaluator<account_update_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::account_update_operation<Major, Hardfork, Release> operation_type;

            account_update_evaluator(database &db) : evaluator<account_update_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::account_update_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class account_whitelist_evaluator : public evaluator<account_whitelist_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::account_whitelist_operation<Major, Hardfork, Release> operation_type;

            account_whitelist_evaluator(database &db) : evaluator<account_whitelist_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::account_whitelist_operation<Major, Hardfork, Release> &o);
        };
    }
}

#endif //GOLOS_ACCOUNT_EVALUATOR_HPP