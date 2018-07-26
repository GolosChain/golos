#include <golos/plugins/private_message/private_message_plugin.hpp>
#include <golos/plugins/private_message/private_message_evaluators.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <appbase/application.hpp>

#include <golos/chain/index.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>
#include <golos/chain/generic_custom_operation_interpreter.hpp>

#include <golos/protocol/validate_helper.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>

#include <fc/smart_ref_impl.hpp>


//
template<typename T>
T dejsonify(const std::string &s) {
    return fc::json::from_string(s).as<T>();
}

#define LOAD_VALUE_SET(options, name, container, type) \
if( options.count(name) ) { \
    const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
    std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &dejsonify<type>); \
}
//

namespace golos {

template<>
std::string get_logic_error_namespace<golos::plugins::private_message::logic_errors::types>() {
    return golos::plugins::private_message::private_message_plugin::name();
}

} // namespace golos

namespace golos { namespace plugins { namespace private_message {

    class private_message_plugin::private_message_plugin_impl final {
    public:
        private_message_plugin_impl(private_message_plugin& plugin)
            : db_(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
            custom_operation_interpreter_ = std::make_shared
                    <generic_custom_operation_interpreter<private_message::private_message_plugin_operation>>(db_);

            custom_operation_interpreter_->register_evaluator<private_message_evaluator>(&plugin);

            db_.set_custom_operation_interpreter(plugin.name(), custom_operation_interpreter_);
            return;
        }

        std::vector<message_api_obj> get_inbox(
            const std::string& to, time_point newest, uint16_t limit, std::uint64_t offset) const;

        std::vector<message_api_obj> get_outbox(
            const std::string& from, time_point newest, uint16_t limit, std::uint64_t offset) const;


        ~private_message_plugin_impl() = default;

        std::shared_ptr<generic_custom_operation_interpreter<private_message_plugin_operation>> custom_operation_interpreter_;
        flat_map<std::string, std::string> tracked_accounts_;

        golos::chain::database &db_;
    };

    std::vector<message_api_obj> private_message_plugin::private_message_plugin_impl::get_inbox(
        const std::string& to, time_point newest, uint16_t limit, std::uint64_t offset
    ) const {
        GOLOS_CHECK_LIMIT_PARAM(limit, 100);

        std::vector<message_api_obj> result;
        const auto &idx = db_.get_index<message_index>().indices().get<by_to_date>();

        auto itr = idx.lower_bound(std::make_tuple(to, newest));
        auto etr = idx.upper_bound(std::make_tuple(to, time_point::min()));

        for (; itr != etr && offset; ++itr, --offset);

        result.reserve(limit);
        for (; itr != etr && limit; ++itr, --limit) {
            result.emplace_back(*itr);
        }

        return result;
    }

    std::vector<message_api_obj> private_message_plugin::private_message_plugin_impl::get_outbox(
        const std::string& from, time_point newest, uint16_t limit, std::uint64_t offset
    ) const {
        GOLOS_CHECK_LIMIT_PARAM(limit, 100);

        std::vector<message_api_obj> result;
        const auto &idx = db_.get_index<message_index>().indices().get<by_from_date>();

        auto itr = idx.lower_bound(std::make_tuple(from, newest));
        auto etr = idx.upper_bound(std::make_tuple(from, time_point::min()));

        for (; itr != etr && offset; ++itr, --offset);

        result.reserve(limit);
        for (; itr != etr && limit; ++itr, --limit) {
            result.emplace_back(*itr);
        }

        return result;
    }

    void private_message_evaluator::do_apply(const private_message_operation &pm) {
        database &d = db();

        const flat_map<std::string, std::string> &tracked_accounts = plugin_->tracked_accounts();

        auto to_itr = tracked_accounts.lower_bound(pm.to);
        auto from_itr = tracked_accounts.lower_bound(pm.from);

        GOLOS_CHECK_LOGIC(pm.from != pm.to, logic_errors::cannot_send_to_yourself,
                "You cannot write to yourself");

        GOLOS_CHECK_LOGIC(pm.from_memo_key != pm.to_memo_key, logic_errors::from_and_to_memo_keys_must_be_different,
                "from_memo_key and to_memo_key must be different");

        GOLOS_CHECK_OP_PARAM(pm, sent_time, GOLOS_CHECK_VALUE_NE(pm.sent_time, 0));
        GOLOS_CHECK_OP_PARAM(pm, encrypted_message, 
            GOLOS_CHECK_VALUE(pm.encrypted_message.size() >= 16, "Message length must be more 16 symbols"));

        if (!tracked_accounts.size() ||
            (to_itr != tracked_accounts.end() && pm.to >= to_itr->first &&
             pm.to <= to_itr->second) ||
            (from_itr != tracked_accounts.end() &&
             pm.from >= from_itr->first && pm.from <= from_itr->second)
        ) {
            d.create<message_object>([&](message_object &pmo) {
                pmo.from = pm.from;
                pmo.to = pm.to;
                pmo.from_memo_key = pm.from_memo_key;
                pmo.to_memo_key = pm.to_memo_key;
                pmo.checksum = pm.checksum;
                pmo.sent_time = pm.sent_time;
                pmo.receive_time = d.head_block_time();
                pmo.encrypted_message.resize(pm.encrypted_message.size());
                std::copy(
                    pm.encrypted_message.begin(), pm.encrypted_message.end(),
                    pmo.encrypted_message.begin());
            });
        }
    }

    private_message_plugin::private_message_plugin() = default;

    private_message_plugin::~private_message_plugin() = default;

    const std::string& private_message_plugin::name() {
        static std::string name = "private_message";
        return name;
    }

    void private_message_plugin::set_program_options(
        boost::program_options::options_description &cli,
        boost::program_options::options_description &cfg
    ) {
        cli.add_options()
            ("pm-account-range",
             boost::program_options::value < std::vector < std::string >> ()->composing()->multitoken(),
             "Defines a range of accounts to private messages to/from as a json pair [\"from\",\"to\"] [from,to)");
        cfg.add(cli);
    }

    void private_message_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
        ilog("Intializing private message plugin");
        my = std::make_unique<private_message_plugin::private_message_plugin_impl>(*this);

        add_plugin_index<message_index>(my->db_);

        using pairstring = std::pair<std::string, std::string>;
        LOAD_VALUE_SET(options, "pm-accounts", my->tracked_accounts_, pairstring);
        JSON_RPC_REGISTER_API(name())
    }

    void private_message_plugin::plugin_startup() {
        ilog("Starting up private message plugin");
    }

    void private_message_plugin::plugin_shutdown() {
        ilog("Shuting down private message plugin");
    }

    flat_map <string, string> private_message_plugin::tracked_accounts() const {
        return my->tracked_accounts_;
    }

    // Api Defines

    DEFINE_API(private_message_plugin, get_inbox) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string,  to)
            (time_point,   newest)
            (uint16_t,     limit)
            (uint64_t,     offset)
        );
        auto &db = my->db_;
        return db.with_weak_read_lock([&]() {
            return my->get_inbox(to, newest, limit, offset);
        });
    }

    DEFINE_API(private_message_plugin, get_outbox) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string,  from)
            (time_point,   newest)
            (uint16_t,     limit)
            (uint64_t,     offset)
        );
        auto &db = my->db_;
        return db.with_weak_read_lock([&]() {
            return my->get_outbox(from, newest, limit, offset);
        });
    }

} } } // golos::plugins::private_message
