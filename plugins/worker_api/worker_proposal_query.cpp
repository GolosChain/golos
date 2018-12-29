#include <golos/plugins/worker_api/worker_proposal_query.hpp>
#include <golos/protocol/validate_helper.hpp>

using golos::protocol::validate_account_name;
using golos::protocol::validate_permlink;

namespace golos { namespace plugins { namespace worker_api {

    void worker_proposal_query::validate() const {
        GOLOS_CHECK_LIMIT_PARAM(limit, 100);
        if (start_author) {
            GOLOS_CHECK_PARAM_ACCOUNT(*start_author);
        }
        if (start_permlink) {
            GOLOS_CHECK_PARAM(*start_permlink, validate_permlink(*start_permlink));
        }
    }

} } } // golos::plugins::worker_api

