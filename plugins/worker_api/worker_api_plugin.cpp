#include <golos/plugins/worker_api/worker_api_plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/chain/comment_object.hpp>
#include <appbase/application.hpp>

namespace golos { namespace plugins { namespace worker_api {

namespace bpo = boost::program_options;

class worker_api_plugin::worker_api_plugin_impl final {
public:
    worker_api_plugin_impl(worker_api_plugin& plugin)
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    template <typename DatabaseIndex, typename OrderIndex, typename Selector>
    void select_postbased_results_ordered(const auto& query, std::vector<auto>& result, Selector&& select);

    ~worker_api_plugin_impl() = default;

    golos::chain::database& _db;
};

worker_api_plugin::worker_api_plugin() = default;

worker_api_plugin::~worker_api_plugin() = default;

const std::string& worker_api_plugin::name() {
    static std::string name = "worker_api";
    return name;
}

void worker_api_plugin::set_program_options(
    bpo::options_description& cli,
    bpo::options_description& cfg
) {
}

void worker_api_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
    ilog("Intializing account notes plugin");

    my = std::make_unique<worker_api_plugin::worker_api_plugin_impl>(*this);

    JSON_RPC_REGISTER_API(name())
}

void worker_api_plugin::plugin_startup() {
    ilog("Starting up worker api plugin");
}

void worker_api_plugin::plugin_shutdown() {
    ilog("Shutting down worker api plugin");
}

template <typename DatabaseIndex, typename OrderIndex, typename Selector>
void worker_api_plugin::worker_api_plugin_impl::select_postbased_results_ordered(const auto& query, std::vector<auto>& result, Selector&& select) {
    query.validate();

    if (!_db.has_index<DatabaseIndex>()) {
        return;
    }

    _db.with_weak_read_lock([&]() {
        const auto& idx = _db.get_index<DatabaseIndex, OrderIndex>();
        auto itr = idx.begin();

        if (query.has_start()) {
            const auto& post_idx = _db.get_index<DatabaseIndex, by_permlink>();
            const auto post_itr = post_idx.find(std::make_tuple(*query.start_author, *query.start_permlink));
            if (post_itr == post_idx.end()) {
                return;
            }
            itr = idx.iterator_to(*post_itr);
        }

        result.reserve(query.limit);

        for (; itr != idx.end() && result.size() < query.limit; ++itr) {
            if (!select(query, *itr)) {
                continue;
            }
            result.emplace_back(*itr);
        }
    });
}

// Api Defines

DEFINE_API(worker_api_plugin, get_worker_proposals) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_proposal_query, query)
        (worker_proposal_sort, sort)
    )
    std::vector<worker_proposal_api_object> result;

    auto wpo_selector = [&](const worker_proposal_query& query, const worker_proposal_object& wpo) -> bool {
        if (!query.is_good_author(wpo.author)) {
            return false;
        }
        if (!query.is_good_state(wpo.state)) {
            return false;
        }
        if (!query.is_good_type(wpo.type)) {
            return false;
        }
        return true;
    };

    if (sort == worker_proposal_sort::by_created) {
        my->select_postbased_results_ordered<worker_proposal_index, by_created>(query, result, wpo_selector);
    } else if (sort == worker_proposal_sort::by_net_rshares) {
        my->select_postbased_results_ordered<worker_proposal_index, by_net_rshares>(query, result, wpo_selector);
    }

    return result;
}

DEFINE_API(worker_api_plugin, get_worker_techspecs) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_techspec_query, query)
        (worker_techspec_sort, sort)
    )
    std::vector<worker_techspec_api_object> result;

    auto wto_selector = [&](const worker_techspec_query& query, const worker_techspec_object& wto) -> bool {
        if (!query.is_good_author(wto.author)) {
            return false;
        }
        if (!query.is_good_worker_proposal(wto.worker_proposal_author, to_string(wto.worker_proposal_permlink))) {
            return false;
        }
        return true;
    };

    if (sort == worker_techspec_sort::by_created) {
        my->select_postbased_results_ordered<worker_techspec_index, by_created>(query, result, wto_selector);
    } else if (sort == worker_techspec_sort::by_net_rshares) {
        my->select_postbased_results_ordered<worker_techspec_index, by_net_rshares>(query, result, wto_selector);
    } else if (sort == worker_techspec_sort::by_approves) {
        my->select_postbased_results_ordered<worker_techspec_index, by_approves>(query, result, wto_selector);
    } else if (sort == worker_techspec_sort::by_disapproves) {
        my->select_postbased_results_ordered<worker_techspec_index, by_disapproves>(query, result, wto_selector);
    }

    return result;
}

DEFINE_API(worker_api_plugin, get_worker_intermediates) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_intermediate_query, query)
    )
    std::vector<worker_intermediate_api_object> result;

    auto wio_selector = [&](const worker_intermediate_query& query, const worker_intermediate_object& wio) -> bool {
        if (!query.is_good_worker_techspec(wio.author, to_string(wio.worker_techspec_permlink))) {
            return false;
        }
        return true;
    };

    my->select_postbased_results_ordered<worker_intermediate_index, by_created>(query, result, wio_selector);

    return result;
}

} } } // golos::plugins::worker_api
