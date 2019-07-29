#pragma once

#include <map>

#include <fc/uint128_t.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/curation_info.hpp>

namespace golos { namespace chain {

    class comment_fund final {
    public:
        comment_fund(database& db):
            db_(db)
        {
            auto& gpo = db.get_dynamic_global_properties();
            reward_shares_ = gpo.total_reward_shares2;
            reward_fund_ = gpo.total_reward_fund_steem;
            vesting_shares_ = gpo.total_vesting_shares;
            vesting_fund_ = gpo.total_vesting_fund_steem;
            worker_fund_ = gpo.total_worker_fund_steem;

            process_funds();
        }

        const fc::uint128_t& reward_shares() const {
            return reward_shares_;
        }

        const asset& reward_fund() const {
            return reward_fund_;
        }

        const asset& vesting_shares() const {
            return vesting_shares_;
        }

        const asset& vesting_fund() const {
            return vesting_fund_;
        }

        const asset& worker_fund() const {
            return worker_fund_;
        }

        price get_vesting_share_price() const {
            return price(vesting_shares_, vesting_fund_);
        }

        asset claim_comment_reward(const comment_object& comment) {
            auto vshares = db_.calculate_vshares(comment.net_rshares.value);
            BOOST_REQUIRE_LE(vshares.to_uint64(), reward_shares_.to_uint64());

            auto reward = vshares * reward_fund_.amount.value / reward_shares_;
            BOOST_REQUIRE_LE(reward.to_uint64(), static_cast<uint64_t>(reward_fund_.amount.value));

            reward_shares_ -= vshares;
            reward_fund_ -= asset(reward.to_uint64(), reward_fund_.symbol);

            return asset(reward.to_uint64(), STEEM_SYMBOL);
        }

        asset create_vesting(asset value) {
            asset vesting = value * price(vesting_shares_, vesting_fund_);
            vesting_shares_ += vesting;
            vesting_fund_ += value;
            return vesting;
        }

        void modify_reward_fund(asset& value) {
            reward_fund_ += value;
        }

    private:
        void process_funds() {
            int64_t inflation_rate = STEEMIT_INFLATION_RATE_START_PERCENT;
            int64_t supply = db_.get_dynamic_global_properties().virtual_supply.amount.value;

            auto total_reward = (supply * inflation_rate) / (int64_t(STEEMIT_100_PERCENT) * STEEMIT_BLOCKS_PER_YEAR);
            auto content_reward = (total_reward * uint16_t(STEEMIT_CONTENT_REWARD_PERCENT)) / STEEMIT_100_PERCENT;
            auto vesting_reward = total_reward * uint16_t(STEEMIT_VESTING_FUND_PERCENT) / STEEMIT_100_PERCENT;
            auto witness_reward = total_reward - content_reward - vesting_reward;

            if (db_.has_hardfork(STEEMIT_HARDFORK_0_22__1013)) {
                auto content_to_worker = content_reward * uint16_t(GOLOS_WORKER_FROM_CONTENT_FUND_PERCENT) / STEEMIT_100_PERCENT;
                content_reward -= content_to_worker;

                auto vesting_to_worker = vesting_reward * uint16_t(GOLOS_WORKER_FROM_VESTING_FUND_PERCENT) / STEEMIT_100_PERCENT;
                vesting_reward -= vesting_to_worker;

                auto witness_to_worker = witness_reward * uint16_t(GOLOS_WORKER_FROM_WITNESS_FUND_PERCENT) / STEEMIT_100_PERCENT;
                witness_reward -= witness_to_worker;

                worker_fund_ += asset(content_to_worker + vesting_to_worker + witness_to_worker, STEEM_SYMBOL);
            }

            auto witness_normalize = db_.get_witness_schedule_object().witness_pay_normalization_factor;
            witness_reward = witness_reward * STEEMIT_MAX_WITNESSES / witness_normalize;

            vesting_fund_ += asset(vesting_reward, STEEM_SYMBOL);
            reward_fund_ += asset(content_reward, STEEM_SYMBOL);

            create_vesting(asset(witness_reward, STEEM_SYMBOL));
        }
        
        database& db_;
        fc::uint128_t reward_shares_;
        asset reward_fund_;
        asset vesting_shares_;
        asset vesting_fund_;
        asset worker_fund_;
    };

    class comment_reward final {
    public:
        comment_reward(
            database& db,
            comment_fund& fund,
            const account_name_type& author,
            const std::string& permlink
        ): comment_reward(db, fund, comment_reward::get_comment(db, author, permlink)) {
        }

        comment_reward(
            database& db,
            comment_fund& fund,
            const comment_object& comment
        ):
            fund_(fund),
            db_(db),
            comment_(comment)
        {
            calculate_rewards();
            calculate_vote_payouts();
            calculate_beneficiaries_payout();
            calculate_comment_payout();
        }

        asset total_vote_payouts() const {
            return total_vote_payouts_;
        }

        int64_t total_curators_reward() const {
            return total_vote_rewards_;
        }

        int64_t total_author_reward() const {
            return comment_rewards_;
        }

        asset vote_payout(const account_object& voter) const {
            auto itr = vote_payout_map_.find(voter.id);
            if (vote_payout_map_.end() != itr) {
                return itr->second;
            }
            return asset(0, VESTS_SYMBOL);
        }

        asset vote_payout(const std::string& voter) const {
            auto account = db_.find_account(voter);
            BOOST_REQUIRE(account != nullptr);
            return vote_payout(*account);
        }

        asset delegator_payout(const account_object& delegator) const {
            auto itr = delegator_payout_map_.find(delegator.id);
            BOOST_CHECK(delegator_payout_map_.end() != itr);
            return itr->second;
        }

        asset delegator_payout(const account_name_type& delegator) const {
            auto account = db_.find_account(delegator);
            BOOST_CHECK(account != nullptr);
            return delegator_payout(*account);
        }

        asset total_beneficiary_payouts() const {
            return total_beneficiary_payouts_;
        }

        asset beneficiary_payout(const account_object& voter) const {
            auto itr = beneficiary_payout_map_.find(voter.id);
            BOOST_REQUIRE(beneficiary_payout_map_.end() != itr);
            return itr->second;
        }

        asset benefeciary_payout(const std::string& voter) const {
            auto account = db_.find_account(voter);
            BOOST_REQUIRE(account != nullptr);
            return beneficiary_payout(*account);
        }

        asset sbd_payout() const {
            return sbd_payout_;
        }

        asset vesting_payout() const {
            return vesting_payout_;
        }

        asset total_payout() const {
            return total_payout_;
        }

    private:
        static const comment_object& get_comment(
            database& db,
            const account_name_type& author,
            const std::string& permlink
        ) {
            auto comment = db.find_comment(author, permlink);
            BOOST_REQUIRE(comment != nullptr);
            return *comment;
        }

        void calculate_rewards() {
            comment_rewards_ = fund_.claim_comment_reward(comment_).amount.value;
            vote_rewards_fund_ = comment_rewards_ * comment_.curation_rewards_percent / STEEMIT_100_PERCENT;
        }

        void calculate_vote_payouts() {
            comment_curation_info c{db_, comment_, false};
            auto total_weight = c.total_vote_weight;

            total_vote_rewards_ = 0;
            total_vote_payouts_ = asset(0, VESTS_SYMBOL);

            auto auction_window_reward = (uint128_t(vote_rewards_fund_) * c.auction_window_weight / total_weight).to_uint64();

            auto auw_time = comment_.created + comment_.auction_window_size;
            uint64_t heaviest_vote_after_auw_weight = 0;
            account_id_type heaviest_vote_after_auw_account;

            for (auto itr = c.vote_list.begin(); itr != c.vote_list.end(); ++itr) {
                BOOST_REQUIRE(vote_payout_map_.find(itr->vote->voter) == vote_payout_map_.end());
                auto weight = u256(itr->weight);
                uint64_t claim = static_cast<uint64_t>(weight * vote_rewards_fund_ / total_weight);

                // to_curators case
                if (comment_.auction_window_reward_destination == protocol::to_curators &&
                    (itr->vote->last_update >= auw_time || db_.get(itr->vote->voter).name == comment_.author)
                ) {
                    if (!heaviest_vote_after_auw_weight) {
                        heaviest_vote_after_auw_weight = itr->weight;
                        heaviest_vote_after_auw_account = itr->vote->voter;
                        continue;
                    }

                    claim += static_cast<int64_t>((auction_window_reward * weight) / c.votes_after_auction_window_weight);
                }

                auto reward = claim;

                if (db_.has_hardfork(STEEMIT_HARDFORK_0_19__756)) {
                    for (auto& dvir : itr->vote->delegator_vote_interest_rates) {
                        auto delegator = db_.find_account(dvir.account);
                        BOOST_CHECK(delegator != nullptr);
                        BOOST_CHECK(delegator_payout_map_.find(delegator->id) == delegator_payout_map_.end());

                        auto delegator_reward = claim * dvir.interest_rate / STEEMIT_100_PERCENT;
                        reward -= delegator_reward;
                        auto delegator_payout = fund_.create_vesting(asset(delegator_reward, STEEM_SYMBOL));

                        delegator_payout_map_.emplace(delegator->id, delegator_payout);
                    }
                } else {
                    BOOST_CHECK(itr->vote->delegator_vote_interest_rates.empty());
                }

                total_vote_rewards_ += reward;
                BOOST_REQUIRE_LE(total_vote_rewards_, vote_rewards_fund_);

                auto payout = fund_.create_vesting(asset(reward, STEEM_SYMBOL));
                vote_payout_map_.emplace(itr->vote->voter, payout);
                total_vote_payouts_ += payout;
            }

            uint64_t unclaimed_rewards = vote_rewards_fund_ - total_vote_rewards_;

            if (comment_.auction_window_reward_destination == protocol::to_curators && heaviest_vote_after_auw_weight) {
                // pay needed claim + rest unclaimed tokens (close to zero value) to curator with greates weight
                // BTW: it has to be unclaimed_rewards.value not heaviest_vote_after_auw_weight + unclaimed_rewards.value, coz
                //      unclaimed_rewards already contains this.
                total_vote_rewards_ += unclaimed_rewards;
                BOOST_REQUIRE_LE(total_vote_rewards_, vote_rewards_fund_);

                auto payout = fund_.create_vesting(asset(unclaimed_rewards, STEEM_SYMBOL));
                vote_payout_map_.emplace(heaviest_vote_after_auw_account, payout);
                total_vote_payouts_ += payout;

                unclaimed_rewards = 0;
            }

            if (comment_.auction_window_reward_destination != protocol::to_author) {
                comment_rewards_ -= unclaimed_rewards;
                auto tokes_back_to_reward_fund = asset(unclaimed_rewards, STEEM_SYMBOL);
                fund_.modify_reward_fund(tokes_back_to_reward_fund);
            }

            comment_rewards_ -= total_vote_rewards_;
        }

        void calculate_beneficiaries_payout() {
            total_beneficiary_rewards_ = 0;
            total_beneficiary_payouts_ = asset(0, VESTS_SYMBOL);
            for (auto& route: comment_.beneficiaries) {
                auto beneficiary = db_.find_account(route.account);
                BOOST_REQUIRE(beneficiary != nullptr);
                BOOST_REQUIRE(beneficiary_payout_map_.find(beneficiary->id) == beneficiary_payout_map_.end());

                int64_t reward = (comment_rewards_ * route.weight) / STEEMIT_100_PERCENT;

                total_beneficiary_rewards_ += reward;
                BOOST_REQUIRE_LE(total_beneficiary_rewards_, comment_rewards_);
                
                auto payout = fund_.create_vesting(asset(reward, STEEM_SYMBOL));
                beneficiary_payout_map_.emplace(beneficiary->id, payout);
                total_beneficiary_payouts_ += payout;
            }

            comment_rewards_ -= total_beneficiary_rewards_;
        }

        void calculate_comment_payout() {
            auto sbd_payout_value = comment_rewards_ / 2;
            auto vesting_payout_value = comment_rewards_ - sbd_payout_value;

            sbd_payout_ = asset(sbd_payout_value, SBD_SYMBOL);
            vesting_payout_ = fund_.create_vesting(asset(vesting_payout_value, STEEM_SYMBOL));
            total_payout_ = sbd_payout_ + db_.to_sbd(vesting_payout_ * fund_.get_vesting_share_price());
        }

        comment_fund& fund_;
        database& db_;

        const comment_object& comment_;
        int64_t comment_rewards_;

        int64_t vote_rewards_fund_;
        int64_t total_vote_rewards_;
        asset total_vote_payouts_;
        std::map<account_id_type, asset> vote_payout_map_;
        std::map<account_id_type, asset> delegator_payout_map_;

        int64_t total_beneficiary_rewards_;
        asset total_beneficiary_payouts_;
        std::map<account_id_type, asset> beneficiary_payout_map_;

        asset sbd_payout_;
        asset vesting_payout_;
        asset total_payout_;
    };

} } // namespace golos::chain