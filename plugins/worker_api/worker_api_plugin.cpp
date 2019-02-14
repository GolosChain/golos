#include <golos/plugins/worker_api/worker_api_plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/social_network/social_network.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/index.hpp>
#include <golos/chain/operation_notification.hpp>
#include <appbase/application.hpp>

namespace golos { namespace plugins { namespace worker_api {

namespace bpo = boost::program_options;
using golos::api::discussion_helper;

struct post_operation_visitor {
    golos::chain::database& _db;

    post_operation_visitor(golos::chain::database& db) : _db(db) {
    }

    typedef void result_type;

    template<typename T>
    result_type operator()(const T&) const {
    }

    result_type operator()(const worker_proposal_operation& o) const {
        const auto& wpmo_idx = _db.get_index<worker_proposal_metadata_index, by_permlink>();
        auto wpmo_itr = wpmo_idx.find(std::make_tuple(o.author, o.permlink));

        const auto now = _db.head_block_time();

        if (wpmo_itr != wpmo_idx.end()) { // Edit case
            _db.modify(*wpmo_itr, [&](worker_proposal_metadata_object& wpmo) {
                wpmo.modified = now;
            });

            return;
        }

        // Create case

        const auto& comment = _db.get_comment(o.author, o.permlink);

        _db.create<worker_proposal_metadata_object>([&](worker_proposal_metadata_object& wpmo) {
            wpmo.created = now;
            wpmo.net_rshares = comment.net_rshares;
        });
    }

    result_type operator()(const worker_proposal_delete_operation& o) const {
        const auto& wpmo_idx = _db.get_index<worker_proposal_metadata_index, by_permlink>();
        auto wpmo_itr = wpmo_idx.find(std::make_tuple(o.author, o.permlink));

        _db.remove(*wpmo_itr);
    }

    result_type operator()(const vote_operation& o) {
        const auto& wpmo_idx = _db.get_index<worker_proposal_metadata_index, by_permlink>();
        auto wpmo_itr = wpmo_idx.find(std::make_tuple(o.author, o.permlink));

        if (wpmo_itr == wpmo_idx.end()) {
            return;
        }

        const auto& comment = _db.get_comment(o.author, o.permlink);

        _db.modify(*wpmo_itr, [&](worker_proposal_metadata_object& wpmo) {
            wpmo.net_rshares = comment.net_rshares;
        });
    }
};

class worker_api_plugin::worker_api_plugin_impl final {
public:
    worker_api_plugin_impl(worker_api_plugin& plugin)
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
        helper = std::make_unique<discussion_helper>(
            _db,
            follow::fill_account_reputation,
            nullptr,
            golos::plugins::social_network::fill_comment_info
        );
    }

    void post_operation(const operation_notification& note) {
        try {
            note.op.visit(post_operation_visitor(_db));
        } catch (const fc::assert_exception&) {
            if (_db.is_producing()) {
                throw;
            }
        }
    }

    template <typename DatabaseIndex, typename OrderIndex, bool ReverseSort, typename Selector, typename FillWorkerFields>
    void select_postbased_results_ordered(const auto& query, std::vector<auto>& result, Selector&& select, FillWorkerFields&& fill_worker_fields, bool fill_posts);

    ~worker_api_plugin_impl() = default;

    golos::chain::database& _db;

    std::unique_ptr<discussion_helper> helper;
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

    my->_db.post_apply_operation.connect([&](const operation_notification& note) {
        my->post_operation(note);
    });

    add_plugin_index<worker_proposal_metadata_index>(my->_db);

    JSON_RPC_REGISTER_API(name())
}

void worker_api_plugin::plugin_startup() {
    ilog("Starting up worker api plugin");
}

void worker_api_plugin::plugin_shutdown() {
    ilog("Shutting down worker api plugin");
}

template <typename DatabaseIndex, typename OrderIndex, bool ReverseSort, typename Selector, typename FillWorkerFields>
void worker_api_plugin::worker_api_plugin_impl::select_postbased_results_ordered(const auto& query, std::vector<auto>& result, Selector&& select, FillWorkerFields&& fill_worker_fields, bool fill_posts) {
    query.validate();

    if (!_db.has_index<DatabaseIndex>()) {
        return;
    }

    _db.with_weak_read_lock([&]() {
        const auto& idx = _db.get_index<DatabaseIndex, OrderIndex>();
        auto itr = idx.begin();
        if (ReverseSort) {
            itr = idx.end();
        }

        const comment_object* post = nullptr;

        if (query.has_start()) {
            post = _db.find_comment(*query.start_author, *query.start_permlink);
            if (!post) {
                return;
            }

            const auto& post_idx = _db.get_index<DatabaseIndex, by_post>();
            const auto post_itr = post_idx.find(post->id);
            if (post_itr == post_idx.end()) {
                return;
            }
            itr = idx.iterator_to(*post_itr);
        }

        result.reserve(query.limit);

        auto handle = [&](auto obj) {
            if (!select(query, obj)) {
                return;
            }
            comment_api_object ca;
            if (fill_posts) {
                if (!post) {
                    post = &_db.get_comment(itr->post);
                }
                ca = helper->create_comment_api_object(*post);
                post = nullptr;
            }
            result.emplace_back(obj, ca);
            auto res_obj = result.back();
            if (!fill_worker_fields(_db, res_obj, query)) {
                result.pop_back();
            }
        };

        if (ReverseSort) {
            auto ritr = boost::make_reverse_iterator(itr);
            for (; ritr != idx.rend() && result.size() < query.limit; ++ritr) {
                handle(*ritr);
            }
        } else {
            for (; itr != idx.end() && result.size() < query.limit; ++itr) {
                handle(*itr);
            }
        }
    });
}

// Api Defines

DEFINE_API(worker_api_plugin, get_worker_proposals) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_proposal_query, query)
        (worker_proposal_sort, sort)
        (bool, fill_posts)
    )
    std::vector<worker_proposal_api_object> result;

    auto wpo_selector = [&](const worker_proposal_query& query, const worker_proposal_metadata_object& wpmo) -> bool {
        if (!query.is_good_author(wpmo.author)) {
            return false;
        }
        return true;
    };

    auto wpo_fill_worker_fields = [&](const golos::chain::database& _db, worker_proposal_api_object& wpo_api, const worker_proposal_query& query) -> bool {
        auto wpo = _db.get_worker_proposal(wpo_api.author, wpo_api.permlink);
        if (!query.is_good_state(wpo.state)) {
            return false;
        }
        if (!query.is_good_type(wpo.type)) {
            return false;
        }
        wpo_api.fill_worker_proposal(wpo);
        return true;
    };

    if (sort == worker_proposal_sort::by_created) {
        my->select_postbased_results_ordered<worker_proposal_metadata_index, by_id, true>(query, result, wpo_selector, wpo_fill_worker_fields, fill_posts);
    } else if (sort == worker_proposal_sort::by_net_rshares) {
        my->select_postbased_results_ordered<worker_proposal_metadata_index, by_net_rshares, false>(query, result, wpo_selector, wpo_fill_worker_fields, fill_posts);
    }

    return result;
}

DEFINE_API(worker_api_plugin, get_worker_techspecs) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_techspec_query, query)
        (worker_techspec_sort, sort)
        (bool, fill_posts)
    )
    std::vector<worker_techspec_api_object> result;

    auto wto_selector = [&](const worker_techspec_query& query, const worker_techspec_object& wto) -> bool {
        if (!query.is_good_author(wto.author)) {
            return false;
        }
        if (!query.is_good_state(wto.state)) {
            return false;
        }
        if (!query.is_good_worker_proposal(wto.worker_proposal_author, to_string(wto.worker_proposal_permlink))) {
            return false;
        }
        return true;
    };

    auto wto_fill_worker_fields = [&](const golos::chain::database& _db, worker_techspec_api_object& wto_api, const worker_techspec_query& query) -> bool {
        return true;
    };

    if (sort == worker_techspec_sort::by_created) {
        my->select_postbased_results_ordered<worker_techspec_index, by_id, true>(query, result, wto_selector, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_net_rshares) {
        my->select_postbased_results_ordered<worker_techspec_index, by_net_rshares, false>(query, result, wto_selector, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_approves) {
        my->select_postbased_results_ordered<worker_techspec_index, by_approves, false>(query, result, wto_selector, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_disapproves) {
        my->select_postbased_results_ordered<worker_techspec_index, by_disapproves, false>(query, result, wto_selector, wto_fill_worker_fields, fill_posts);
    }

    return result;
}

} } } // golos::plugins::worker_api
