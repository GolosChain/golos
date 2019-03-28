#pragma once

#include <golos/chain/worker_objects.hpp>

namespace golos { namespace plugins { namespace worker_api {

    enum class worker_proposal_sort {
        by_created,
        by_net_rshares
    };

    enum class worker_techspec_sort {
        by_created,
        by_net_rshares,
        by_approves,
        by_disapproves,
        by_worker_payment_approves,
        by_worker_payment_disapproves
    };

} } } // golos::plugins::worker_api

FC_REFLECT_ENUM(golos::plugins::worker_api::worker_proposal_sort, (by_created)(by_net_rshares))

FC_REFLECT_ENUM(golos::plugins::worker_api::worker_techspec_sort, (by_created)(by_net_rshares)(by_approves)(by_disapproves)
    (by_worker_payment_approves)(by_worker_payment_disapproves))
