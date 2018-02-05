#pragma once

#include <golos/chain/steem_object_types.hpp>

#include <golos/protocol/operations/asset_operations.hpp>
#include <golos/protocol/asset.hpp>

/**
 * @defgroup prediction_market Prediction Market
 *
 * A prediction market is a specialized BitAsset such that total debt and total collateral are always equal amounts
 * (although asset IDs differ). No margin calls or force settlements may be performed on a prediction market asset. A
 * prediction market is globally settled by the issuer after the event being predicted resolves, thus a prediction
 * market must always have the @ref global_settle permission enabled. The maximum price for global settlement or short
 * sale of a prediction market asset is 1-to-1.
 */

namespace golos {
    namespace chain {
        class account_object;

        class database;

        /**
         *  @brief tracks the asset information that changes frequently
         *  @ingroup object
         *  @ingroup implementation
         *
         *  Because the asset_object is very large it doesn't make sense to save an undo state
         *  for all of the parameters that never change.   This object factors out the parameters
         *  of an asset that change in almost every transaction that involves the asset.
         *
         *  This object exists as an implementation detail and its ID should never be referenced by
         *  a blockchain operation.
         */
        class asset_dynamic_data_object
                : public object<asset_dynamic_data_object_type, asset_dynamic_data_object> {
        public:
            template<typename Constructor, typename Allocator>
            asset_dynamic_data_object(Constructor &&c, allocator<Allocator> a) {
                c(*this);
            }

            asset_dynamic_data_object() {
            }

            id_type id;

            /// Ticker symbol for this asset, i.e. "USD"
            protocol::asset_name_type asset_name;

            /// The number of shares currently in existence
            share_type current_supply;
            share_type confidential_supply; ///< total asset held in confidential balances
            share_type accumulated_fees; ///< fees accumulate to be paid out over time
            share_type fee_pool;         ///< in core asset
        };

        /**
         *  @brief tracks the parameters of an asset
         *  @ingroup object
         *
         *  All assets have a globally unique symbol name that controls how they are traded and an issuer who
         *  has authority over the parameters of the asset.
         */
        class asset_object
                : public object<asset_object_type, asset_object> {
        public:
            template<typename Constructor, typename Allocator>
            asset_object(Constructor &&c, allocator<Allocator> a) {
                c(*this);
            }

            asset_object() {
            }

            id_type id;

            /// This function does not check if any registered asset has this symbol or not; it simply checks whether the
            /// symbol would be valid.
            /// @return true if symbol is a valid ticker symbol; false otherwise.
            static bool is_valid_symbol(const string &symbol);

            /// @return true if this is a market-issued asset; false otherwise.
            bool is_market_issued() const {
                return market_issued;
            }

            /// @return true if users may request force-settlement of this market-issued asset; false otherwise
            bool can_force_settle() const {
                return !(options.flags & protocol::disable_force_settle);
            }

            /// @return true if the issuer of this market-issued asset may globally settle the asset; false otherwise
            bool can_global_settle() const {
                return options.issuer_permissions & protocol::global_settle;
            }

            /// @return true if this asset charges a fee for the issuer on market operations; false otherwise
            bool charges_market_fees() const {
                return options.flags & protocol::charge_market_fee;
            }

            /// @return true if this asset may only be transferred to/from the issuer or market orders
            bool is_transfer_restricted() const {
                return options.flags & protocol::transfer_restricted;
            }

            bool can_override() const {
                return options.flags & protocol::override_authority;
            }

            bool allow_confidential() const {
                return !(options.flags &
                         protocol::asset_issuer_permission_flags::disable_confidential);
            }

            /// Helper function to get an asset object with the given amount in this asset's type
            protocol::asset<0, 17, 0> amount(share_type a) const {
                return protocol::asset<0, 17, 0>(a, asset_name);
            }

            /// Convert a string amount (i.e. "123.45") to an asset object with this asset's type
            /// The string may have a decimal and/or a negative sign.
            protocol::asset<0, 17, 0> amount_from_string(string amount_string) const;

            /// Convert an asset to a textual representation, i.e. "123.45"
            string amount_to_string(share_type amount) const;

            /// Convert an asset to a textual representation, i.e. "123.45"
            string amount_to_string(const protocol::asset<0, 17, 0> &amount) const {
                FC_ASSERT(amount.symbol == asset_name);
                return amount_to_string(amount.amount);
            }

            /// Convert an asset to a textual representation with symbol, i.e. "123.45 USD"
            string amount_to_pretty_string(share_type amount) const {
                return amount_to_string(amount) + " " + asset_name;
            }

            /// Convert an asset to a textual representation with symbol, i.e. "123.45 USD"
            string amount_to_pretty_string(const protocol::asset<0, 17, 0> &amount) const {
                FC_ASSERT(amount.symbol == asset_name);
                return amount_to_pretty_string(amount.amount);
            }

            /// Ticker symbol for this asset, i.e. "USD"
            protocol::asset_name_type asset_name;
            /// Maximum number of digits after the decimal point (must be <= 12)
            uint8_t precision = 0;
            /// ID of the account which issued this asset.
            account_name_type issuer;

            protocol::asset_options<0, 17, 0> options;

            /// Extra data associated with BitAssets. This field is non-null if and only if is_market_issued() returns true
            bool market_issued = false;

            optional<account_name_type> buyback_account;

            void validate() const {
                // UIAs may not be prediction markets, have force settlement, or global settlements
                if (!is_market_issued()) {
                    FC_ASSERT(!(
                            options.flags & protocol::disable_force_settle ||
                            options.flags & protocol::global_settle));
                    FC_ASSERT(!(
                            options.issuer_permissions &
                            protocol::disable_force_settle ||
                            options.issuer_permissions &
                            protocol::global_settle));
                }
            }
        };

        /**
         *  @brief contains properties that only apply to bitassets (market issued assets)
         *
         *  @ingroup object
         *  @ingroup implementation
         */
        class asset_bitasset_data_object
                : public object<asset_bitasset_data_object_type, asset_bitasset_data_object> {
        public:
            template<typename Constructor, typename Allocator>
            asset_bitasset_data_object(Constructor &&c, allocator<Allocator> a) {
                c(*this);
            }

            asset_bitasset_data_object() {

            }

            id_type id;

            /// Ticker symbol for this asset, i.e. "USD"
            protocol::asset_name_type asset_name;

            /// The tunable options for BitAssets are stored in this field.
            protocol::bitasset_options options;

            /// Feeds published for this asset. If issuer is not committee, the keys in this map are the feed publishing
            /// accounts; otherwise, the feed publishers are the currently active committee_members and witnesses and this map
            /// should be treated as an implementation detail. The timestamp on each feed is the time it was published.
            flat_map<account_name_type, pair<time_point_sec, protocol::price_feed<0, 17, 0>>> feeds;
            /// This is the currently active price feed, calculated as the median of values from the currently active
            /// feeds.
            protocol::price_feed<0, 17, 0> current_feed;
            /// This is the publication time of the oldest feed which was factored into current_feed.
            time_point_sec current_feed_publication_time;

            /// True if this asset implements a @ref prediction_market
            bool is_prediction_market = false;

            /// This is the volume of this asset which has been force-settled this maintanence interval
            share_type force_settled_volume;

            /// Calculate the maximum force settlement volume per maintenance interval, given the current share supply
            share_type max_force_settlement_volume(share_type current_supply) const;

            /** return true if there has been a black swan, false otherwise */
            bool has_settlement() const {
                return !settlement_price.is_null();
            }

            /**
             *  In the event of a black swan, the swan price is saved in the settlement price, and all margin positions
             *  are settled at the same price with the siezed collateral being moved into the settlement fund. From this
             *  point on no further updates to the asset are permitted (no feeds, etc) and forced settlement occurs
             *  immediately when requested, using the settlement price and fund.
             */
            ///@{
            /// Price at which force settlements of a black swanned asset will occur
            protocol::price<0, 17, 0> settlement_price;
            /// Amount of collateral which is available for force settlement
            share_type settlement_fund;
            ///@}

            time_point_sec feed_expiration_time() const {
                return current_feed_publication_time +
                       options.feed_lifetime_sec;
            }

            bool feed_is_expired(time_point_sec current_time) const {
                return feed_expiration_time() <= current_time;
            }

            void update_median_feeds(time_point_sec current_time);
        };

        struct by_feed_expiration;
        struct by_asset_name;
        struct by_type;
        struct by_issuer;

        typedef multi_index_container<
                asset_bitasset_data_object,
                indexed_by<ordered_unique<tag<by_id>,
                        member<asset_bitasset_data_object, asset_bitasset_data_object::id_type, &asset_bitasset_data_object::id>>,
                        ordered_non_unique<tag<by_feed_expiration>,
                                const_mem_fun<asset_bitasset_data_object, time_point_sec, &asset_bitasset_data_object::feed_expiration_time>
                        >,
                        ordered_unique<tag<by_asset_name>,
                                member<asset_bitasset_data_object, protocol::asset_name_type, &asset_bitasset_data_object::asset_name>>
                >, allocator<asset_bitasset_data_object>
        > asset_bitasset_data_index;

        typedef multi_index_container<
                asset_dynamic_data_object,
                indexed_by<
                        ordered_unique<tag<
                                by_id>, member<asset_dynamic_data_object, asset_dynamic_data_object::id_type, &asset_dynamic_data_object::id>>,
                        ordered_unique<tag<
                                by_asset_name>, member<asset_dynamic_data_object, protocol::asset_name_type, &asset_dynamic_data_object::asset_name>>
                >, allocator<asset_dynamic_data_object>
        > asset_dynamic_data_index;

        typedef multi_index_container<
                asset_object,
                indexed_by<
                        ordered_unique<tag<
                                by_id>, member<asset_object, asset_object::id_type, &asset_object::id>>,
                        ordered_unique<tag<by_asset_name>, member<asset_object, protocol::asset_name_type, &asset_object::asset_name>>,
                        ordered_non_unique<tag<by_issuer>, member<asset_object, account_name_type, &asset_object::issuer>>,
                        ordered_unique<tag<by_type>,
                                composite_key<asset_object,
                                        const_mem_fun<asset_object, bool, &asset_object::is_market_issued>,
                                        member<asset_object, asset_object::id_type, &asset_object::id>
                                >
                        >
                >, allocator<asset_object>
        > asset_index;
    }
} // golos::chain

FC_REFLECT((golos::chain::asset_dynamic_data_object),
        (id)(asset_name)(current_supply)(confidential_supply)(accumulated_fees)(fee_pool))
CHAINBASE_SET_INDEX_TYPE(golos::chain::asset_dynamic_data_object, golos::chain::asset_dynamic_data_index)

FC_REFLECT((golos::chain::asset_bitasset_data_object),
        (id)
                (asset_name)
                (feeds)
                (current_feed)
                (current_feed_publication_time)
                (options)
                (force_settled_volume)
                (is_prediction_market)
                (settlement_price)
                (settlement_fund)
)

CHAINBASE_SET_INDEX_TYPE(golos::chain::asset_bitasset_data_object, golos::chain::asset_bitasset_data_index)

FC_REFLECT((golos::chain::asset_object),
        (id)
                (asset_name)
                (precision)
                (issuer)
                (options)
                (buyback_account)
                (market_issued)
)

CHAINBASE_SET_INDEX_TYPE(golos::chain::asset_object, golos::chain::asset_index)