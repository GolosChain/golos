#ifndef GOLOS_ESCROW_EVALUATOR_HPP
#define GOLOS_ESCROW_EVALUATOR_HPP

#include <golos/protocol/operations/escrow_operations.hpp>

#include <golos/chain/evaluator.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class escrow_transfer_evaluator : public evaluator<escrow_transfer_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::escrow_transfer_operation<Major, Hardfork, Release> operation_type;

            escrow_transfer_evaluator(database &db)
                    : evaluator<escrow_transfer_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class escrow_approve_evaluator
                : public evaluator<escrow_approve_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::escrow_approve_operation<Major, Hardfork, Release> operation_type;

            escrow_approve_evaluator(database &db)
                    : evaluator<escrow_approve_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class escrow_dispute_evaluator : public evaluator<escrow_dispute_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::escrow_dispute_operation<Major, Hardfork, Release> operation_type;

            escrow_dispute_evaluator(database &db)
                    : evaluator<escrow_dispute_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class escrow_release_evaluator : public evaluator<escrow_release_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::escrow_release_operation<Major, Hardfork, Release> operation_type;

            escrow_release_evaluator(database &db)
                    : evaluator<escrow_release_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };
    }
}

#endif //GOLOS_ESCROW_EVALUATOR_HPP
