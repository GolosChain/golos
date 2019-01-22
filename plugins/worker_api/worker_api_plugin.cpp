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

    void select_worker_proposals(const worker_proposal_query& query, std::vector<worker_proposal_api_object>& result);

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

void worker_api_plugin::worker_api_plugin_impl::select_worker_proposals(const worker_proposal_query& query, std::vector<worker_proposal_api_object>& result) {
    query.validate();

    if (!_db.has_index<worker_proposal_index>()) {
        return;
    }

    _db.with_weak_read_lock([&]() {
        auto& wpo_idx = _db.get_index<worker_proposal_index, by_created>();
        if (query.sort == worker_proposal_sort::by_rshares) {
            // TODO
        }
        auto wpo_itr = wpo_idx.begin();

        if (query.has_start()) {
            const auto& wpo_post_idx = _db.get_index<worker_proposal_index, by_permlink>();
            const auto wpo_post_itr = wpo_post_idx.find(std::make_tuple(*query.start_author, *query.start_permlink));
            if (wpo_post_itr == wpo_post_idx.end()) {
                return;
            }
            wpo_itr = wpo_idx.iterator_to(*wpo_post_itr);
        }

        result.reserve(query.limit);

        for (; wpo_itr != wpo_idx.end() && result.size() < query.limit; ++wpo_itr) {
            if (!query.select_authors.empty() && !query.select_authors.count(wpo_itr->author)) {
                continue;
            }
            if (!query.select_states.empty() && !query.select_states.count(wpo_itr->state)) {
                continue;
            }
            if (!query.select_types.empty() && !query.select_types.count(wpo_itr->type)) {
                continue;
            }
            result.emplace_back(*wpo_itr);
        }
    });
}

// Api Defines

DEFINE_API(worker_api_plugin, get_worker_proposals) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_proposal_query, query)
    )
    std::vector<worker_proposal_api_object> result;

    my->select_worker_proposals(query, result);
    return result;
}

} } } // golos::plugins::worker_api
