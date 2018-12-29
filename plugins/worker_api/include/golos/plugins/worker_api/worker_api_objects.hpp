#pragma once

#include <golos/chain/worker_objects.hpp>

namespace golos { namespace plugins { namespace worker_api {

    using namespace golos::chain;

    struct worker_proposal_api_object {
        worker_proposal_api_object(const worker_proposal_object& o)
            : author(o.author),
              permlink(to_string(o.permlink)),
              type(o.type),
              state(o.state),
              approved_techspec_author(o.approved_techspec_author),
              approved_techspec_permlink(to_string(o.approved_techspec_permlink)),
              created(o.created),
              modified(o.modified) {
        }

        worker_proposal_api_object() {
        }

        account_name_type author;
        std::string permlink;
        worker_proposal_type type;
        worker_proposal_state state;
        account_name_type approved_techspec_author;
        std::string approved_techspec_permlink;
        time_point_sec created;
        time_point_sec modified;
    };

} } } // golos::plugins::worker_api

FC_REFLECT((golos::plugins::worker_api::worker_proposal_api_object),
    (author)(permlink)(type)(state)(approved_techspec_author)(approved_techspec_permlink)(created)(modified)
)
