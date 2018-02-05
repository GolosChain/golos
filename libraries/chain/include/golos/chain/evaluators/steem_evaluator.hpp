#pragma once

#include <golos/protocol/operations/witness_operations.hpp>

#include <golos/chain/evaluator.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class withdraw_vesting_evaluator : public evaluator<withdraw_vesting_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::withdraw_vesting_operation<Major, Hardfork, Release> operation_type;

            withdraw_vesting_evaluator(database &db) : evaluator<withdraw_vesting_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class set_withdraw_vesting_route_evaluator : public evaluator<
                set_withdraw_vesting_route_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::set_withdraw_vesting_route_operation<Major, Hardfork, Release> operation_type;

            set_withdraw_vesting_route_evaluator(database &db) : evaluator<
                    set_withdraw_vesting_route_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class comment_evaluator : public evaluator<comment_evaluator<Major, Hardfork, Release>, Major, Hardfork,
                Release> {
        public:
            typedef protocol::comment_operation<Major, Hardfork, Release> operation_type;

            comment_evaluator(database &db) : evaluator<comment_evaluator<Major, Hardfork, Release>, Major, Hardfork,
                    Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class comment_options_evaluator : public evaluator<comment_options_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::comment_options_operation<Major, Hardfork, Release> operation_type;

            comment_options_evaluator(database &db) : evaluator<comment_options_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class comment_payout_extension_evaluator : public evaluator<
                comment_payout_extension_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::comment_payout_extension_operation<Major, Hardfork, Release> operation_type;

            comment_payout_extension_evaluator(database &db) : evaluator<
                    comment_payout_extension_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class delete_comment_evaluator : public evaluator<delete_comment_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::delete_comment_operation<Major, Hardfork, Release> operation_type;

            delete_comment_evaluator(database &db) : evaluator<delete_comment_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::delete_comment_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class vote_evaluator : public evaluator<vote_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::vote_operation<Major, Hardfork, Release> operation_type;

            vote_evaluator(database &db) : evaluator<vote_evaluator<Major, Hardfork, Release>, Major, Hardfork,
                    Release>(db) {
            }

            void do_apply(const protocol::vote_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class pow_evaluator : public evaluator<pow_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::pow_operation<Major, Hardfork, Release> operation_type;

            pow_evaluator(database &db) : evaluator<pow_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(
                    db) {
            }

            void do_apply(const protocol::pow_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class pow2_evaluator : public evaluator<pow2_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::pow2_operation<Major, Hardfork, Release> operation_type;

            pow2_evaluator(database &db) : evaluator<pow2_evaluator<Major, Hardfork, Release>, Major, Hardfork,
                    Release>(db) {
            }

            void do_apply(const protocol::pow2_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class feed_publish_evaluator : public evaluator<feed_publish_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::feed_publish_operation<Major, Hardfork, Release> operation_type;

            feed_publish_evaluator(database &db) : evaluator<feed_publish_evaluator<Major, Hardfork, Release>, Major,
                    Hardfork, Release>(db) {
            }

            void do_apply(const protocol::feed_publish_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class report_over_production_evaluator : public evaluator<
                report_over_production_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::report_over_production_operation<Major, Hardfork, Release> operation_type;

            report_over_production_evaluator(database &db) : evaluator<
                    report_over_production_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::report_over_production_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class challenge_authority_evaluator : public evaluator<challenge_authority_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::challenge_authority_operation<Major, Hardfork, Release> operation_type;

            challenge_authority_evaluator(database &db) : evaluator<
                    challenge_authority_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::challenge_authority_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class prove_authority_evaluator : public evaluator<prove_authority_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::prove_authority_operation<Major, Hardfork, Release> operation_type;

            prove_authority_evaluator(database &db) : evaluator<prove_authority_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::prove_authority_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class request_account_recovery_evaluator : public evaluator<
                request_account_recovery_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::request_account_recovery_operation<Major, Hardfork, Release> operation_type;

            request_account_recovery_evaluator(database &db) : evaluator<
                    request_account_recovery_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::request_account_recovery_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class recover_account_evaluator : public evaluator<recover_account_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::recover_account_operation<Major, Hardfork, Release> operation_type;

            recover_account_evaluator(database &db) : evaluator<recover_account_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::recover_account_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class change_recovery_account_evaluator : public evaluator<
                change_recovery_account_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::change_recovery_account_operation<Major, Hardfork, Release> operation_type;

            change_recovery_account_evaluator(database &db) : evaluator<
                    change_recovery_account_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::change_recovery_account_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class decline_voting_rights_evaluator : public evaluator<
                decline_voting_rights_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::decline_voting_rights_operation<Major, Hardfork, Release> operation_type;

            decline_voting_rights_evaluator(database &db) : evaluator<
                    decline_voting_rights_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::decline_voting_rights_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class reset_account_evaluator : public evaluator<reset_account_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::reset_account_operation<Major, Hardfork, Release> operation_type;

            reset_account_evaluator(database &db) : evaluator<reset_account_evaluator<Major, Hardfork, Release>, Major,
                    Hardfork, Release>(db) {
            }

            void do_apply(const protocol::reset_account_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class set_reset_account_evaluator : public evaluator<set_reset_account_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::set_reset_account_operation<Major, Hardfork, Release> operation_type;

            set_reset_account_evaluator(database &db) : evaluator<set_reset_account_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::set_reset_account_operation<Major, Hardfork, Release> &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class delegate_vesting_shares_evaluator : public evaluator<
                delegate_vesting_shares_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::delegate_vesting_shares_operation<Major, Hardfork, Release> operation_type;

            delegate_vesting_shares_evaluator(database &db) : evaluator<
                    delegate_vesting_shares_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const protocol::delegate_vesting_shares_operation<Major, Hardfork, Release> &o);
        };
    }
} // golos::chain