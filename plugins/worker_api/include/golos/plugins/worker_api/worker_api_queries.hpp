#pragma once

#include <fc/optional.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos::chain;

namespace golos { namespace plugins { namespace worker_api {

    struct worker_proposal_query {
        uint32_t                        limit = 20;
        fc::optional<std::string>       start_author;
        fc::optional<std::string>       start_permlink;
        std::set<std::string>           select_authors;
        std::set<worker_proposal_state> select_states;
        std::set<worker_proposal_type>  select_types;

        void validate() const;

        bool has_start() const {
            return !!start_author;
        }
    };

    struct worker_techspec_query {
        uint32_t                        limit = 20;
        fc::optional<std::string>       start_author;
        fc::optional<std::string>       start_permlink;
        std::set<std::string>           select_authors;
        fc::optional<std::string>       worker_proposal_author;
        fc::optional<std::string>       worker_proposal_permlink;

        void validate() const;

        bool has_start() const {
            return !!start_author;
        }
    };

} } } // golos::plugins::worker_api

FC_REFLECT((golos::plugins::worker_api::worker_proposal_query),
    (limit)(start_author)(start_permlink)(select_authors)(select_states)(select_types)
);

FC_REFLECT((golos::plugins::worker_api::worker_techspec_query),
    (limit)(start_author)(start_permlink)(select_authors)(worker_proposal_author)(worker_proposal_permlink)
);
