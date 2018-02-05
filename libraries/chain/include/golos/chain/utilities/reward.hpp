#pragma once

#include <golos/chain/utilities/asset.hpp>
#include <golos/chain/objects/steem_objects.hpp>

#include <golos/protocol/asset.hpp>
#include <golos/protocol/config.hpp>
#include <golos/protocol/types.hpp>

#include <fc/reflect/reflect.hpp>

#include <fc/uint128.hpp>

namespace golos {
    namespace chain {
        namespace utilities {
            using fc::uint128_t;

            struct comment_reward_context {
                protocol::share_type rshares;
                uint16_t reward_weight = 0;
                protocol::asset<0, 17, 0> max_sbd;
                uint128_t total_reward_shares2;
                protocol::asset<0, 17, 0> total_reward_fund_steem;
                protocol::price<0, 17, 0> current_steem_price;
            };

            uint64_t get_rshare_reward(const comment_reward_context &ctx);

            uint64_t get_rshare_reward(const comment_reward_context &ctx, const reward_fund_object &rf);

            uint64_t get_vote_weight(uint64_t vote_rshares, const reward_fund_object &rf);

            inline uint128_t get_content_constant_s() {
                return uint128_t(uint64_t(2000000000000ull)); // looking good for posters
            }

            uint128_t calculate_claims(const uint128_t &rshares);

            uint128_t calculate_claims(const uint128_t &rshares, const reward_fund_object &rf);

            inline bool is_comment_payout_dust(const protocol::price<0, 17, 0> &p, uint64_t steem_payout) {
                return to_sbd(p, protocol::asset<0, 17, 0>(steem_payout, STEEM_SYMBOL_NAME)) < STEEMIT_MIN_PAYOUT_SBD;
            }
        }
    }
}

FC_REFLECT((golos::chain::utilities::comment_reward_context),
           (rshares)(reward_weight)(max_sbd)(total_reward_shares2)(total_reward_fund_steem)(current_steem_price))