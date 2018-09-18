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
        fc::optional<asset> max_referral_break_fee;
        fc::optional<uint32_t> auction_window_size;
    };

} } // golos::api

FC_REFLECT(
    (golos::api::chain_api_properties),
    (account_creation_fee)(maximum_block_size)(sbd_interest_rate)
    (create_account_min_golos_fee)(create_account_min_delegation)
    (create_account_delegation_time)(min_delegation)
    (max_referral_interest_rate)(max_referral_term_sec)(max_referral_break_fee)(auction_window_size))
