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
            min_referral_break_fee = src.min_referral_break_fee;
            max_referral_break_fee = src.max_referral_break_fee;
            posts_window = src.posts_window;
            posts_per_window = src.posts_per_window;
            comments_window = src.comments_window;
            comments_per_window = src.comments_per_window;
            votes_window = src.votes_window;
            votes_per_window = src.votes_per_window;
            auction_window_size = src.auction_window_size;
            max_delegated_vesting_interest_rate = src.max_delegated_vesting_interest_rate;
            custom_ops_bandwidth_multiplier = src.custom_ops_bandwidth_multiplier;
            min_curation_percent = src.min_curation_percent;
            max_curation_percent = src.max_curation_percent;
            curation_reward_curve = src.curation_reward_curve;
            allow_distribute_auction_reward = src.allow_distribute_auction_reward;
            allow_return_auction_reward_to_fund = src.allow_return_auction_reward_to_fund;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_22)) {
            worker_from_content_fund_percent = src.worker_from_content_fund_percent;
            worker_from_vesting_fund_percent = src.worker_from_vesting_fund_percent;
            worker_from_witness_fund_percent = src.worker_from_witness_fund_percent;
            worker_techspec_approve_term_sec = src.worker_techspec_approve_term_sec;
            worker_result_approve_term_sec = src.worker_result_approve_term_sec;
            min_vote_author_promote_rate = src.min_vote_author_promote_rate;
            max_vote_author_promote_rate = src.max_vote_author_promote_rate;
        }
    }

} } // golos::api
