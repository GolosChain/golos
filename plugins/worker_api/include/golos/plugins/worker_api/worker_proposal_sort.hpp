#pragma once

#include <golos/chain/worker_objects.hpp>

namespace golos { namespace plugins { namespace worker_api {

    enum class worker_proposal_sort {
        by_created,
        by_net_rshares
    };

} } } // golos::plugins::worker_api

FC_REFLECT_ENUM(golos::plugins::worker_api::worker_proposal_sort, (by_created)(by_net_rshares))
