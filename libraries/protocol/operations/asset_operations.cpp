#include <string>

#include <golos/protocol/operations/asset_operations.hpp>

#include <fc/exception/exception.hpp>

namespace golos {
    namespace protocol {

/**
 *  Valid symbols can contain [A-Z0-9], and '.'
 *  They must start with [A, Z]
 *  They must end with [A, Z]
 *  They can contain a maximum of one '.'
 */
        bool is_valid_symbol(const std::string &symbol) {
            if (symbol.size() < STEEMIT_MIN_ASSET_SYMBOL_LENGTH) {
                return false;
            }

            if (symbol.substr(0, 3) == "BIT") {
                return false;
            }

            if (symbol.size() > STEEMIT_MAX_ASSET_SYMBOL_LENGTH) {
                return false;
            }

            if (!isalpha(symbol.front())) {
                return false;
            }

            if (!isalpha(symbol.back())) {
                return false;
            }

            bool dot_already_present = false;
            for (const auto c : symbol) {
                if ((isalpha(c) && isupper(c)) || isdigit(c)) {
                    continue;
                }

                if (c == '.') {
                    if (dot_already_present) {
                        return false;
                    }

                    dot_already_present = true;
                    continue;
                }

                return false;
            }

            return true;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_create_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(is_valid_symbol(asset_name));
            common_options.validate();
            if (common_options.issuer_permissions &
                (disable_force_settle | global_settle)) {
                FC_ASSERT(bitasset_opts.valid());
            }
            if (is_prediction_market) {
                FC_ASSERT(bitasset_opts.valid(), "Cannot have a User-Issued Asset implement a prediction market.");
                FC_ASSERT(common_options.issuer_permissions & global_settle);
            }
            if (bitasset_opts) {
                bitasset_opts->validate();
            }

            FC_ASSERT(precision <= 12);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_update_operation<Major, Hardfork, Release>::validate() const {
            if (new_issuer) {
                FC_ASSERT(issuer != *new_issuer);
            }
            new_options.validate();
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_publish_feed_operation<Major, Hardfork, Release>::validate() const {
            feed.validate();

            // maybe some of these could be moved to feed.validate()
            if (!feed.core_exchange_rate.is_null()) {
                feed.core_exchange_rate.validate();
            }
            if ((!feed.settlement_price.is_null()) &&
                (!feed.core_exchange_rate.is_null())) {
                FC_ASSERT(feed.settlement_price.base.symbol ==
                          feed.core_exchange_rate.base.symbol);
            }

            FC_ASSERT(!feed.settlement_price.is_null());
            FC_ASSERT(!feed.core_exchange_rate.is_null());
            FC_ASSERT(feed.is_for(asset_name));
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_reserve_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(amount_to_reserve.amount.value <=
                      STEEMIT_MAX_SHARE_SUPPLY);
            FC_ASSERT(amount_to_reserve.amount.value > 0);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_issue_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(asset_to_issue.amount.value <= STEEMIT_MAX_SHARE_SUPPLY);
            FC_ASSERT(asset_to_issue.amount.value > 0);
            FC_ASSERT(asset_to_issue.symbol_name() != STEEM_SYMBOL_NAME);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_fund_fee_pool_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(amount > 0);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_settle_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(amount.amount >= 0);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_force_settle_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(amount.amount >= 0);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_update_bitasset_operation<Major, Hardfork, Release>::validate() const {
            new_options.validate();
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_update_feed_producers_operation<Major, Hardfork, Release>::validate() const {
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_global_settle_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(asset_to_settle == settle_price.base.symbol_name());
        }

        void bitasset_options::validate() const {
            FC_ASSERT(minimum_feeds > 0);
            FC_ASSERT(force_settlement_offset_percent <= STEEMIT_100_PERCENT);
            FC_ASSERT(maximum_force_settlement_volume <= STEEMIT_100_PERCENT);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_options<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(max_supply > 0);
            FC_ASSERT(max_supply <= STEEMIT_MAX_SHARE_SUPPLY);
            FC_ASSERT(market_fee_percent <= STEEMIT_100_PERCENT);
            FC_ASSERT(max_market_fee >= 0 &&
                      max_market_fee <= STEEMIT_MAX_SHARE_SUPPLY);
            // There must be no high bits in permissions whose meaning is not known.
            FC_ASSERT(!(issuer_permissions & ~asset_issuer_permission_mask));
            // The global_settle flag may never be set (this is a permission only)
            FC_ASSERT(!(flags & global_settle));
            // the witness_fed and committee_fed flags cannot be set simultaneously
            FC_ASSERT((flags & (witness_fed_asset | committee_fed_asset)) !=
                      (witness_fed_asset | committee_fed_asset));
            core_exchange_rate.validate();
            FC_ASSERT(core_exchange_rate.base.symbol_name() == STEEM_SYMBOL_NAME ||
                      core_exchange_rate.quote.symbol_name() == STEEM_SYMBOL_NAME);

            if (!whitelist_authorities.empty() ||
                !blacklist_authorities.empty()) {
                FC_ASSERT(flags & white_list);
            }
            for (auto item : whitelist_markets) {
                FC_ASSERT(blacklist_markets.find(item) ==
                          blacklist_markets.end());
            }
            for (auto item : blacklist_markets) {
                FC_ASSERT(whitelist_markets.find(item) ==
                          whitelist_markets.end());
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_claim_fees_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(amount_to_claim.amount > 0);
        }
    }
} // namespace golos::chain

#include <golos/protocol/operations/asset_operations.tpp>