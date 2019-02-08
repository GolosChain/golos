#pragma once

#include <golos/chain/witness_objects.hpp>
#include <golos/chain/database.hpp>

namespace golos { namespace api {
    using golos::protocol::asset;
    using golos::chain::chain_properties;
    using golos::chain::database;

    struct chain_api_properties {
        chain_api_properties(const chain_properties&, const database&);
        chain_api_properties() = default;

        asset account_creation_fee;
        uint32_t maximum_block_size;
        uint16_t sbd_interest_rate;

        fc::optional<asset> create_account_min_golos_fee;
        fc::optional<asset> create_account_min_delegation;
        fc::optional<uint32_t> create_account_delegation_time;
        fc::optional<asset> min_delegation;

        fc::optional<uint16_t> max_referral_interest_rate;
        fc::optional<uint32_t> max_referral_term_sec;
        fc::optional<asset> min_referral_break_fee;
        fc::optional<asset> max_referral_break_fee;

        fc::optional<uint16_t> posts_window;
        fc::optional<uint16_t> posts_per_window;
        fc::optional<uint16_t> comments_window;
        fc::optional<uint16_t> comments_per_window;
        fc::optional<uint16_t> votes_window;
        fc::optional<uint16_t> votes_per_window;

        fc::optional<uint32_t> auction_window_size;

        fc::optional<uint16_t> max_delegated_vesting_interest_rate;

        fc::optional<uint16_t> custom_ops_bandwidth_multiplier;

        fc::optional<uint16_t> min_curation_percent;
        fc::optional<uint16_t> max_curation_percent;

        fc::optional<protocol::curation_curve> curation_reward_curve;

        fc::optional<bool> allow_distribute_auction_reward;
        fc::optional<bool> allow_return_auction_reward_to_fund;

        fc::optional<uint16_t> worker_from_content_fund_percent;
        fc::optional<uint16_t> worker_from_vesting_fund_percent;
        fc::optional<uint16_t> worker_from_witness_fund_percent;
        fc::optional<uint32_t> worker_techspec_approve_term_sec;
        fc::optional<uint32_t> worker_result_approve_term_sec;
    };

} } // golos::api

FC_REFLECT(
    (golos::api::chain_api_properties),
    (account_creation_fee)(maximum_block_size)(sbd_interest_rate)
    (create_account_min_golos_fee)(create_account_min_delegation)
    (create_account_delegation_time)(min_delegation)
    (max_referral_interest_rate)(max_referral_term_sec)(min_referral_break_fee)(max_referral_break_fee)
    (posts_window)(posts_per_window)(comments_window)(comments_per_window)(votes_window)(votes_per_window)(auction_window_size)
    (max_delegated_vesting_interest_rate)(custom_ops_bandwidth_multiplier)
    (min_curation_percent)(max_curation_percent)(curation_reward_curve)
    (allow_distribute_auction_reward)(allow_return_auction_reward_to_fund)
    (worker_from_content_fund_percent)(worker_from_vesting_fund_percent)(worker_from_witness_fund_percent)
    (worker_techspec_approve_term_sec)(worker_result_approve_term_sec))
