#pragma once

#include <golos/chain/evaluator.hpp>

#include <golos/protocol/operations/market_operations.hpp>
#include <golos/protocol/types.hpp>

namespace golos {
    namespace chain {

        class account_object;

        class asset_object;

        class asset_bitasset_data_object;

        class call_order_object;

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class convert_evaluator : public golos::chain::evaluator<convert_evaluator<Major, Hardfork, Release>, Major,
                Hardfork, Release> {
        public:
            typedef protocol::convert_operation<Major, Hardfork, Release> operation_type;

            convert_evaluator(database &db) : golos::chain::evaluator<convert_evaluator<Major, Hardfork, Release>,
                    Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release, typename = type_traits::static_range<true>>
        class limit_order_create_evaluator : public evaluator<limit_order_create_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_create_operation<Major, Hardfork, Release> operation_type;

            limit_order_create_evaluator(database &db) : evaluator<
                    limit_order_create_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class limit_order_create_evaluator<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>> : public evaluator<limit_order_create_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_create_operation<Major, Hardfork, Release> operation_type;

            limit_order_create_evaluator(database &db) : evaluator<
                    limit_order_create_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class limit_order_create_evaluator<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>> : public evaluator<limit_order_create_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_create_operation<Major, Hardfork, Release> operation_type;

            limit_order_create_evaluator(database &db) : evaluator<
                    limit_order_create_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }

            void do_apply(const operation_type &o);

        protected:
            share_type deferred_fee = 0;
            const account_object *seller = nullptr;
            const asset_object *sell_asset = nullptr;
            const asset_object *receive_asset = nullptr;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release, typename = type_traits::static_range<true>>
        class limit_order_create2_evaluator : public golos::chain::evaluator<
                limit_order_create2_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_create2_operation<Major, Hardfork, Release> operation_type;

            limit_order_create2_evaluator(database &db) : golos::chain::evaluator<
                    limit_order_create2_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class limit_order_create2_evaluator<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>> : public golos::chain::evaluator<
                limit_order_create2_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_create2_operation<Major, Hardfork, Release> operation_type;

            limit_order_create2_evaluator(database &db) : golos::chain::evaluator<
                    limit_order_create2_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);

        protected:
            share_type deferred_fee = 0;
            const account_object *seller = nullptr;
            const asset_object *sell_asset = nullptr;
            const asset_object *receive_asset = nullptr;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class limit_order_create2_evaluator<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>> : public golos::chain::evaluator<
                limit_order_create2_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_create2_operation<Major, Hardfork, Release> operation_type;

            limit_order_create2_evaluator(database &db) : golos::chain::evaluator<
                    limit_order_create2_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {
            }

            void do_apply(const operation_type &o);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release, typename = type_traits::static_range<true>>
        class limit_order_cancel_evaluator : public evaluator<limit_order_cancel_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_cancel_operation<Major, Hardfork, Release> operation_type;

            limit_order_cancel_evaluator(database &db) : evaluator<
                    limit_order_cancel_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class limit_order_cancel_evaluator<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>> : public evaluator<limit_order_cancel_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_cancel_operation<Major, Hardfork, Release> operation_type;

            limit_order_cancel_evaluator(database &db) : evaluator<
                    limit_order_cancel_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }

            void do_apply(const operation_type &op);

        protected:
            const limit_order_object *_order;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class limit_order_cancel_evaluator<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>> : public evaluator<limit_order_cancel_evaluator<Major, Hardfork, Release>,
                Major, Hardfork, Release> {
        public:
            typedef protocol::limit_order_cancel_operation<Major, Hardfork, Release> operation_type;

            limit_order_cancel_evaluator(database &db) : evaluator<
                    limit_order_cancel_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }

            void do_apply(const operation_type &op);
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class call_order_update_evaluator : public evaluator<call_order_update_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::call_order_update_operation<Major, Hardfork, Release> operation_type;

            call_order_update_evaluator(database &db) : evaluator<call_order_update_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }

            void do_apply(const operation_type &op);

        protected:
            const asset_object *_debt_asset = nullptr;
            const account_object *_paying_account = nullptr;
            const call_order_object *_order = nullptr;
            const asset_bitasset_data_object *_bitasset_data = nullptr;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class bid_collateral_evaluator : public evaluator<bid_collateral_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release> {
        public:
            typedef protocol::bid_collateral_operation<Major, Hardfork, Release> operation_type;

            bid_collateral_evaluator(database &db) : evaluator<bid_collateral_evaluator<Major, Hardfork, Release>, Major, Hardfork, Release>(db) {

            }

            void do_apply(const operation_type &o);

            const collateral_bid_object *_bid = nullptr;
        };
    }
} // golos::chain