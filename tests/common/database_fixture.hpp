#ifndef DATABASE_FIXTURE_HPP
#define DATABASE_FIXTURE_HPP

#include <appbase/application.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/shared_authority.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/plugins/debug_node/plugin.hpp>
#include <golos/plugins/account_history/plugin.hpp>
#include <golos/plugins/social_network/social_network.hpp>

#include <graphene/utilities/key_conversion.hpp>
#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>

#include <iostream>

#define INITIAL_TEST_SUPPLY (10000000000ll)

extern uint32_t STEEMIT_TESTING_GENESIS_TIMESTAMP;


#define STEEMIT_REQUIRE_THROW(expr, exc_type)          \
   BOOST_REQUIRE_THROW(expr, exc_type);

#define STEEMIT_CHECK_THROW(expr, exc_type) {             \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "STEEMIT_CHECK_THROW begin "           \
         << req_throw_info << std::endl;                  \
   BOOST_CHECK_THROW( expr, exc_type );                   \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "STEEMIT_CHECK_THROW end "             \
         << req_throw_info << std::endl;                  \
}

#define GOLOS_CHECK_THROW_PROPS_IMPL(S, E, C, TL)                                                       \
    try {                                                                                               \
        BOOST_TEST_PASSPOINT();                                                                         \
        S;                                                                                              \
        BOOST_##TL( "exception '" BOOST_STRINGIZE( E ) "' is expected, but no exception thrown" ); }    \
    catch( E const& ex ) {                                                                              \
        const fc::variant_object &props = ex.get_log().at(0).get_data();                                \
        (void)props; /*avoid "unused var" spam. TODO: @smedvedev, make it used?*/                       \
        try {                                                                                           \
            C;                                                                                          \
        } catch (const fc::exception& err) {                                                            \
            BOOST_##TL( "caught exception '" << err.name() << "' while check props:" <<                 \
                err.to_detail_string());                                                                \
        }                                                                                               \
    } catch ( ... ) {                                                                                   \
        try {                                                                                           \
            throw;                                                                                      \
        } catch (const fc::exception& ex) {                                                             \
            BOOST_##TL( "exception '" BOOST_STRINGIZE( E ) "' is expected, "                            \
                "but '" << ex.name() << "' is caught");                                                 \
        } catch (...) {                                                                                 \
            BOOST_##TL( "exception " BOOST_STRINGIZE( E ) " is expected, "                              \
                "but unknown is caught");                                                               \
        }                                                                                               \
    }                                                                                                   \

#define GOLOS_WARN_THROW_PROPS(S, E, C)          GOLOS_CHECK_THROW_PROPS_IMPL(S, E, C, WARN)
#define GOLOS_CHECK_THROW_PROPS(S, E, C)         GOLOS_CHECK_THROW_PROPS_IMPL(S, E, C, ERROR)
#define GOLOS_REQUIRE_THROW_PROPS(S, E, C)       GOLOS_CHECK_THROW_PROPS_IMPL(S, E, C, FAIL)

#define GOLOS_CHECK_NO_THROW_IMPL(S, TL)                                                                \
    try {                                                                                               \
        BOOST_TEST_PASSPOINT();                                                                         \
        S;                                                                                              \
    } catch(fc::exception const& ex) {                                                                  \
        BOOST_##TL("no exception expected, but '" << ex.name() << "' thrown: \n" <<                     \
            ex.to_detail_string());                                                                     \
    } catch (...) {                                                                                     \
        BOOST_##TL("no exception expected, but unknown exception thrown");                              \
    }

#define GOLOS_WARN_NO_THROW(S)          GOLOS_CHECK_NO_THROW_IMPL(S, WARN)
#define GOLOS_CHECK_NO_THROW(S)         GOLOS_CHECK_NO_THROW_IMPL(S, ERROR)
#define GOLOS_REQUIRE_NO_THROW(S)       GOLOS_CHECK_NO_THROW_IMPL(S, FAIL)

template<typename exception>
struct ErrorValidator {};

using ErrorValidateFunc = std::function<void(const std::string&, const fc::variant& props)>;

#define CHECK_ERROR(exception, ...) [&](const std::string& name, const fc::variant& props) {\
    ErrorValidator<exception> v; \
    v.validate(name, props, __VA_ARGS__); \
}

template<>
struct ErrorValidator<golos::invalid_parameter> {
    void validate(const std::string& name, const fc::variant& props,
            const std::string& param) {
        BOOST_CHECK_EQUAL(name, "invalid_parameter");
        BOOST_CHECK_EQUAL(props["param"].get_string(), param);
    }
};

template<>
struct ErrorValidator<golos::insufficient_funds> {
    void validate(const std::string& name, const fc::variant& props,
            const std::string& account, const std::string& balance, const std::string& amount) {
        BOOST_CHECK_EQUAL(name, "insufficient_funds");
        BOOST_CHECK_EQUAL(props["account"].get_string(), account);
        BOOST_CHECK_EQUAL(props["balance"].get_string(), balance);
        BOOST_CHECK_EQUAL(props["required"].get_string(), amount);
    }
    void validate(const std::string& name, const fc::variant& props,
            const std::string& account, const std::string& balance, const golos::protocol::asset& amount) {
        BOOST_CHECK_EQUAL(name, "insufficient_funds");
        BOOST_CHECK_EQUAL(props["account"].get_string(), account);
        BOOST_CHECK_EQUAL(props["balance"].get_string(), balance);
        BOOST_CHECK_EQUAL(props["required"].get_string(), amount.to_string());
    }
};

template<>
struct ErrorValidator<golos::protocol::tx_invalid_operation> {
    void validate(const std::string& name, const fc::variant& props,
            int index, ErrorValidateFunc validator = NULL) {
        BOOST_CHECK_EQUAL(name, "tx_invalid_operation");
        BOOST_CHECK_EQUAL(props["index"].as_int64(), index);
        if(validator) {
            validator(props["error"]["name"].get_string(), props["error"]["stack"][(size_t)0]["data"]);
        }
    }
};

template<>
struct ErrorValidator<golos::missing_object> {
    void validate(const std::string& name, const fc::variant& props,
            const std::string& type, const std::string& id) {
        BOOST_CHECK_EQUAL(name, "missing_object");
        BOOST_CHECK_EQUAL(props["type"].get_string(), type);
        BOOST_CHECK_EQUAL(props["id"].get_string(), id);
    }

    void validate(const std::string& name, const fc::variant& props,
            const std::string& type, const fc::variant_object& id) {
        BOOST_CHECK_EQUAL(name, "missing_object");
        BOOST_CHECK_EQUAL(props["type"].get_string(), type);
        BOOST_CHECK_EQUAL(props["id"].get_object(), id);
    }
};

template<>
struct ErrorValidator<golos::object_already_exist> {
    void validate(const std::string& name, const fc::variant& props,
            const std::string& type, const std::string& id) {
        BOOST_CHECK_EQUAL(name, "object_already_exist");
        BOOST_CHECK_EQUAL(props["type"].get_string(), type);
        BOOST_CHECK_EQUAL(props["id"].get_string(), id);
    }

    void validate(const std::string& name, const fc::variant& props,
            const std::string& type, const fc::variant_object& id) {
        BOOST_CHECK_EQUAL(name, "object_already_exist");
        BOOST_CHECK_EQUAL(props["type"].get_string(), type);
        BOOST_CHECK_EQUAL(props["id"].get_object(), id);
    }
};

template<>
struct ErrorValidator<golos::logic_exception> {
    template<typename errors>
    void validate(const std::string& name, const fc::variant& props, errors err) {
        BOOST_CHECK_EQUAL(name, "logic_exception");
        BOOST_CHECK_EQUAL(props["errid"].get_string(), fc::reflector<errors>::to_string(err));
        BOOST_CHECK_EQUAL(props["namespace"].get_string(), golos::get_logic_error_namespace<errors>());
    }
};

template<>
struct ErrorValidator<golos::bandwidth_exception> {
    void validate(const std::string& name, const fc::variant& props,
            golos::bandwidth_exception::bandwidth_types type) {
        BOOST_CHECK_EQUAL(name, "bandwidth_exception");
        BOOST_CHECK_EQUAL(props["bandwidth"].get_string(),
            fc::reflector<golos::bandwidth_exception::bandwidth_types>::to_string(type));
        BOOST_CHECK_NO_THROW(props["now"].as_string());
        BOOST_CHECK_NO_THROW(props["next"].as_string());
    }
};

#define SIMPLE_PROTOCOL_ERROR_VALIDATOR(E) SIMPLE_ERROR_VALIDATOR(golos::protocol, E)
#define SIMPLE_ERROR_VALIDATOR(NS, E)   \
template<>                              \
struct ErrorValidator<NS::E> {          \
    void validate(const std::string& name, const fc::variant& props, int) { \
        BOOST_CHECK_EQUAL(name, #E);    \
    }                                   \
};

// Auto-generate trivial ErrorValidators
SIMPLE_PROTOCOL_ERROR_VALIDATOR(tx_irrelevant_sig);
SIMPLE_PROTOCOL_ERROR_VALIDATOR(tx_duplicate_sig);
SIMPLE_PROTOCOL_ERROR_VALIDATOR(tx_duplicate_transaction);
SIMPLE_PROTOCOL_ERROR_VALIDATOR(tx_missing_posting_auth);
SIMPLE_PROTOCOL_ERROR_VALIDATOR(tx_missing_active_auth);
SIMPLE_PROTOCOL_ERROR_VALIDATOR(tx_missing_owner_auth);
SIMPLE_PROTOCOL_ERROR_VALIDATOR(tx_missing_other_auth);


#define GOLOS_CHECK_ERROR_PROPS_IMPL(S, C, TL) \
    GOLOS_CHECK_THROW_PROPS_IMPL(S, golos::golos_exception, C(ex.name(), ex.get_log().at(0).get_data()), TL)

#define GOLOS_WARN_ERROR_PROPS(S, C)          GOLOS_CHECK_ERROR_PROPS_IMPL(S, C, WARN)
#define GOLOS_CHECK_ERROR_PROPS(S, C)         GOLOS_CHECK_ERROR_PROPS_IMPL(S, C, ERROR)
#define GOLOS_REQUIRE_ERROR_PROPS(S, C)       GOLOS_CHECK_ERROR_PROPS_IMPL(S, C, FAIL)

#define GOLOS_CHECK_ERROR_LOGIC(EXCEPTION, KEY, OP) \
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, KEY, OP), \
        CHECK_ERROR(tx_invalid_operation, 0, \
            CHECK_ERROR(logic_exception, logic_exception::EXCEPTION)))

#define GOLOS_CHECK_ERROR_MISSING(OBJECT, ID, KEY, OP) \
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, KEY, OP), \
        CHECK_ERROR(tx_invalid_operation, 0, \
            CHECK_ERROR(missing_object, #OBJECT, ID)))


///This simply resets v back to its default-constructed value. Requires v to have a working assingment operator and
/// default constructor.
#define RESET(v) v = decltype(v)()
///This allows me to build consecutive test cases. It's pretty ugly, but it works well enough for unit tests.
/// i.e. This allows a test on update_account to begin with the database at the end state of create_account.
#define INVOKE(test) ((struct test*)this)->test_method(); trx.clear()

#define PREP_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name));   \
   fc::ecc::private_key name ## _post_key = generate_private_key(BOOST_PP_STRINGIZE(name) "_post"); \
   public_key_type name ## _public_key = name ## _private_key.get_public_key();

#define ACTOR(name) \
   PREP_ACTOR(name) \
   const auto& name = account_create(BOOST_PP_STRINGIZE(name), name ## _public_key, name ## _post_key.get_public_key()); \
   account_id_type name ## _id = name.id; (void)name ## _id;

#define GET_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name)); \
   const account_object& name = db->get_account(BOOST_PP_STRINGIZE(name)); \
   account_id_type name ## _id = name.id; (void)name ## _id;

#define ACTORS_IMPL(r, data, elem) ACTOR(elem)
#define ACTORS(names) BOOST_PP_SEQ_FOR_EACH(ACTORS_IMPL, ~, names) \
   validate_database();


// Note: testnet and mainnet can have different asset names
#define ASSET(s) asset::from_string(s)

#define ASSET_GBG(x)   asset(int64_t((x)*1e3), SBD_SYMBOL)
#define ASSET_GOLOS(x) asset(int64_t((x)*1e3), STEEM_SYMBOL)
#define ASSET_GESTS(x) asset(int64_t((x)*1e6), VESTS_SYMBOL)


// get_vesting_share_price() is a dynamic value which depends on funds,
//   that is why comparision can be done only with some correction
#define GOLOS_VEST_REQUIRE_EQUAL(left, right) \
    BOOST_REQUIRE( \
        std::abs((left).amount.value - (right).amount.value) < 5 && \
        (left).symbol == (right).symbol \
    )


// ostream <<
//-------------------------------------------------------------

namespace fc {

std::ostream& operator<<(std::ostream& out, const fc::exception& e);
std::ostream& operator<<(std::ostream& out, const fc::time_point& v);
std::ostream& operator<<(std::ostream& out, const fc::uint128_t& v);
std::ostream& operator<<(std::ostream& out, const fc::fixed_string<fc::uint128_t>& v);
std::ostream& operator<<(std::ostream& out, const fc::variant_object& v);

template<typename T>
std::ostream& operator<<(std::ostream& out, const fc::safe<T>& v) {
    out << v.value;
    return out;
}

bool operator==(const fc::variant_object& left, const fc::variant_object& right);

} // fc


namespace fc { namespace ecc {

std::ostream& operator<<(std::ostream& out, const public_key& v);

} } // fc::ecc


namespace chainbase {

template<typename T>
std::ostream& operator<<(std::ostream& out, const object_id<T> &v) {
    out << v._id;
    return out;
}

} // chainbase


namespace std {

template<typename T1, typename T2>
std::ostream& operator<<(std::ostream& out, const std::pair<T1,T2>& v) {
    out << "<" << v.first << ":" << v.second << ">";
    return out;
}

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T> &vec) {
    out << "(";
    if (!vec.empty()) {
        std::for_each(vec.begin(), vec.end()-1, [&](const T& v) {out << v << ",";});
        out << *vec.rbegin();
    }
    out << ")";
    return out;
}

} // std


namespace boost { namespace container {

template<typename T>
std::ostream& operator<<(std::ostream& out, const flat_set<T>& t) {
    out << "(";
    if (!t.empty()) {
        std::for_each(t.begin(), t.end()-1, [&](const T& v) {out << v << ",";});
        out << *t.rbegin();
    }
    out << ")";
    return out;
}

template<typename T, typename... V>
std::ostream& operator<<(std::ostream& out, const flat_map<T,V...>& t) {
    out << "(";
    if (!t.empty()) {
        std::for_each(t.begin(), t.end()-1,
            [&](const typename flat_map<T,V...>::value_type& v) {
                out << v.first << ":" << v.second << ",";
            });
        auto last = *t.rbegin();
        out << last.first << ":" << last.second;
    }
    out << ")";
    return out;
}

} } // boost::container


namespace golos { namespace protocol {

std::ostream& operator<<(std::ostream& out, const asset& v);
std::ostream& operator<<(std::ostream& out, const public_key_type& v);
std::ostream& operator<<(std::ostream& out, const authority& v);
std::ostream& operator<<(std::ostream& out, const price& v);

} } // golos::protocol


namespace golos { namespace chain {

std::ostream& operator<<(std::ostream& out, const shared_authority& v);

} } // golos::chain


///////////////////////////////////////////////////////////////
// database_fixture
///////////////////////////////////////////////////////////////


#ifndef STEEMIT_INIT_PRIVATE_KEY
#  define STEEMIT_INIT_PRIVATE_KEY (fc::ecc::private_key::regenerate(fc::sha256::hash(BLOCKCHAIN_NAME)))
#endif

namespace golos { namespace chain {

        using namespace golos::protocol;

        fc::variant_object make_comment_id(const std::string& author, const std::string& permlink);
        fc::variant_object make_limit_order_id(const std::string& author, uint32_t orderid);
        fc::variant_object make_convert_request_id(const std::string& account, uint32_t requestid);
        fc::variant_object make_escrow_id(const string& name, uint32_t escrow_id);

        using account_name_set = fc::flat_set<account_name_type>;

        namespace {
            template<typename... S>
                struct PluginRegistrator;

            template<typename P, typename... S>
                struct PluginRegistrator<P, S...> {
                    static void register_plugins() {
                        appbase::app().register_plugin<P>();
                        PluginRegistrator<S...>::register_plugins();
                    }
                };

            template<>
                struct PluginRegistrator<> {
                    static void register_plugins() {}
                };
        } // anonymous namespace


        struct database_fixture {
            // the reason we use an app is to exercise the indexes of built-in plugins
            chain::database* db;
            signed_transaction trx;
            fc::ecc::private_key init_account_priv_key = STEEMIT_INIT_PRIVATE_KEY;
            string debug_key = golos::utilities::key_to_wif(init_account_priv_key);
            public_key_type init_account_pub_key = init_account_priv_key.get_public_key();
            uint32_t default_skip = 0 | database::skip_undo_history_check | database::skip_authority_check;

            golos::plugins::chain::plugin* ch_plugin = nullptr;
            golos::plugins::debug_node::plugin* db_plugin = nullptr;
            golos::plugins::account_history::plugin* ah_plugin = nullptr;
            golos::plugins::social_network::social_network* sn_plugin = nullptr;

            optional<fc::temp_directory> data_dir;
            bool skip_key_index_test = false;

            database_fixture() {
            }

            virtual ~database_fixture();

            static fc::ecc::private_key generate_private_key(string seed);

            template<typename Plugin>
            Plugin* find_plugin() {
                return dynamic_cast<Plugin*>(appbase::app().find_plugin<Plugin>());
            }

            using plugin_options = std::map<std::string,std::string>;
            template<typename... Plugins>
            void initialize(const plugin_options& opts = {}) {
                int argc = boost::unit_test::framework::master_test_suite().argc;
                char** argv = boost::unit_test::framework::master_test_suite().argv;

                for (int i = 1; i < argc; i++) {
                    const std::string arg = argv[i];
                    if (arg == "--record-assert-trip") {
                        fc::enable_record_assert_trip = true;
                    }
                    if (arg == "--show-test-names") {
                        std::cout << "running test "
                                  << boost::unit_test::framework::current_test_case().p_name
                                  << std::endl;
                    }
                }

                ch_plugin = &appbase::app().register_plugin<golos::plugins::chain::plugin>();
                db_plugin = &appbase::app().register_plugin<golos::plugins::debug_node::plugin>();
                sn_plugin = &appbase::app().register_plugin<golos::plugins::social_network::social_network>();
                ah_plugin = &appbase::app().register_plugin<golos::plugins::account_history::plugin>();
                PluginRegistrator<Plugins...>::register_plugins();

                ch_plugin->skip_startup = true;

                std::vector<std::string> args_data;
                // fill all elements first so c_str() pointers stay valid
                for (const auto& opt: opts) {
                    args_data.push_back(std::string("--") + opt.first);
                    args_data.push_back(opt.second);
                }
                std::vector<const char*> args;
                args.push_back(argv[0]);
                for (const auto& arg: args_data) {
                    args.push_back(arg.c_str());
                }
                for (int i = 1; i < argc; i++) {
                    args.push_back(argv[i]);
                }

                appbase::app().initialize<
                    golos::plugins::chain::plugin,
                    golos::plugins::account_history::plugin,
                    golos::plugins::debug_node::plugin,
                    golos::plugins::social_network::social_network,
                    Plugins...
                >(args.size(), const_cast<char**>(args.data()));

                db_plugin->set_logging(false);

                db = &ch_plugin->db();
                BOOST_REQUIRE(db);
            }

            void startup(bool generate_hardfork = true);

            void open_database();
            void close_database();

            void generate_block(
                uint32_t skip = 0,
                const fc::ecc::private_key& key = STEEMIT_INIT_PRIVATE_KEY,
                int miss_blocks = 0);

            /**
             * @brief Generates block_count blocks
             * @param block_count number of blocks to generate
             */
            void generate_blocks(uint32_t block_count);

            /**
             * @brief Generates blocks until the head block time matches or exceeds timestamp
             * @param timestamp target time to generate blocks until
             */
            void generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks = true);

            const account_object &account_create(
                    const string &name,
                    const string &creator,
                    const private_key_type &creator_key,
                    const share_type &fee,
                    const public_key_type &key,
                    const public_key_type &post_key,
                    const string &json_metadata
            );

            const account_object &account_create(
                    const string &name,
                    const public_key_type &key,
                    const public_key_type &post_key
            );

            const account_object &account_create(
                    const string &name,
                    const public_key_type &key
            );


            const witness_object &witness_create(
                    const string &owner,
                    const private_key_type &owner_key,
                    const string &url,
                    const public_key_type &signing_key,
                    const share_type &fee
            );

            const comment_object& comment_create(
                    const string& author,
                    const private_key_type& author_key,
                    const string& permlink,
                    const string& parent_author,
                    const string& parent_permlink,
                    const string& title,
                    const string& body,
                    const string& json
            );

            const comment_object& comment_create(
                    const string& author,
                    const private_key_type& author_key,
                    const string& permlink,
                    const string& parent_author,
                    const string& parent_permlink
            );

            void fund(const string &account_name, const share_type &amount = 500000);

            void fund(const string &account_name, const asset &amount);

            void transfer(const string &from, const string &to, const share_type &steem);

            void convert(const string &account_name, const asset &amount);

            void vest(const string &from, const share_type &amount);

            void vest(const string &account, const asset &amount);

            void proxy(const string &account, const string &proxy);

            void set_price_feed(const price &new_price);

            const asset &get_balance(const string &account_name) const;

            void sign(signed_transaction &trx, const fc::ecc::private_key &key);

            vector<operation> get_last_operations(uint32_t ops);

            // we have producer_reward virtual op now, so get_last_operations
            // can be hard to use after generate_blocks.
            // this get_last_operations variant helps to get only required op type
            template<typename op_type>
            vector<op_type> get_last_operations(uint32_t num_ops) {
                vector<op_type> ops;
                const auto& acc_hist_idx = db->get_index<golos::plugins::account_history::account_history_index>().indices().get<by_id>();
                auto itr = acc_hist_idx.end();
                while (itr != acc_hist_idx.begin() && ops.size() < num_ops) {
                    itr--;
                    auto op = fc::raw::unpack<golos::chain::operation>(db->get(itr->op).serialized_op);
                    if (op.which() == operation::tag<op_type>::value)
                        ops.push_back(op.get<op_type>());
                }
                return ops;
            }

            void validate_database(void);


            // helpers to remove boilerplate code
        private:
            void tx_push_ops(signed_transaction& tx, operation op) {
                tx.operations.push_back(op);
            }
            template<typename... Ops>
            void tx_push_ops(signed_transaction& tx, operation op, Ops... ops) {
                tx.operations.push_back(op);
                tx_push_ops(tx, ops...);
            }

        public:
            template<typename... Ops>
            void sign_tx_with_ops(signed_transaction& tx, const fc::ecc::private_key& k, Ops... ops) {
                tx.clear();
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx_push_ops(tx, ops...);
                sign(tx, k);
            }

            template<typename... Ops>
            void push_tx_with_ops(signed_transaction& tx, const fc::ecc::private_key& k, Ops... ops) {
                sign_tx_with_ops(tx, k, ops...);
                db->push_transaction(tx, 0);
            }
        };

        struct clean_database_fixture : public database_fixture {
            clean_database_fixture();

            ~clean_database_fixture() override;

            void resize_shared_mem(uint64_t size);
        };

        struct live_database_fixture : public database_fixture {
            live_database_fixture();

            ~live_database_fixture() override;

            fc::path _chain_dir;
        };

        struct add_operations_database_fixture : public database_fixture {
            using operation_history_plugin = golos::plugins::operation_history::plugin;
            using operations_map = std::map<std::string, std::string>;

            template<typename... Plugins>
            void initialize(const plugin_options& opts = {}) {
                database_fixture::initialize<operation_history_plugin>(opts);
                oh_plugin = find_plugin<operation_history_plugin>();
                open_database();
                startup();
            }

            operations_map add_operations();
            operation_history_plugin* oh_plugin = nullptr;
        };

        namespace test {
            bool _push_block(database& db, const signed_block& b, uint32_t skip_flags = 0);

            void _push_transaction(database& db, const signed_transaction& tx, uint32_t skip_flags = 0);
        }

} } // golos:chain
#endif
