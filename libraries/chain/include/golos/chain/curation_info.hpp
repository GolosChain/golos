#pragma once

#include <golos/chain/database.hpp>
#include <golos/chain/comment_object.hpp>

namespace golos { namespace chain {

    struct comment_vote_info {
        const comment_vote_object* vote = nullptr;
        uint64_t weight = 0; ///< defines the score this vote receives, used by vote payout calc. 0 if a negative vote or changed votes.
    }; // struct comment_vote_info

    struct comment_curation_info {
        const comment_object& comment;

        std::vector<comment_vote_info> vote_list;

        uint64_t total_vote_weight = 0; ///< The total weight of voting rewards, used to calculate pro-rata share of curation payouts
        uint64_t auction_window_weight = 0; ///< The weight of auction window without weight of voters
        uint64_t votes_in_auction_window_weight = 0; ///< The weight of votes in auction window
        uint64_t author_promote_weight = 0; ///< Sum of author promote weights
        uint64_t votes_after_auction_window_weight = 0;
        protocol::curation_curve curve = protocol::curation_curve::detect;

        comment_curation_info(comment_curation_info&&) = default;
        comment_curation_info(const comment_curation_info&) = delete;

        comment_curation_info(database& db, const comment_object&, bool);
    }; // struct comment_curation_info

} } // namespace golos::chain