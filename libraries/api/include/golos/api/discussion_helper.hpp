#pragma once
#include <golos/api/account_vote.hpp>
#include <golos/api/vote_state.hpp>
#include <golos/api/discussion.hpp>
#include <golos/api/comment_api_object.hpp>

namespace golos { namespace api {
    struct comment_metadata {
        std::set<std::string> tags;
        std::string language;
    };

    class discussion_helper {
    public:
        discussion_helper() = delete;
        discussion_helper(
            golos::chain::database& db,
            std::function<void(const golos::chain::database&, const account_name_type&, fc::optional<share_type>&)> fill_reputation,
            std::function<void(const golos::chain::database&, discussion&)> fill_promoted,
            std::function<void(const database&, const comment_object&, comment_api_object&)> fill_comment_info
        );
        ~discussion_helper();


        void set_pending_payout(discussion& d) const;

        void set_url(discussion& d) const;

        std::vector<vote_state> select_active_votes(
                const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
        ) const;

        discussion create_discussion(const std::string& author) const;

        discussion create_discussion(const comment_object& o) const;

        discussion get_discussion(const comment_object& c, uint32_t vote_limit, uint32_t offset) const;

        comment_api_object create_comment_api_object(const comment_object& o) const;

        void fill_comment_api_object(const comment_object& o, comment_api_object& d) const;


    private:
        struct impl;
        std::unique_ptr<impl> pimpl;
    };

} } // golos::api

FC_REFLECT((golos::api::comment_metadata), (tags)(language))
