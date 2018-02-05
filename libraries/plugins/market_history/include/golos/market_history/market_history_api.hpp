#pragma once

#include <golos/market_history/market_history_plugin.hpp>

#include <golos/protocol/types.hpp>

#include <golos/chain/objects/market_object.hpp>

#include <golos/application/state.hpp>

#include <fc/api.hpp>

namespace golos {
    namespace application {
        struct api_context;
    }
}

namespace golos {
    namespace market_history {

        using chain::share_type;
        using fc::time_point_sec;

        namespace detail {
            class market_history_api_impl;
        }

        struct market_ticker {
            string base;
            string quote;
            double latest;
            double lowest_ask;
            double highest_bid;
            double percent_change;
            double base_volume;
            double quote_volume;
        };

        struct market_volume {
            string base;
            string quote;
            double base_volume;
            double quote_volume;
        };

        struct order {
            double price;
            double quote;
            double base;
        };

        struct order_book {
            string base;
            string quote;
            std::vector<order> bids;
            std::vector<order> asks;
        };

        struct market_trade {
            fc::time_point_sec date;
            double price;
            double amount;
            double value;
            account_name_type side1_account_name;
            account_name_type side2_account_name;
        };

        struct liquidity_balance {
            std::string account;
            fc::uint128_t weight;
        };

        class market_history_api {
        public:
            market_history_api(const golos::application::api_context &ctx);

            void on_api_startup();

            ///////////////////
            // Subscriptions //
            ///////////////////

            void set_subscribe_callback(std::function<void(const variant &)> cb, bool clear_filter);

            void set_pending_transaction_callback(std::function<void(const variant &)> cb);

            void set_block_applied_callback(std::function<void(const variant &block_id)> cb);

            /**
             * @brief Stop receiving any notifications
             *
             * This unsubscribes from all subscribed markets and objects.
             */
            void cancel_all_subscriptions();

            /**
             * @brief Request notification when the active orders in the market between two assets changes
             * @param callback Callback method which is called when the market changes
             * @param a First asset ID
             * @param b Second asset ID
             *
             * Callback will be passed a variant containing a vector<pair<operation, operation_result>>. The vector will
             * contain, in order, the operations which changed the market, and their results.
             */
            void subscribe_to_market(std::function<void(const variant &)> callback, const string &a, const string &b);

            /**
             * @brief Unsubscribe from updates to a given market
             * @param a First asset ID
             * @param b Second asset ID
             */
            void unsubscribe_from_market(const string &a, const string &b);

            std::vector<golos::application::extended_limit_order> get_limit_orders_by_owner(const string &owner) const;

            std::vector<call_order_object> get_call_orders_by_owner(const string &owner) const;

            std::vector<force_settlement_object> get_settle_orders_by_owner(const string &owner) const;

            /**
             * @brief Returns the ticker for the market assetA:assetB
             * @param a String name of the first asset
             * @param b String name of the second asset
             * @return The market ticker for the past 24 hours.
             */
            market_ticker get_ticker(const string &base, const string &quote) const;

            /**
             * @brief Returns the 24 hour volume for the market assetA:assetB
             * @param a String name of the first asset
             * @param b String name of the second asset
             * @return The market volume over the past 24 hours
             */
            market_volume get_volume(const string &base, const string &quote) const;

            /**
             * @brief Returns the order book for the market base:quote
             * @param base String name of the first asset
             * @param quote String name of the second asset
             * @param depth of the order book. Up to depth of each asks and bids, capped at 50. Prioritizes most moderate of each
             * @return Order book of the market
             */
            order_book get_order_book(const string &base, const string &quote, unsigned limit = 50) const;

            /**
             * @brief Returns recent trades for the market assetA:assetB
             * Note: Currently, timezone offsets are not supported. The time must be UTC.
             * @param a String name of the first asset
             * @param b String name of the second asset
             * @param stop Stop time as a UNIX timestamp
             * @param limit Number of transactions to retrieve, capped at 100
             * @param start Start time as a UNIX timestamp
             * @return Recent transactions in the market
             */
            std::vector<market_trade> get_trade_history(const string &base, const string &quote,
                                                        fc::time_point_sec start, fc::time_point_sec stop,
                                                        unsigned limit = 100) const;

            vector<order_history_object> get_fill_order_history(const string &a, const string &b, uint32_t limit) const;

            /**
             * @brief Returns the market history for the internal SBD:STEEM market.
             * @param bucket_seconds The size of buckets the history is broken into. The bucket size must be configured in the plugin options.
             * @param start The start time to get market history.
             * @param end The end time to get market history
             * @return A list of market history buckets.
             */
            vector<bucket_object> get_market_history(const string &a, const string &b, uint32_t bucket_seconds,
                                                     fc::time_point_sec start, fc::time_point_sec end) const;

            /**
             * @brief Returns the bucket seconds being tracked by the plugin.
             */
            flat_set<uint32_t> get_market_history_buckets() const;

            /**
             * @brief Get limit orders in a given market
             * @param a ID of asset being sold
             * @param b ID of asset being purchased
             * @param limit Maximum number of orders to retrieve
             * @return The limit orders, ordered from least price to greatest
             */
            vector<limit_order_object> get_limit_orders(const string &a, const string &b, uint32_t limit) const;

            /**
             * @brief Get call orders in a given asset
             * @param a ID of asset being called
             * @param limit Maximum number of orders to retrieve
             * @return The call orders, ordered from earliest to be called to latest
             */
            vector<call_order_object> get_call_orders(const string &a, uint32_t limit) const;

            /**
             * @brief Get forced settlement orders in a given asset
             * @param a ID of asset being settled
             * @param limit Maximum number of orders to retrieve
             * @return The settle orders, ordered from earliest settlement date to latest
             */
            vector<force_settlement_object> get_settle_orders(const string &a, uint32_t limit) const;

            /**
             * @brief Get collateral_bid_objects for a given asset
             * @param a ID of asset
             * @param limit Maximum number of objects to retrieve
             * @param start skip that many results
             * @return The settle orders, ordered from earliest settlement date to latest
             */
            vector<collateral_bid_object> get_collateral_bids(const asset_name_type asset, uint32_t limit,
                                                              uint32_t start) const;

            /**
             *  @return all open margin positions for a given account id.
             */
            vector<call_order_object> get_margin_positions(const string &name) const;

            /**
             * @breif Gets the current liquidity reward queue.
             * @param start_account The account to start the list from, or "" to get the head of the queue
             * @param limit Maxmimum number of accounts to return -- Must not exceed 1000
             */
            std::vector<liquidity_balance> get_liquidity_queue(const string &start_account,
                                                               uint32_t limit = 1000) const;

        private:
            std::shared_ptr<detail::market_history_api_impl> my;
        };

    }
} // golos::market_history

FC_REFLECT((golos::market_history::order), (price)(quote)(base));
FC_REFLECT((golos::market_history::order_book), (asks)(bids)(base)(quote));
FC_REFLECT((golos::market_history::market_ticker),
           (base)(quote)(latest)(lowest_ask)(highest_bid)(percent_change)(base_volume)(quote_volume));
FC_REFLECT((golos::market_history::market_volume), (base)(quote)(base_volume)(quote_volume));
FC_REFLECT((golos::market_history::market_trade), (date)(price)(amount)(value)(side1_account_name)(side2_account_name));

FC_REFLECT((golos::market_history::liquidity_balance), (account)(weight));

FC_API(golos::market_history::market_history_api,
// Subscriptions
       (set_subscribe_callback)(set_pending_transaction_callback)(set_block_applied_callback)(cancel_all_subscriptions)

               //Market
               (get_ticker)(get_volume)(get_order_book)(get_trade_history)(get_market_history)(
               get_market_history_buckets)(get_limit_orders)(get_call_orders)(get_settle_orders)(get_margin_positions)(
               get_liquidity_queue)(subscribe_to_market)(unsubscribe_from_market)(get_limit_orders_by_owner)(
               get_call_orders_by_owner)(get_settle_orders_by_owner)(get_fill_order_history)(get_collateral_bids));