#include <golos/api/chain_api_properties.hpp>

namespace golos { namespace api {

    chain_api_properties::chain_api_properties(
        const chain_properties& src,
        const database& db
    ) : account_creation_fee(src.account_creation_fee),
        maximum_block_size(src.maximum_block_size),
        sbd_interest_rate(src.sbd_interest_rate)
    {
        if (db.has_hardfork(STEEMIT_HARDFORK_0_18__673)) {
            create_account_min_golos_fee = src.create_account_min_golos_fee;
            create_account_min_delegation = src.create_account_min_delegation;
            create_account_delegation_time = src.create_account_delegation_time;
            min_delegation = src.min_delegation;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_19)) {
            max_referral_interest_rate = src.max_referral_interest_rate;
            max_referral_term_sec = src.max_referral_term_sec;
            max_referral_break_fee = src.max_referral_break_fee;
            auction_window_size = src.auction_window_size;
        }
    }

} } // golos::api
