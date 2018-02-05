#ifndef GOLOS_TRANSFER_EVALUATOR_HPP
#define GOLOS_TRANSFER_EVALUATOR_HPP

#include <golos/protocol/operations/steem_operations.hpp>

#include <golos/chain/evaluator.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class transfer_evaluator : public evaluator<transfer_evaluator<Major, Hardfork, Release>, Major, Hardfork,
                Release> {
        public:
            typedef protocol::transfer_operation<Major, Hardfork, Release> operation_type;

            transfer_evaluator(database &db) : evaluator<transfer_evaluator<Major, Hardfork, Release>, Major, Hardfork,
                    Release>(db) {
            }

            void do_apply(const protocol::transfer_operation<Major, Hardfork, Release> &op);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class transfer_to_vesting_evaluator : public evaluator<transfer_to_vesting_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::transfer_to_vesting_operation<Major, Hardfork, Release> operation_type;

            transfer_to_vesting_evaluator(database &db) : evaluator<
                    transfer_to_vesting_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::transfer_to_vesting_operation<Major, Hardfork, Release> &op);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class transfer_to_savings_evaluator : public evaluator<transfer_to_savings_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::transfer_to_savings_operation<Major, Hardfork, Release> operation_type;

            transfer_to_savings_evaluator(database &db) : evaluator<
                    transfer_to_savings_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::transfer_to_savings_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class transfer_from_savings_evaluator : public evaluator<
                transfer_from_savings_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::transfer_from_savings_operation<Major, Hardfork, Release> operation_type;

            transfer_from_savings_evaluator(database &db) : evaluator<
                    transfer_from_savings_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::transfer_from_savings_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class cancel_transfer_from_savings_evaluator : public evaluator<
                cancel_transfer_from_savings_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::cancel_transfer_from_savings_operation<Major, Hardfork, Release> operation_type;

            cancel_transfer_from_savings_evaluator(database &db) : evaluator<
                    cancel_transfer_from_savings_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::cancel_transfer_from_savings_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class override_transfer_evaluator : public evaluator<override_transfer_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::override_transfer_operation<Major, Hardfork, Release> operation_type;

            override_transfer_evaluator(database &db) : evaluator<override_transfer_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::override_transfer_operation<Major, Hardfork, Release> &op);
        };
    }
}

#endif //GOLOS_TRANSFER_EVALUATOR_HPP
