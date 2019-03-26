#include <golos/chain/database.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/plugins/account_history/plugin.hpp>
#include <golos/plugins/account_history/history_object.hpp>
#include <golos/plugins/operation_history/history_object.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>

#include <boost/algorithm/string.hpp>
#include <queue>

#define ACCOUNT_HISTORY_MAX_LIMIT 10000
#define ACCOUNT_HISTORY_DEFAULT_LIMIT 100
#define GOLOS_OP_NAMESPACE "golos::protocol::"


namespace golos { namespace plugins { namespace account_history {

using namespace golos::protocol;
using namespace golos::chain;
namespace bpo = boost::program_options;
using impacted_accounts = fc::flat_map<golos::chain::account_name_type, operation_direction>;

struct operation_visitor_filter;
void operation_get_impacted_accounts(const operation& op, impacted_accounts& result);


template<typename T>
T dejsonify(const string &s) {
    return fc::json::from_string(s).as<T>();
}

#define LOAD_VALUE_SET(options, name, container, type) \
if (options.count(name)) { \
    const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
    std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &dejsonify<type>); \
}

    struct op_name_visitor {
        using result_type = std::string;
        template<class T>
        std::string operator()(const T&) const {
            return fc::get_typename<T>::name();
        }
    };

    struct operation_visitor final {
        operation_visitor(
            golos::chain::database& db,
            const golos::chain::operation_notification& op_note,
            std::string op_account,
            operation_direction dir)
            : db(db),
              note(op_note),
              account(op_account),
              dir(dir) {
        }

        using result_type = void;

        golos::chain::database& db;
        const golos::chain::operation_notification& note;
        std::string account;
        operation_direction dir;

        template<typename Op>
        void operator()(Op &&) const {
            const auto& idx = db.get_index<account_history_index>().indices().get<by_account>();

            auto itr = idx.lower_bound(std::make_tuple(account, uint32_t(-1)));
            uint32_t sequence = 0;
            if (itr != idx.end() && itr->account == account) {
                sequence = itr->sequence + 1;
            }

            db.create<account_history_object>([&](account_history_object& history) {
                history.block = note.block;
                history.account = account;
                history.sequence = sequence;
                history.dir = dir;
                history.op_tag = note.op.which();
                history.op = operation_history::operation_id_type(note.db_id);
            });
        }
    };

    struct plugin::plugin_impl final {
    public:
        plugin_impl(): db(appbase::app().get_plugin<chain::plugin>().db()) {
        }

        ~plugin_impl() = default;

        void erase_old_blocks() {
            uint32_t head_block = db.head_block_num();
            if (history_blocks <= head_block) {
                uint32_t need_block = head_block - history_blocks;
                const auto& idx = db.get_index<account_history_index>().indices().get<by_location>();
                auto it = idx.begin();
                while (it != idx.end() && it->block <= need_block) {
                    auto next_it = it;
                    ++next_it;
                    db.remove(*it);
                    it = next_it;
                }
            }
        }

        void on_operation(const golos::chain::operation_notification& note) {
            if (!note.stored_in_db) {
                return;
            }

            impacted_accounts impacted;
            operation_get_impacted_accounts(note.op, impacted);

            for (const auto& item : impacted) {
                auto itr = tracked_accounts.lower_bound(item.first);
                if (!tracked_accounts.size() ||
                    (itr != tracked_accounts.end() && itr->first <= item.first && item.first <= itr->second)
                ) {
                    note.op.visit(operation_visitor(db, note, item.first, item.second));
                }
            }
        }

        ///////////////////////////////////////////////////////
        // API
        history_operations fetch_unfiltered(string account, uint32_t from, uint32_t limit) {
            history_operations result;
            const auto& idx = db.get_index<account_history_index>().indices().get<by_account>();
            auto itr = idx.lower_bound(std::make_tuple(account, from));
            auto end = idx.upper_bound(std::make_tuple(account, std::max(int64_t(0), int64_t(itr->sequence) - limit)));
            for (; itr != end; ++itr) {
                result[itr->sequence] = db.get(itr->op);
            }
            return result;
        }

        using op_tag_type = int;
        using op_tags = fc::flat_set<op_tag_type>;
        using op_names = fc::flat_set<std::string>;

        op_tags op_names_to_tags(op_names names) {
            op_tags result;
            for (const auto& n: names) {
                int from = 0, to = 0;
                if (n == "ALL") {
                    to = operation::count();
                } else if (n == "REAL") {
                    to = virtual_op_tag;
                } else if (n == "VIRTUAL") {
                    from = virtual_op_tag;
                    to = operation::count();
                }
                if (to > 0) {
                    for (; from < to; from++) {
                        result.insert(from);
                    }
                } else {
                    GOLOS_CHECK_VALUE(op_name2tag.count(n), "Unknown operation: ${o}", ("o",n));
                    result.insert(op_name2tag[n]);
                }
            }
            return result;
        }

        struct sequenced_itr {
            using op_idx_type = account_history_index::index<by_operation>::type;
            using op_itr_type = account_history_index::index_const_iterator<by_operation>::type;

            op_itr_type itr;

            sequenced_itr(const op_idx_type& idx, account_name_type a, uint8_t o, operation_direction d, uint32_t s)
                : itr(idx.lower_bound(std::make_tuple(a, o, d, s))) {
            }

            // reconstruct to put next value into queue (can't reuse previous because const in queue)
            sequenced_itr(op_itr_type itr): itr(itr) {
            }

            bool operator<(const sequenced_itr& other) const {
                return itr->sequence < other.itr->sequence;
            }
        };

        history_operations get_account_history(
            std::string account,
            uint32_t from,
            uint32_t limit,
            account_history_query query
        ) {
            GOLOS_CHECK_LIMIT_PARAM(limit, ACCOUNT_HISTORY_MAX_LIMIT);
            GOLOS_CHECK_PARAM(from, GOLOS_CHECK_VALUE(from >= limit, "From must be greater then limit"));
            op_tags select_ops;
            GOLOS_CHECK_PARAM(query, {
                select_ops = op_names_to_tags(query.select_ops ? *query.select_ops : op_names({"ALL"}));
                auto filter_ops = op_names_to_tags(query.filter_ops ? *query.filter_ops : op_names({}));
                for (auto t: filter_ops) {
                    select_ops.erase(t);
                }
                GOLOS_CHECK_VALUE(!select_ops.empty(), "Query contains no operations to select");
            });
            auto dir = query.direction ? *query.direction : operation_direction::any;

            bool is_all_ops = select_ops.size() == operation::count();
            if (is_all_ops && dir == operation_direction::any) {
                return fetch_unfiltered(account, from, limit);
            }
            std::priority_queue<sequenced_itr> itrs;
            const auto& idx = db.get_index<account_history_index>().indices().get<by_operation>();
            const auto& end = idx.end();

            auto put_itr = [&](op_tag_type o, operation_direction d, bool force = false) {
                if (force || operation_direction::any == dir || d == dir) {
                    auto i = sequenced_itr(idx, account, uint8_t(o), d, from);
                    if (i.itr != end && i.itr->op_tag == o && i.itr->dir == d)
                        itrs.push(i);
                }
            };
            for (const auto o: select_ops) {
                put_itr(o, operation_direction::sender);
                put_itr(o, operation_direction::receiver);
                put_itr(o, operation_direction::dual, dir == sender || dir == receiver);
            }

            history_operations result;
            while (!itrs.empty() && result.size() <= limit) {
                auto itr = itrs.top().itr;
                itrs.pop();
                result[itr->sequence] = db.get(itr->op);
                auto o = itr->op_tag;
                auto d = itr->dir;
                auto next = sequenced_itr(++itr);
                if (next.itr != end && next.itr->op_tag == o && next.itr->dir == d)
                    itrs.push(next);
            }
            return result;
        }

        op_tag_type virtual_op_tag = -1;                        // all operations >= this value are virtual
        fc::flat_map<std::string, op_tag_type> op_name2tag;
        fc::flat_map<std::string, std::string> tracked_accounts;
        golos::chain::database& db;
        uint32_t history_blocks = UINT32_MAX;
    };

    DEFINE_API(plugin, get_account_history) {
        PLUGIN_API_VALIDATE_ARGS(
            (account_name_type, account)
            (uint32_t, from, 0xFFFFFFFF)
            (uint32_t, limit, ACCOUNT_HISTORY_DEFAULT_LIMIT)
            (account_history_query, query, account_history_query())
        );
        return pimpl->db.with_weak_read_lock([&]() {
            return pimpl->get_account_history(account, from, limit, query);
        });
    }

    struct get_impacted_account_visitor final {
        impacted_accounts& impacted;

        get_impacted_account_visitor(impacted_accounts& impact)
            : impacted(impact) {
        }

        using result_type = void;

        template<typename T>
        void operator()(const T& op) {
            fc::flat_set<golos::chain::account_name_type> impd;
            op.get_required_posting_authorities(impd);
            op.get_required_active_authorities(impd);
            op.get_required_owner_authorities(impd);
            for (auto i : impd) {
                impacted.insert(make_pair(i, operation_direction::dual));
            }
        }

        void insert_account(account_name_type a, operation_direction d) {
            impacted.insert(make_pair(a, d));
        }
        void insert_sender(account_name_type a) {
            insert_account(a, operation_direction::sender);
        }
        void insert_receiver(account_name_type a) {
            insert_account(a, operation_direction::receiver);
        }
        void insert_dual(account_name_type a) {
            insert_account(a, operation_direction::dual);
        }

        void insert_pair(account_name_type sender, account_name_type receiver, bool have_receiver = true) {
            if (sender == receiver) {
                insert_dual(sender);
            } else {
                insert_sender(sender);
                if (have_receiver)
                    insert_receiver(receiver);
            }
        }

        void operator()(const account_create_operation& op) {
            insert_pair(op.creator, op.new_account_name);
        }

        void operator()(const account_create_with_delegation_operation& op) {
            insert_pair(op.creator, op.new_account_name);
        }

        void operator()(const account_update_operation& op) {
            insert_dual(op.account);
        }

        void operator()(const account_metadata_operation& op) {
            insert_dual(op.account);
        }

        void operator()(const comment_operation& op) {
            insert_pair(op.author, op.parent_author, op.parent_author.size());
        }

        void operator()(const delete_comment_operation& op) {
            insert_dual(op.author);
        }

        void operator()(const vote_operation& op) {
            insert_pair(op.voter, op.author);
        }

        void operator()(const author_reward_operation& op) {
            insert_receiver(op.author);
        }

        void operator()(const curation_reward_operation& op) {
            insert_receiver(op.curator);
        }

        void operator()(const liquidity_reward_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const interest_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const fill_convert_request_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const transfer_operation& op) {
            insert_pair(op.from, op.to);
        }

        void operator()(const transfer_to_vesting_operation& op) {
            auto have_to = op.to != account_name_type();
            insert_pair(op.from, have_to ? op.to: op.from);
        }

        void operator()(const withdraw_vesting_operation& op) {
            insert_dual(op.account);
        }

        void operator()(const witness_update_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const chain_properties_update_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const account_witness_vote_operation& op) {
            insert_pair(op.account, op.witness);
        }

        void operator()(const account_witness_proxy_operation& op) {
            insert_pair(op.account, op.proxy);
        }

        void operator()(const feed_publish_operation& op) {
            insert_dual(op.publisher);
        }

        void operator()(const limit_order_create_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const fill_order_operation& op) {
            insert_pair(op.current_owner, op.open_owner);
        }

        void operator()(const limit_order_cancel_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const pow_operation& op) {
            insert_dual(op.worker_account);
        }

        void operator()(const fill_vesting_withdraw_operation& op) {
            insert_pair(op.from_account, op.to_account);
        }

        void operator()(const shutdown_witness_operation& op) {
            insert_dual(op.owner);
        }

        void operator()(const custom_operation& op) {
            for (auto& s: op.required_auths) {
                insert_dual(s);
            }
        }

        void operator()(const request_account_recovery_operation& op) {
            insert_dual(op.account_to_recover);
        }

        void operator()(const recover_account_operation& op) {
            insert_dual(op.account_to_recover);
        }

        void operator()(const change_recovery_account_operation& op) {
            insert_pair(op.account_to_recover, op.new_recovery_account);
        }

        template<typename Op>
        void insert_from_to_agent_direction(const Op& op) {
            insert_sender(op.from);
            insert_receiver(op.to);
            insert_receiver(op.agent);
        }

        void operator()(const escrow_transfer_operation& op) {
            insert_from_to_agent_direction(op);
        }

        // note: the initiator (signer) of escrow_approve_operation is who. he can be either to or agent
        void operator()(const escrow_approve_operation& op) {
            insert_from_to_agent_direction(op);
        }

        // note: the initiator (signer) of escrow_dispute_operation is who. he can be either from or to
        void operator()(const escrow_dispute_operation& op) {
            insert_from_to_agent_direction(op);
        }

        // note: the initiator (signer) of escrow_release_operation is who. he can be either from or to or agent.
        //       the receiver receives funds, he can be either from or to
        void operator()(const escrow_release_operation& op) {
            insert_from_to_agent_direction(op);
        }

        void operator()(const transfer_to_savings_operation& op) {
            insert_pair(op.from, op.to);
        }

        void operator()(const transfer_from_savings_operation& op) {
            insert_pair(op.from, op.to);
        }

        void operator()(const cancel_transfer_from_savings_operation& op) {
            insert_dual(op.from);
        }

        void operator()(const decline_voting_rights_operation& op) {
            insert_dual(op.account);
        }

        void operator()(const comment_benefactor_reward_operation& op) {
            insert_pair(op.author, op.benefactor);
        }

        void operator()(const producer_reward_operation& op) {
            insert_receiver(op.producer);
        }

        void operator()(const delegation_reward_operation& op) {
            insert_pair(op.delegator, op.delegatee);
        }

        void operator()(const delegate_vesting_shares_operation& op) {
            insert_pair(op.delegator, op.delegatee);
        }

        void operator()(const return_vesting_delegation_operation& op) {
            insert_receiver(op.account);
        }

        // todo: proposal tx signers are receivers
        void operator()(const proposal_create_operation& op) {
            insert_dual(op.author);
        }

        void operator()(const proposal_update_operation& op) {
            insert_receiver(op.author);
            auto insert_set = [this](const fc::flat_set<golos::chain::account_name_type>& impd) {
                for (auto i : impd) {
                    insert_dual(i);
                }
            };
            insert_set(op.active_approvals_to_add);
            insert_set(op.owner_approvals_to_add);
            insert_set(op.posting_approvals_to_add);
            insert_set(op.active_approvals_to_remove);
            insert_set(op.owner_approvals_to_remove);
            insert_set(op.posting_approvals_to_remove);
        }

        void operator()(const proposal_delete_operation& op) {
            insert_pair(op.requester, op.author);
        }

        void operator()(const worker_proposal_operation& op) {
            insert_dual(op.author);
        }

        void operator()(const worker_proposal_delete_operation& op) {
            insert_dual(op.author);
        }

        void operator()(const worker_techspec_operation& op) {
            insert_pair(op.author, op.worker_proposal_author);
        }

        void operator()(const worker_techspec_delete_operation& op) {
            insert_dual(op.author);
        }

        void operator()(const worker_techspec_approve_operation& op) {
            insert_pair(op.approver, op.author);
        }

        void operator()(const worker_assign_operation& op) {
            insert_sender(op.assigner);
            insert_receiver(op.worker);
            insert_receiver(op.worker_techspec_author);
        }

        void operator()(const worker_result_operation& op) {
            insert_dual(op.author);
        }

        void operator()(const worker_result_premade_operation& op) {
            insert_pair(op.author, op.worker_proposal_author);
        }

        void operator()(const worker_result_delete_operation& op) {
            insert_dual(op.author);
        }

        void operator()(const worker_result_approve_operation& op) {
            insert_pair(op.approver, op.author);
        }

        void operator()(const techspec_reward_operation& op) {
            insert_receiver(op.author);
        }

        void operator()(const worker_reward_operation& op) {
            insert_pair(op.worker_techspec_author, op.worker);
        }
    };

    void operation_get_impacted_accounts(const operation& op, impacted_accounts& result) {
        get_impacted_account_visitor vtor = get_impacted_account_visitor(result);
        op.visit(vtor);
    }

    void plugin::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
        cfg.add_options()(
            "track-account-range",
            bpo::value<std::vector<std::string>>()->composing()->multitoken(),
            "Defines a range of accounts to track as a json pair [\"from\",\"to\"] [from,to]. "
            "Can be specified multiple times"
        )
        (
            "track-account",
            bpo::value<std::vector<std::string>>()->composing(),
            "Defines a individual account to track (in addition to ranges). "
            "Can be specified multiple times"
        );
    }

    void plugin::plugin_initialize(const bpo::variables_map& options) {
        ilog("account_history plugin: plugin_initialize() begin");
        pimpl = std::make_unique<plugin_impl>();

        if (options.count("history-blocks")) {
            uint32_t history_blocks = options.at("history-blocks").as<uint32_t>();
            pimpl->history_blocks = history_blocks;
            pimpl->db.applied_block.connect([&](const signed_block& block){
                pimpl->erase_old_blocks();
            });
        } else {
            pimpl->history_blocks = UINT32_MAX;
        }
        ilog("account_history: history-blocks ${s}", ("s", pimpl->history_blocks));

        // this is worked, because the appbase initialize required plugins at first
        pimpl->db.pre_apply_operation.connect([&](operation_notification& note) {
            pimpl->on_operation(note);
        });

        add_plugin_index<account_history_index>(pimpl->db);

        using pairstring = std::pair<std::string, std::string>;
        fc::flat_map<std::string, std::string> ranges;
        LOAD_VALUE_SET(options, "track-account-range", ranges, pairstring);

        if (options.count("track-account") > 0) {
            auto accounts = options.at("track-account").as<std::vector<std::string>>();
            for (auto& a : accounts) {
                std::vector<std::string> names;
                boost::split(names, a, boost::is_any_of(" \t,"));
                for (auto& n : names) {
                    if (!n.empty())
                        ranges[n] = n;          // construct "range" with 1 account name in it
                }
            }
        }
        // exclude embedded ranges (fix #701)
        const auto end = ranges.end();
        for (auto i = ranges.begin(); i < end; ++i) {
            bool bad = false;
            for (auto j = ranges.begin(); !bad && j < end && j->first <= i->second; ++j) {
                if (j == i) continue;
                bad = i->first >= j->first && i->second <= j->second;
            }
            if (!bad)
                pimpl->tracked_accounts[i->first] = i->second;
        }
        ilog("account_history: tracked_accounts ${s}", ("s", pimpl->tracked_accounts));

        // prepare map to convert operation name to operation tag
        pimpl->op_name2tag = {};
        op_name_visitor nvisit;
        operation op;
        auto count = operation::count();
        for (auto i = 0; i < count; i++) {
            op.set_which(i);
            auto name = op.visit(nvisit);
            name = name.substr(sizeof(GOLOS_OP_NAMESPACE) - 1);             // cut "golos::protocol::"
            pimpl->op_name2tag[name] = i;
            name = name.substr(0, name.size() + 1 - sizeof("_operation"));  // support names without "_operation" too
            pimpl->op_name2tag[name] = i;
            if (pimpl->virtual_op_tag == -1 && is_virtual_operation(op)) {
                pimpl->virtual_op_tag = i;
            }
        }

        JSON_RPC_REGISTER_API(name());
        ilog("account_history plugin: plugin_initialize() end");
    }

    plugin::plugin() = default;

    plugin::~plugin() = default;

    const std::string& plugin::name() {
        static std::string name = "account_history";
        return name;
    }

    void plugin::plugin_startup() {
        ilog("account_history plugin: plugin_startup() begin");
        ilog("account_history plugin: plugin_startup() end");
    }

    void plugin::plugin_shutdown() {
    }

    fc::flat_map<std::string, std::string> plugin::tracked_accounts() const {
        return pimpl->tracked_accounts;
    }

} } } // golos::plugins::account_history
