#include <golos/api/discussion_helper.hpp>
#include <golos/api/comment_api_object.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/steem_objects.hpp>
#include <fc/io/json.hpp>
#include <boost/algorithm/string.hpp>


namespace golos { namespace api {

     boost::multiprecision::uint256_t to256(const fc::uint128_t& t) {
        boost::multiprecision::uint256_t result(t.high_bits());
        result <<= 65;
        result += t.low_bits();
        return result;
    }

    struct discussion_helper::impl final {
    public:
        impl() = delete;
        impl(
            golos::chain::database& db,
            std::function<void(const golos::chain::database&, const account_name_type&, fc::optional<share_type>&)> fill_reputation,
            std::function<void(const golos::chain::database&, discussion&)> fill_promoted,
            std::function<void(const golos::chain::database&, const comment_object&, comment_api_object&)> fill_comment_info)
            : database_(db),
              fill_reputation_(fill_reputation),
              fill_promoted_(fill_promoted),
              fill_comment_info_(fill_comment_info) {
        }
        ~impl() = default;

        discussion create_discussion(const std::string& author) const ;

        discussion create_discussion(const comment_object& o) const ;

        void select_active_votes(
            std::vector<vote_state>& result, uint32_t& total_count,
            const std::string& author, const std::string& permlink, uint32_t limit
        ) const ;

        share_type get_curator_unclaimed_rewards(const discussion& d, share_type max_rewards) const;

        void set_pending_payout(discussion& d) const;

        void set_url(discussion& d) const;

        golos::chain::database& database() {
            return database_;
        }

        golos::chain::database& database() const {
            return database_;
        }

        comment_api_object create_comment_api_object(const comment_object& o) const;

        discussion get_discussion(const comment_object& c, uint32_t vote_limit) const;

        void fill_comment_api_object(const comment_object& o, comment_api_object& d) const;

    private:
        golos::chain::database& database_;
        std::function<void(const golos::chain::database&, const account_name_type&, fc::optional<share_type>&)> fill_reputation_;
        std::function<void(const golos::chain::database&, discussion&)> fill_promoted_;
        std::function<void(const golos::chain::database&, const comment_object&, comment_api_object&)> fill_comment_info_;
    };

// create_comment_api_object 
    comment_api_object discussion_helper::create_comment_api_object(const comment_object& o) const {
        return pimpl->create_comment_api_object(o);
    }

    comment_api_object discussion_helper::impl::create_comment_api_object(const comment_object& o) const {
        comment_api_object result;

        fill_comment_api_object(o, result);

        return result;
    }

// fill_comment_api_object 

    void discussion_helper::fill_comment_api_object(const comment_object& o, comment_api_object& d) const {
        pimpl->fill_comment_api_object(o, d);
    }

    void discussion_helper::impl::fill_comment_api_object(const comment_object& o, comment_api_object& d) const {
        d.id = o.id;
        d.parent_author = o.parent_author;
        d.parent_permlink = to_string(o.parent_permlink);
        d.author = o.author;
        d.permlink = to_string(o.permlink);
        d.created = o.created;
        d.last_payout = o.last_payout;
        d.depth = o.depth;
        d.children = o.children;
        d.children_rshares2 = o.children_rshares2;
        d.net_rshares = o.net_rshares;
        d.abs_rshares = o.abs_rshares;
        d.vote_rshares = o.vote_rshares;
        d.children_abs_rshares = o.children_abs_rshares;
        d.cashout_time = o.cashout_time;
        d.max_cashout_time = o.max_cashout_time;
        d.total_vote_weight = o.total_vote_weight;
        d.reward_weight = o.reward_weight;
        d.net_votes = o.net_votes;
        d.mode = o.mode;
        d.root_comment = o.root_comment;
        d.max_accepted_payout = o.max_accepted_payout;
        d.percent_steem_dollars = o.percent_steem_dollars;
        d.allow_replies = o.allow_replies;
        d.allow_votes = o.allow_votes;
        d.allow_curation_rewards = o.allow_curation_rewards;
        d.auction_window_weight = o.auction_window_weight;

        for (auto& route : o.beneficiaries) {
            d.beneficiaries.push_back(route);
        }

        if (fill_comment_info_) {
            fill_comment_info_(database(), o, d);
        }

        if (o.parent_author == STEEMIT_ROOT_POST_PARENT) {
            d.category = to_string(o.parent_permlink);
        } else {
            d.category = to_string(database().get<comment_object, by_id>(o.root_comment).parent_permlink);
        }
    }

// get_discussion
    discussion discussion_helper::impl::get_discussion(const comment_object& c, uint32_t vote_limit) const {
        discussion d = create_discussion(c);
        set_url(d);
        set_pending_payout(d);
        select_active_votes(d.active_votes, d.active_votes_count, d.author, d.permlink, vote_limit);
        return d;
    }

    discussion discussion_helper::get_discussion(const comment_object& c, uint32_t vote_limit) const {
        return pimpl->get_discussion(c, vote_limit);
    }
//

// select_active_votes
    void discussion_helper::impl::select_active_votes(
        std::vector<vote_state>& result, uint32_t& total_count,
        const std::string& author, const std::string& permlink, uint32_t limit
    ) const {
        const auto& comment = database().get_comment(author, permlink);
        const auto& idx = database().get_index<comment_vote_index>().indices().get<by_comment_voter>();
        comment_object::id_type cid(comment.id);
        total_count = 0;
        result.clear();
        for (auto itr = idx.lower_bound(cid); itr != idx.end() && itr->comment == cid; ++itr, ++total_count) {
            if (result.size() < limit) {
                const auto& vo = database().get(itr->voter);
                vote_state vstate;
                vstate.voter = vo.name;
                vstate.weight = itr->weight;
                vstate.rshares = itr->rshares;
                vstate.percent = itr->vote_percent;
                vstate.time = itr->last_update;
                fill_reputation_(database(), vo.name, vstate.reputation);
                result.emplace_back(vstate);
            }
        }
    }

    void discussion_helper::select_active_votes(
        std::vector<vote_state>& result, uint32_t& total_count,
        const std::string& author, const std::string& permlink, uint32_t limit
    ) const {
        pimpl->select_active_votes(result, total_count, author, permlink, limit);
    }

    share_type discussion_helper::impl::get_curator_unclaimed_rewards(const discussion& d, share_type max_rewards) const {
        share_type unclaimed_rewards = max_rewards;
        auto& db = database();
        try {
            uint128_t total_weight(d.total_vote_weight);

            if (d.allow_curation_rewards) {
                if (d.total_vote_weight > 0) {
                    const auto &cvidx = db.get_index<comment_vote_index>().indices().get<by_comment_weight_voter>();
                    for (auto itr = cvidx.lower_bound(d.id); itr != cvidx.end() && itr->comment == d.id; ++itr) {
                        auto claim = ((max_rewards.value * uint128_t(itr->weight)) / total_weight).to_uint64();
                        if (claim > 0) { // min_amt is non-zero satoshis
                            unclaimed_rewards -= claim;
                        } else {
                            break;
                        }
                    }

                    if (db.has_hardfork(STEEMIT_HARDFORK_0_19__898)) {
                        auto reward_fund_claim = ((max_rewards.value * d.auction_window_weight) / total_weight).to_uint64();
                        unclaimed_rewards -= reward_fund_claim;
                    }
                }
            } else {
                unclaimed_rewards = 0;
            }

            return unclaimed_rewards;
        } FC_CAPTURE_AND_RETHROW()
    }

//
// set_pending_payout
    void discussion_helper::impl::set_pending_payout(discussion& d) const {
        auto& db = database();

        fill_promoted_(db, d);

        const auto& props = db.get_dynamic_global_properties();
        const auto& hist = db.get_feed_history();
        asset pot = props.total_reward_fund_steem;

        if (hist.current_median_history.is_null()) {
            return;
        }

        u256 total_r2 = to256(props.total_reward_shares2);

        if (props.total_reward_shares2 > 0) {
            auto vshares = db.calculate_vshares(d.net_rshares.value > 0 ? d.net_rshares.value : 0);

            u256 r2 = to256(vshares); //to256(abs_net_rshares);
            r2 = (r2 * d.reward_weight) / STEEMIT_100_PERCENT;
            r2 *= pot.amount.value;
            r2 /= total_r2;

            uint64_t payout = static_cast<uint64_t>(r2);

            payout = std::min(payout, uint64_t(d.max_accepted_payout.amount.value));

            uint128_t reward_tokens = uint128_t(payout);

            share_type curation_tokens = ((reward_tokens * db.get_curation_rewards_percent())
                / STEEMIT_100_PERCENT).to_uint64();
            auto crs_unclaimed = get_curator_unclaimed_rewards(d, curation_tokens);
            auto crs_claim = curation_tokens - crs_unclaimed;
            share_type author_tokens = reward_tokens.to_uint64() - crs_claim;
            if (d.allow_curation_rewards) {
                d.pending_curator_payout_value = db.to_sbd(asset(crs_claim, STEEM_SYMBOL));
                d.pending_curator_payout_gests_value = asset(crs_claim, STEEM_SYMBOL) * props.get_vesting_share_price();
                d.pending_payout_value += d.pending_curator_payout_value;
            }

            uint32_t benefactor_weights = 0;
            for (auto &b : d.beneficiaries) {
                benefactor_weights += b.weight;
            }
            if (benefactor_weights != 0) {
                auto total_beneficiary = (author_tokens * benefactor_weights) / STEEMIT_100_PERCENT;
                author_tokens -= total_beneficiary;
                d.pending_benefactor_payout_value = db.to_sbd(asset(total_beneficiary, STEEM_SYMBOL));
                d.pending_benefactor_payout_gests_value = (asset(total_beneficiary, STEEM_SYMBOL) * props.get_vesting_share_price());
                d.pending_payout_value += d.pending_benefactor_payout_value;
            }
            
            auto sbd_steem = (author_tokens * d.percent_steem_dollars) / (2 * STEEMIT_100_PERCENT);
            auto vesting_steem = asset(author_tokens - sbd_steem, STEEM_SYMBOL);
            d.pending_author_payout_gests_value = vesting_steem * props.get_vesting_share_price();
            auto to_sbd = asset((props.sbd_print_rate * sbd_steem) / STEEMIT_100_PERCENT, STEEM_SYMBOL);
            auto to_steem = asset(sbd_steem, STEEM_SYMBOL) - to_sbd;

            d.pending_author_payout_golos_value = to_steem;
            d.pending_author_payout_gbg_value = db.to_sbd(to_sbd);
            d.pending_author_payout_value = d.pending_author_payout_gbg_value + db.to_sbd(to_steem + vesting_steem);
            d.pending_payout_value += d.pending_author_payout_value;

            // End of main calculation

            u256 tpp = to256(d.children_rshares2);
            tpp *= pot.amount.value;
            tpp /= total_r2;

            d.total_pending_payout_value = db.to_sbd(asset(static_cast<uint64_t>(tpp), pot.symbol));
        }

        fill_reputation_(db, d.author, d.author_reputation);

        if (d.parent_author != STEEMIT_ROOT_POST_PARENT) {
            d.cashout_time = db.calculate_discussion_payout_time(db.get_comment(d.id));
        }

        if (d.body.size() > 1024 * 128) {
            d.body = "body pruned due to size";
        }
        if (d.parent_author.size() > 0 && d.body.size() > 1024 * 16) {
            d.body = "comment pruned due to size";
        }

        set_url(d);
    }

    void discussion_helper::set_pending_payout(discussion& d) const {
        pimpl->set_pending_payout(d);
    }
//
// set_url
    void discussion_helper::impl::set_url(discussion& d) const {
        comment_object cm = database().get<comment_object, by_id>(d.root_comment);

        d.url = "/" + d.category + "/@" + std::string(cm.author) + "/" + to_string(cm.permlink);

        if (cm.id != d.id) {
            d.url += "#@" + d.author + "/" + d.permlink;
        }
    }

    void discussion_helper::set_url(discussion& d) const {
        pimpl->set_url(d);
    }
//
// create_discussion
    discussion discussion_helper::impl::create_discussion(const std::string& author) const {
        auto dis = discussion();
        fill_reputation_(database_, author, dis.author_reputation);
        dis.active = time_point_sec::min();
        dis.last_update = time_point_sec::min();
        return dis;
    }

    discussion discussion_helper::impl::create_discussion(const comment_object& o) const {
        discussion d;
        fill_comment_api_object(o, d);
        return d;
    }

    discussion discussion_helper::create_discussion(const std::string& author) const {
        return pimpl->create_discussion(author);
    }

    discussion discussion_helper::create_discussion(const comment_object& o) const {
        return pimpl->create_discussion(o);
    }

    discussion_helper::discussion_helper(
        golos::chain::database& db,
        std::function<void(const golos::chain::database&, const account_name_type&, fc::optional<share_type>&)> fill_reputation,
        std::function<void(const golos::chain::database&, discussion&)> fill_promoted,
        std::function<void(const database&, const comment_object &, comment_api_object&)> fill_comment_info
    ) {
        pimpl = std::make_unique<impl>(db, fill_reputation, fill_promoted, fill_comment_info);
    }

    discussion_helper::~discussion_helper() = default;

//
} } // golos::api
