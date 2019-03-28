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
    worker_api_plugin& _plugin;

    post_operation_visitor(golos::chain::database& db, worker_api_plugin& plugin) : _db(db), _plugin(plugin) {
    }

    typedef void result_type;

    template<typename T>
    result_type operator()(const T&) const {
    }

    result_type operator()(const worker_proposal_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wpmo_idx = _db.get_index<worker_proposal_metadata_index, by_post>();
        auto wpmo_itr = wpmo_idx.find(post.id);

        const auto now = _db.head_block_time();

        if (wpmo_itr != wpmo_idx.end()) { // Edit case
            _db.modify(*wpmo_itr, [&](worker_proposal_metadata_object& wpmo) {
                wpmo.modified = now;
            });

            return;
        }

        // Create case

        _db.create<worker_proposal_metadata_object>([&](worker_proposal_metadata_object& wpmo) {
            wpmo.post = post.id;
            wpmo.created = now;
            wpmo.net_rshares = post.net_rshares;
        });
    }

    result_type operator()(const worker_proposal_delete_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wpmo_idx = _db.get_index<worker_proposal_metadata_index, by_post>();
        auto wpmo_itr = wpmo_idx.find(post.id);

        _db.remove(*wpmo_itr);
    }

    result_type operator()(const worker_techspec_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wtmo_idx = _db.get_index<worker_techspec_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(post.id);

        if (wtmo_itr != wtmo_idx.end()) { // Edit case
            _db.modify(*wtmo_itr, [&](worker_techspec_metadata_object& wtmo) {
                wtmo.modified = _db.head_block_time();
            });

            return;
        }

        // Create case

        _db.create<worker_techspec_metadata_object>([&](worker_techspec_metadata_object& wtmo) {
            wtmo.post = post.id;
            wtmo.net_rshares = post.net_rshares;
        });
    }

    result_type operator()(const worker_techspec_delete_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto* wto = _db.find_worker_techspec(post.id);
        if (wto) {
            return;
        }

        const auto& wtmo_idx = _db.get_index<worker_techspec_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(post.id);

        _db.remove(*wtmo_itr);
    }

    result_type operator()(const vote_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wpmo_idx = _db.get_index<worker_proposal_metadata_index, by_post>();
        auto wpmo_itr = wpmo_idx.find(post.id);
        if (wpmo_itr != wpmo_idx.end()) {
            _db.modify(*wpmo_itr, [&](worker_proposal_metadata_object& wpmo) {
                wpmo.net_rshares = post.net_rshares;
            });
            return;
        }

        const auto& wtmo_idx = _db.get_index<worker_techspec_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(post.id);
        if (wtmo_itr != wtmo_idx.end()) {
            _db.modify(*wtmo_itr, [&](worker_techspec_metadata_object& wtmo) {
                wtmo.net_rshares = post.net_rshares;
            });
        }
    }

    result_type operator()(const worker_techspec_approve_operation& o) const {
        const auto& wto_post = _db.get_comment(o.author, o.permlink);

        const auto& wtmo_idx = _db.get_index<worker_techspec_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(wto_post.id);

        auto approves = _db.count_worker_techspec_approves(wto_post.id);

        _db.modify(*wtmo_itr, [&](worker_techspec_metadata_object& wtmo) {
            wtmo.approves = approves[worker_techspec_approve_state::approve];
            wtmo.disapproves = approves[worker_techspec_approve_state::disapprove];
        });
    }

    result_type operator()(const worker_assign_operation& o) const {
        const auto& wto_post = _db.get_comment(o.worker_techspec_author, o.worker_techspec_permlink);

        const auto& wtmo_idx = _db.get_index<worker_techspec_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(wto_post.id);

        _db.modify(*wtmo_itr, [&](worker_techspec_metadata_object& wtmo) {
            if (!o.worker.size()) { // Unassign worker
                wtmo.work_beginning_time = time_point_sec::min();
                return;
            }

            wtmo.work_beginning_time = _db.head_block_time();
        });
    }

    result_type operator()(const worker_result_approve_operation& o) const {
        const auto& worker_result_post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_result(worker_result_post.id);

        auto approves = _db.count_worker_result_approves(worker_result_post.id);

        const auto& wtmo_idx = _db.get_index<worker_techspec_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(wto.post);
        _db.modify(*wtmo_itr, [&](worker_techspec_metadata_object& wtmo) {
            wtmo.worker_result_approves = approves[worker_techspec_approve_state::approve];
            wtmo.worker_result_disapproves = approves[worker_techspec_approve_state::disapprove];

            if (wto.state == worker_techspec_state::payment && wtmo.consumption_per_day.amount == 0) {
                wtmo.payment_beginning_time = wto.next_cashout_time;
                wtmo.consumption_per_day = _db.calculate_worker_techspec_consumption_per_day(wto);
            } else if (wto.state == worker_techspec_state::payment_canceled) {
                wtmo.consumption_per_day = asset(0, STEEM_SYMBOL);
            }
        });
    }

    result_type operator()(const techspec_reward_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wto = _db.get_worker_techspec(post.id);

        if (wto.finished_payments_count != wto.payments_count) {
            return;
        }

        const auto& wtmo_idx = _db.get_index<worker_techspec_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(post.id);
        _db.modify(*wtmo_itr, [&](worker_techspec_metadata_object& wtmo) {
            wtmo.consumption_per_day = asset(0, STEEM_SYMBOL);
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

    void post_operation(const operation_notification& note, worker_api_plugin& self) {
        try {
            note.op.visit(post_operation_visitor(_db, self));
        } catch (const fc::assert_exception&) {
            if (_db.is_producing()) {
                throw;
            }
        }
    }

    template <typename DatabaseIndex, typename OrderIndex, bool ReverseSort, typename FillWorkerFields>
    void select_postbased_results_ordered(const auto& query, std::vector<auto>& result, FillWorkerFields&& fill_worker_fields, bool fill_posts);

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
        my->post_operation(note, *this);
    });

    add_plugin_index<worker_proposal_metadata_index>(my->_db);
    add_plugin_index<worker_techspec_metadata_index>(my->_db);

    JSON_RPC_REGISTER_API(name())
}

void worker_api_plugin::plugin_startup() {
    ilog("Starting up worker api plugin");
}

void worker_api_plugin::plugin_shutdown() {
    ilog("Shutting down worker api plugin");
}

template <typename DatabaseIndex, typename OrderIndex, bool ReverseSort, typename FillWorkerFields>
void worker_api_plugin::worker_api_plugin_impl::select_postbased_results_ordered(const auto& query, std::vector<auto>& result, FillWorkerFields&& fill_worker_fields, bool fill_posts) {
    if (!_db.has_index<DatabaseIndex>()) {
        return;
    }

    if (query.has_author()) {
        GOLOS_CHECK_PARAM(fill_posts, {
            GOLOS_CHECK_VALUE(fill_posts, "Cannot filter by author without filling posts");
        });
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
            comment_api_object ca;
            if (fill_posts) {
                if (!post) {
                    post = &_db.get_comment(itr->post);
                }

                if (!query.is_good_author(post->author)) {
                    return;
                }

                ca = helper->create_comment_api_object(*post);
                post = nullptr;
            }
            result.emplace_back(obj, ca);
            if (!fill_worker_fields(this, obj, result.back(), query)) {
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
        (bool, fill_approved_techspec_posts)
    )
    query.validate();

    std::vector<worker_proposal_api_object> result;

    auto wpo_fill_worker_fields = [&](worker_api_plugin_impl* my, const worker_proposal_metadata_object& wpmo, worker_proposal_api_object& wpo_api, const worker_proposal_query& query) -> bool {
        auto wpo = my->_db.get_worker_proposal(wpmo.post);
        if (!query.is_good_state(wpo.state)) {
            return false;
        }
        if (!query.is_good_type(wpo.type)) {
            return false;
        }
        wpo_api.fill_worker_proposal(wpo);
        if (fill_approved_techspec_posts && wpo.approved_techspec_post != comment_id_type()) {
            const auto post = my->_db.get_comment(wpo.approved_techspec_post);
            wpo_api.approved_techspec_post = my->helper->create_comment_api_object(post);
        }
        return true;
    };

    if (sort == worker_proposal_sort::by_created) {
        my->select_postbased_results_ordered<worker_proposal_metadata_index, by_id, true>(query, result, wpo_fill_worker_fields, fill_posts);
    } else if (sort == worker_proposal_sort::by_net_rshares) {
        my->select_postbased_results_ordered<worker_proposal_metadata_index, by_net_rshares, false>(query, result, wpo_fill_worker_fields, fill_posts);
    }

    return result;
}

DEFINE_API(worker_api_plugin, get_worker_techspecs) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_techspec_query, query)
        (worker_techspec_sort, sort)
        (bool, fill_posts)
        (bool, fill_worker_proposal_posts)
        (bool, fill_worker_result_posts)
    )
    query.validate();

    if (query.has_worker_proposal()) {
        GOLOS_CHECK_PARAM(fill_worker_proposal_posts, {
            GOLOS_CHECK_VALUE(fill_worker_proposal_posts, "Cannot filter by worker proposal without filling worker proposal posts");
        });
    }

    std::vector<worker_techspec_api_object> result;

    auto wto_fill_worker_fields = [&](worker_api_plugin_impl* my, const worker_techspec_metadata_object& wtmo, worker_techspec_api_object& wto_api, const worker_techspec_query& query) -> bool {
        auto wto = my->_db.get_worker_techspec(wtmo.post);
        if (!query.is_good_state(wto.state)) {
            return false;
        }
        if (fill_worker_proposal_posts) {
            const auto post = my->_db.get_comment(wto.worker_proposal_post);
            wto_api.worker_proposal_post = my->helper->create_comment_api_object(post);

            if (!query.is_good_worker_proposal(post.author, to_string(post.permlink))) {
                return false;
            }
        }
        if (fill_worker_result_posts && wto.worker_result_post != comment_id_type()) {
            const auto post = my->_db.get_comment(wto.worker_result_post);
            wto_api.worker_result_post = my->helper->create_comment_api_object(post);
        }
        wto_api.fill_worker_techspec(wto);
        return true;
    };

    if (sort == worker_techspec_sort::by_created) {
        my->select_postbased_results_ordered<worker_techspec_metadata_index, by_id, true>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_net_rshares) {
        my->select_postbased_results_ordered<worker_techspec_metadata_index, by_net_rshares, false>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_approves) {
        my->select_postbased_results_ordered<worker_techspec_metadata_index, by_approves, false>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_disapproves) {
        my->select_postbased_results_ordered<worker_techspec_metadata_index, by_disapproves, false>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_worker_result_approves) {
        my->select_postbased_results_ordered<worker_techspec_metadata_index, by_worker_result_approves, false>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_techspec_sort::by_worker_result_disapproves) {
        my->select_postbased_results_ordered<worker_techspec_metadata_index, by_worker_result_disapproves, false>(query, result, wto_fill_worker_fields, fill_posts);
    }

    return result;
}

} } } // golos::plugins::worker_api
