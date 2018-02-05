#include <golos/chain/objects/asset_object.hpp>
#include <golos/chain/database.hpp>

#include <fc/uint128.hpp>

#include <cmath>

namespace golos {
    namespace chain {
        share_type asset_bitasset_data_object::max_force_settlement_volume(share_type current_supply) const {
            if (options.maximum_force_settlement_volume == 0) {
                return 0;
            }
            if (options.maximum_force_settlement_volume ==
                STEEMIT_100_PERCENT) {
                return current_supply + force_settled_volume;
            }

            fc::uint128 volume =
                    current_supply.value + force_settled_volume.value;
            volume *= options.maximum_force_settlement_volume;
            volume /= STEEMIT_100_PERCENT;
            return volume.to_uint64();
        }

        void asset_bitasset_data_object::update_median_feeds(time_point_sec current_time) {
            current_feed_publication_time = current_time;
            vector<std::reference_wrapper<const price_feed<0, 17, 0>>> current_feeds;
            for (const pair<account_name_type, pair<time_point_sec, price_feed<0, 17, 0>>> &f : feeds) {
                if ((current_time - f.second.first).to_seconds() <
                    options.feed_lifetime_sec &&
                    f.second.first != time_point_sec()) {
                    current_feeds.emplace_back(f.second.second);
                    current_feed_publication_time = std::min(current_feed_publication_time, f.second.first);
                }
            }

            // If there are no valid feeds, or the number available is less than the minimum to calculate a median...
            if (current_feeds.size() < options.minimum_feeds) {
                //... don't calculate a median, and set a null feed
                current_feed_publication_time = current_time;
                current_feed = price_feed<0, 17, 0>();
                return;
            }
            if (current_feeds.size() == 1) {
                current_feed = std::move(current_feeds.front());
                return;
            }

            // *** Begin Median Calculations ***
            price_feed<0, 17, 0> median_feed;
            const auto median_itr =
                    current_feeds.begin() + current_feeds.size() / 2;
#define CALCULATE_MEDIAN_VALUE(r, data, field_name) \
   std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(), \
                     [](const price_feed<0, 17, 0>& a, const price_feed<0, 17, 0>& b) { \
      return a.field_name < b.field_name; \
   }); \
   median_feed.field_name = median_itr->get().field_name;

            BOOST_PP_SEQ_FOR_EACH(CALCULATE_MEDIAN_VALUE, ~, (settlement_price)(maintenance_collateral_ratio)(maximum_short_squeeze_ratio)(core_exchange_rate))
#undef CALCULATE_MEDIAN_VALUE
            // *** End Median Calculations ***

            current_feed = median_feed;
        }


        asset<0, 17, 0> asset_object::amount_from_string(string amount_string) const {
            try {
                bool negative_found = false;
                bool decimal_found = false;
                for (const char c : amount_string) {
                    if (isdigit(c)) {
                        continue;
                    }

                    if (c == '-' && !negative_found) {
                        negative_found = true;
                        continue;
                    }

                    if (c == '.' && !decimal_found) {
                        decimal_found = true;
                        continue;
                    }

                    FC_THROW((amount_string));
                }

                share_type satoshis = 0;

                share_type scaled_precision = asset<0, 17, 0>::scaled_precision(precision);

                const auto decimal_pos = amount_string.find('.');
                const string lhs = amount_string.substr(negative_found, decimal_pos);
                if (!lhs.empty()) {
                    satoshis += fc::safe<int64_t>(std::stoll(lhs)) *= scaled_precision;
                }

                if (decimal_found) {
                    const size_t max_rhs_size = std::to_string(scaled_precision.value).substr(1).size();

                    string rhs = amount_string.substr(decimal_pos + 1);
                    FC_ASSERT(rhs.size() <= max_rhs_size);

                    while (rhs.size() < max_rhs_size) {
                        rhs += '0';
                    }

                    if (!rhs.empty()) {
                        satoshis += std::stoll(rhs);
                    }
                }

                FC_ASSERT(satoshis <= STEEMIT_MAX_SHARE_SUPPLY);

                if (negative_found) {
                    satoshis *= -1;
                }

                return amount(satoshis);
            } FC_CAPTURE_AND_RETHROW((amount_string))
        }

        string asset_object::amount_to_string(share_type amount) const {
            share_type scaled_precision = 1;
            for (uint8_t i = 0; i < precision; ++i) {
                scaled_precision *= 10;
            }
            assert(scaled_precision > 0);

            string result = fc::to_string(
                    amount.value / scaled_precision.value);
            auto decimals = amount.value % scaled_precision.value;
            if (decimals) {
                result += "." +
                          fc::to_string(
                                  scaled_precision.value +
                                  decimals).erase(0, 1);
            }
            return result;
        }
    }
}
