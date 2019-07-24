#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>

#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/protocol/steem_operations.hpp>

#include <golos/plugins/json_rpc/plugin.hpp>

#include "database_fixture.hpp"

using namespace golos::chain;
using namespace golos::protocol;

typedef golos::plugins::json_rpc::plugin json_rpc_plugin;

namespace test_plugin {

    using golos::plugins::json_rpc::msg_pack;

    DEFINE_API_ARGS(throw_exception, msg_pack, std::string)

    class testing_api final : public appbase::plugin<testing_api> {
    public:
        testing_api() { }
        ~testing_api() { }

        constexpr static const char *plugin_name = "testing_api";

        APPBASE_PLUGIN_REQUIRES((json_rpc_plugin));

        static const std::string &name() {
            static std::string name = plugin_name;
            return name;
        }

        void set_program_options(boost::program_options::options_description &,
                boost::program_options::options_description &) override {
        }

        void plugin_initialize(const boost::program_options::variables_map &options) override {
            JSON_RPC_REGISTER_API(plugin_name);
        }

        void plugin_startup() override { }

        void plugin_shutdown() override { }

        DECLARE_API((throw_exception))
    };

    DEFINE_API(testing_api, throw_exception) {
        auto error = args.args->at(0).get_string();

        if (error == "unsupported_operation")
            FC_THROW_EXCEPTION(golos::unsupported_operation, "Unsupported operation");

        if (error == "invalid_parameter")
            FC_THROW_EXCEPTION(golos::invalid_parameter, "Invalid parameter");

        if (error == "business_exception")
            FC_THROW_EXCEPTION(golos::business_exception, "Business logic error");

        if (error == "tx_missing_authority")
            FC_THROW_EXCEPTION(golos::protocol::tx_missing_authority, "Missing authority");

        if (error == "tx_invalid_operation")
            FC_THROW_EXCEPTION(golos::protocol::tx_invalid_operation, "Invalid operation");

        if (error == "transaction_exception")
            FC_THROW_EXCEPTION(golos::protocol::transaction_exception, "Transaction error");

        if (error == "golos_exception")
            FC_THROW_EXCEPTION(golos::golos_exception, "Internal server exception");

        if (error == "fc::exception")
            FC_THROW_EXCEPTION(fc::exception, "Internal error");

        if (error == "std::exception")
            throw std::logic_error("Internal error");

        throw "Internal error";
    }
} // namespace test_plugin

fc::variant call(json_rpc_plugin& plugin, const std::string& request) {
    fc::variant response;
    plugin.call(request, [&](const std::string& str) {response = fc::json::from_string(str);});
    return response;
}

void check_error_response(const fc::variant& response, const fc::variant& id, int32_t code, const std::string& error_name = std::string()) {
    BOOST_CHECK_EQUAL(response["jsonrpc"].get_string(), "2.0");
    BOOST_CHECK_EQUAL(response["id"].get_type(), id.get_type());
    if (!id.is_null()) {
        BOOST_CHECK_EQUAL(response["id"].as_string(), id.as_string());
    }
    BOOST_CHECK(response["error"].is_object());
    BOOST_CHECK(response["error"]["message"].is_string());
    BOOST_CHECK(response["error"]["code"].is_integer());
    BOOST_CHECK_EQUAL(response["error"]["code"].as<int32_t>(), code);

    if (!error_name.empty()) {
        auto data = response["error"]["data"].get_object();
        BOOST_CHECK_EQUAL(data["name"].get_string(), error_name);
    }
}

BOOST_FIXTURE_TEST_SUITE(json_rpc, database_fixture)

    BOOST_AUTO_TEST_CASE(json_rpc_test) {
        try {
            initialize();


            auto &rpc_plugin  = appbase::app().register_plugin<json_rpc_plugin>();
            auto &testing_api = appbase::app().register_plugin<test_plugin::testing_api>();

            {
                boost::program_options::options_description desc;
                rpc_plugin.set_program_options(desc, desc);

                boost::program_options::variables_map options;
                boost::program_options::store(parse_command_line(0, (char**)NULL, desc), options);
                rpc_plugin.plugin_initialize(options);
            }
            {
                boost::program_options::variables_map options;
                testing_api.plugin_initialize(options);
            }

            open_database();

            startup();
            rpc_plugin.plugin_startup();
            testing_api.plugin_startup();


            BOOST_TEST_MESSAGE("--- empty request");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "").get_object();
                check_error_response(response, fc::variant(), JSON_RPC_PARSE_ERROR);
            });

            BOOST_TEST_MESSAGE("--- invalid json sctructure");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{]").get_object();
                check_error_response(response, fc::variant(), JSON_RPC_PARSE_ERROR);
            });

            BOOST_TEST_MESSAGE("--- empty array of request");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "[]").get_object();
                check_error_response(response, fc::variant(), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- empty request object");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{}").get_object();
                check_error_response(response, fc::variant(), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- invalid 'jsonrpc' field");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"1.2\"}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- invalid type of 'jsonrpc' field");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":[]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- invalid 'method' field");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"database_api.get_dynamic_global_properties\"}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- invalid type of 'method' field");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":[]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- missing 'params' field");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\"}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- invalid type of 'params' field");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":1234}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- invalid type of 'args' field");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",{}]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INVALID_REQUEST);
            });

            BOOST_TEST_MESSAGE("--- missing API");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"missing_api\",\"missing_method\"]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_METHOD_NOT_FOUND);
            });

            BOOST_TEST_MESSAGE("--- missing method");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"missing_method\",[]]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_METHOD_NOT_FOUND);
            });

            BOOST_TEST_MESSAGE("--- return UNSUPPORTED_OPERATION when thrown golos::unsupported_operation");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"unsupported_operation\"]]}").get_object();
                check_error_response(response, fc::variant(1u), SERVER_UNSUPPORTED_OPERATION, "unsupported_operation");
            });

            BOOST_TEST_MESSAGE("--- return INVALID_PARAMETER when thrown golos::parameter_exception");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"invalid_parameter\"]]}").get_object();
                check_error_response(response, fc::variant(1u), SERVER_INVALID_PARAMETER, "invalid_parameter");
            });

            BOOST_TEST_MESSAGE("--- return BUSINESS_LOGIC_ERROR when thrown golos::business_exception");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"business_exception\"]]}").get_object();
                check_error_response(response, fc::variant(1u), SERVER_BUSINESS_LOGIC_ERROR, "business_exception");
            });

            BOOST_TEST_MESSAGE("--- return MISSING_AUTHORITY when thrown golos::protocol::tx_missing_authority");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"tx_missing_authority\"]]}").get_object();
                check_error_response(response, fc::variant(1u), SERVER_MISSING_AUTHORITY, "tx_missing_authority");
            });

            BOOST_TEST_MESSAGE("--- return INVALID_OPERATION when thrown golos::protocol::tx_invalid_operation");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"tx_invalid_operation\"]]}").get_object();
                check_error_response(response, fc::variant(1u), SERVER_INVALID_OPERATION, "tx_invalid_operation");
            });

            BOOST_TEST_MESSAGE("--- return INVALID_TRANSACTION when thrown golos::protocol::transaction_exception");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"transaction_exception\"]]}").get_object();
                check_error_response(response, fc::variant(1u), SERVER_INVALID_TRANSACTION, "transaction_exception");
            });

            BOOST_TEST_MESSAGE("--- return INTERNAL_ERROR when thrown golos::golos_exception");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"golos_exception\"]]}").get_object();
                check_error_response(response, fc::variant(1u), SERVER_INTERNAL_ERROR, "golos_exception");
            });


            BOOST_TEST_MESSAGE("--- return INTERNAL_ERROR when thrown fc::exception");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"fc::exception\"]]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INTERNAL_ERROR);
            });

            BOOST_TEST_MESSAGE("--- return INTERNAL_ERROR when thrown std::excption");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"std::exception\"]]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INTERNAL_ERROR);
            });

            BOOST_TEST_MESSAGE("--- return INTERNAL_ERROR when thrown unknown exception");
            BOOST_CHECK_NO_THROW({
                auto response = call(rpc_plugin, "{\"id\":1, \"jsonrpc\":\"2.0\",\"method\":\"call\",\"params\":["
                        "\"testing_api\",\"throw_exception\",[\"...\"]]}").get_object();
                check_error_response(response, fc::variant(1u), JSON_RPC_INTERNAL_ERROR);
            });

        }
        FC_LOG_AND_RETHROW()
    }

BOOST_AUTO_TEST_SUITE_END()
#endif
