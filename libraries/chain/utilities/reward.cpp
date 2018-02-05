#include <golos/chain/utilities/reward.hpp>
#include <golos/chain/utilities/uint256.hpp>

namespace golos {
    namespace chain {
        namespace utilities {

            uint64_t get_rshare_reward(const comment_reward_context &ctx) {
                try {
                    FC_ASSERT(ctx.rshares > 0);
                    FC_ASSERT(ctx.total_reward_shares2 > 0);

                    boost::multiprecision::uint256_t rs(ctx.rshares.value);
                    boost::multiprecision::uint256_t rf(ctx.total_reward_fund_steem.amount.value);
                    boost::multiprecision::uint256_t total_rshares2 = to256(ctx.total_reward_shares2);

                    boost::multiprecision::uint256_t rs2 = to256(calculate_claims(ctx.rshares.value));
                    rs2 = (rs2 * ctx.reward_weight) / STEEMIT_100_PERCENT;

                    boost::multiprecision::uint256_t payout_u256 = (rf * rs2) / total_rshares2;
                    FC_ASSERT(payout_u256 <=
                              boost::multiprecision::uint256_t(uint64_t(std::numeric_limits<int64_t>::max())));
                    uint64_t payout = static_cast< uint64_t >( payout_u256 );

                    if (is_comment_payout_dust(ctx.current_steem_price, payout)) {
                        payout = 0;
                    }

                    protocol::asset<0, 17, 0> max_steem = to_steem(ctx.current_steem_price, ctx.max_sbd);

                    payout = std::min(payout, uint64_t(max_steem.amount.value));

                    return payout;
                } FC_CAPTURE_AND_RETHROW((ctx))
            }

            uint64_t get_rshare_reward(const comment_reward_context &ctx, const reward_fund_object &rf_object) {
                try {
                    FC_ASSERT(ctx.rshares > 0);
                    FC_ASSERT(ctx.total_reward_shares2 > 0);

                    boost::multiprecision::uint256_t rs(ctx.rshares.value);
                    boost::multiprecision::uint256_t rf(ctx.total_reward_fund_steem.amount.value);
                    boost::multiprecision::uint256_t total_rshares2 = to256(ctx.total_reward_shares2);

                    //idump( (ctx) );

                    boost::multiprecision::uint256_t rs2 = to256(calculate_claims(ctx.rshares.value, rf_object));
                    rs2 = (rs2 * ctx.reward_weight) / STEEMIT_100_PERCENT;

                    boost::multiprecision::uint256_t payout_u256 = (rf * rs2) / total_rshares2;
                    FC_ASSERT(payout_u256 <=
                              boost::multiprecision::uint256_t(uint64_t(std::numeric_limits<int64_t>::max())));
                    uint64_t payout = static_cast< uint64_t >( payout_u256 );

                    if (is_comment_payout_dust(ctx.current_steem_price, payout)) {
                        payout = 0;
                    }

                    protocol::asset<0, 17, 0> max_steem = to_steem(ctx.current_steem_price, ctx.max_sbd);

                    payout = std::min(payout, uint64_t(max_steem.amount.value));

                    return payout;
                } FC_CAPTURE_AND_RETHROW((ctx))
            }

            uint64_t get_vote_weight(uint64_t vote_rshares, const reward_fund_object &rf) {
                uint64_t result = 0;
                if (rf.name == STEEMIT_POST_REWARD_FUND_NAME || rf.name == STEEMIT_COMMENT_REWARD_FUND_NAME) {
                    uint128_t two_alpha = rf.content_constant * 2;
                    result = (uint128_t(vote_rshares, 0) / (two_alpha + vote_rshares)).to_uint64();
                } else {
                    wlog("Unknown reward fund type ${rf}", ("rf", rf.name));
                }

                return result;
            }

            uint128_t calculate_claims(const uint128_t &rshares) {
                uint128_t s = get_content_constant_s();
                uint128_t rshares_plus_s = rshares + s;
                return rshares_plus_s * rshares_plus_s - s * s;
            }

            uint128_t calculate_claims(const uint128_t &rshares, const reward_fund_object &rf) {
                uint128_t result = 0;
                if (rf.name == STEEMIT_POST_REWARD_FUND_NAME || rf.name == STEEMIT_COMMENT_REWARD_FUND_NAME) {
                    uint128_t s = rf.content_constant;
                    uint128_t rshares_plus_s = rshares + s;
                    result = rshares_plus_s * rshares_plus_s - s * s;
                } else {
                    wlog("Unknown reward fund type ${rf}", ("rf", rf.name));
                }

                return result;
            }
        }
    }
}