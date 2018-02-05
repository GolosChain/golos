#ifndef DATABASE_FIXTURE_HPP
#define DATABASE_FIXTURE_HPP

#include <golos/application/application.hpp>
#include <golos/chain/database.hpp>

#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>

#include <golos/plugins/debug_node/debug_node_plugin.hpp>

#include <golos/utilities/key_conversion.hpp>

#include <iostream>

#define INITIAL_TEST_SUPPLY (10000000000ll)
using namespace graphene::db;

extern uint32_t ( STEEMIT_TESTING_GENESIS_TIMESTAMP );

#define PUSH_TX \
   golos::chain::test::_push_transaction

#define PUSH_BLOCK \
   golos::chain::test::_push_block

// See below
#define REQUIRE_OP_VALIDATION_SUCCESS(op, field, value) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   op.validate(); \
   op.field = temp; \
}
#define REQUIRE_OP_EVALUATION_SUCCESS(op, field, value) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = temp; \
   db.push_transaction( trx, ~0 ); \
}

/*#define STEEMIT_REQUIRE_THROW( expr, exc_type )          \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "STEEMIT_REQUIRE_THROW begin "        \
         << req_throw_info << std::endl;                  \
   BOOST_REQUIRE_THROW( expr, exc_type );                 \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "STEEMIT_REQUIRE_THROW end "          \
         << req_throw_info << std::endl;                  \
}*/

#define STEEMIT_REQUIRE_THROW(expr, exc_type)          \
   BOOST_REQUIRE_THROW( expr, exc_type );

#define STEEMIT_CHECK_THROW(expr, exc_type)            \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "STEEMIT_CHECK_THROW begin "          \
         << req_throw_info << std::endl;                  \
   BOOST_CHECK_THROW( expr, exc_type );                   \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "STEEMIT_CHECK_THROW end "            \
         << req_throw_info << std::endl;                  \
}

#define REQUIRE_OP_VALIDATION_FAILURE_2(op, field, value, exc_type) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   STEEMIT_REQUIRE_THROW( op.validate(), exc_type ); \
   op.field = temp; \
}
#define REQUIRE_OP_VALIDATION_FAILURE(op, field, value) \
   REQUIRE_OP_VALIDATION_FAILURE_2( op, field, value, fc::exception )

#define REQUIRE_THROW_WITH_VALUE_2(op, field, value, exc_type) \
{ \
   auto bak = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = bak; \
   STEEMIT_REQUIRE_THROW(db.push_transaction(trx, ~0), exc_type); \
}

#define REQUIRE_THROW_WITH_VALUE(op, field, value) \
   REQUIRE_THROW_WITH_VALUE_2( op, field, value, fc::exception )

///This simply resets v back to its default-constructed value. Requires v to have a working assingment operator and
/// default constructor.
#define RESET(v) v = decltype(v)()
///This allows me to build consecutive test cases. It's pretty ugly, but it works well enough for unit tests.
/// i.e. This allows a test on update_account to begin with the database at the end state of create_account.
#define INVOKE(test) ((struct test*)this)->test_method(); trx.clear()

#define PREP_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name));   \
   fc::ecc::private_key name ## _post_key = generate_private_key(std::string( BOOST_PP_STRINGIZE(name) ) + "_post" ); \
   public_key_type name ## _public_key = name ## _private_key.get_public_key();

#define ACTOR(account_name) \
   PREP_ACTOR(account_name) \
   const auto& account_name = account_create(BOOST_PP_STRINGIZE(account_name), account_name ## _public_key, account_name ## _post_key.get_public_key()); \
   account_name_type account_name ## _id = account_name.name; (void)account_name ## _id;

#define GET_ACTOR(name) \
   fc::ecc::private_key name ## _private_key = generate_private_key(BOOST_PP_STRINGIZE(name)); \
   const account_object& name = get_account(BOOST_PP_STRINGIZE(name)); \
   account_object::id_type name ## _id = name.id; \
   (void)name ##_id

#define ACTORS_IMPL(r, data, elem) ACTOR(elem)
#define ACTORS(names) BOOST_PP_SEQ_FOR_EACH(ACTORS_IMPL, ~, names) \
   validate_database();

namespace golos {
    namespace chain {

        using namespace golos::protocol;

        struct database_fixture {
            // the reason we use an application is to exercise the indexes of built-in
            //   plugins
            golos::application::application app;
            chain::database &db;
            signed_transaction trx;
            public_key_type committee_key;
            account_name_type committee_account;
            fc::ecc::private_key private_key = fc::ecc::private_key::generate();
            fc::ecc::private_key init_account_priv_key;
            string debug_key = graphene::utilities::key_to_wif(init_account_priv_key);
            public_key_type init_account_pub_key = init_account_priv_key.get_public_key();
            uint32_t default_skip = 0 | database::skip_undo_history_check | database::skip_authority_check;

            std::shared_ptr<golos::plugin::debug_node::debug_node_plugin> db_plugin;

            optional<fc::temp_directory> data_dir;
            bool skip_key_index_test = false;

            uint32_t anon_acct_count;

            database_fixture() : app(), db(*app.chain_database()) {
            }

            virtual ~database_fixture() {

            }

            static fc::ecc::private_key generate_private_key(string seed);

            string generate_anon_acct_name();

            void open_database();

            static void verify_asset_supplies(const database &input_db);

            void verify_account_history_plugin_index() const;

            void generate_block(uint32_t skip = 0,
                                const fc::ecc::private_key &key = generate_private_key(BLOCKCHAIN_NAME),
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

            void force_global_settle(const asset_object &what, const price<0, 17, 0> &p);

            void force_settle(const account_name_type &who, const asset<0, 17, 0> &what) {
                return force_settle(db.get_account(who), what);
            }

            void force_settle(const account_object &who, const asset<0, 17, 0> &what);

            void update_feed_producers(const asset_name_type &mia, flat_set<account_name_type> producers) {
                update_feed_producers(db.get_asset(mia), producers);
            }

            void update_feed_producers(const asset_object &mia, flat_set<account_name_type> producers);

            void publish_feed(const asset_name_type &mia, const account_name_type &by, const price_feed<0, 17, 0> &f) {
                publish_feed(db.get_asset(mia), db.get_account(by), f);
            }

            void publish_feed(const asset_object &mia, const account_object &by, const price_feed<0, 17, 0> &f);

            const limit_order_object *create_sell_order(account_name_type user, const asset<0, 17, 0> &amount,
                                                        const asset<0, 17, 0> &recv);

            const limit_order_object *create_sell_order(const account_object &user, const asset<0, 17, 0> &amount,
                                                        const asset<0, 17, 0> &recv);

            asset<0, 17, 0> cancel_limit_order(const limit_order_object &order);

            const call_order_object *borrow(const account_name_type &who, const asset<0, 17, 0> &what,
                                            asset<0, 17, 0> collateral) {
                return borrow(db.get_account(who), std::move(what), std::move(collateral));
            }

            const call_order_object *borrow(const account_object &who, const asset<0, 17, 0> &what,
                                            asset<0, 17, 0> collateral);

            void cover(const account_name_type &who, const asset<0, 17, 0> &what, const asset<0, 17, 0> &collateral_freed) {
                cover(db.get_account(who), std::move(what), std::move(collateral_freed));
            }

            void cover(const account_object &who, const asset<0, 17, 0> &what, const asset<0, 17, 0> &collateral_freed);

            void bid_collateral(const account_object &who, const asset<0, 17, 0> &to_bid,
                                const asset<0, 17, 0> &to_cover);

            const asset_object &get_asset(const asset_name_type &symbol) const;

            const account_object &get_account(const string &name) const;

            const asset_object &create_bitasset(const string &name, account_name_type issuer = STEEMIT_WITNESS_ACCOUNT,
                                                uint16_t market_fee_percent = 100 /*1%*/,
                                                uint16_t flags = charge_market_fee);

            const asset_object &create_prediction_market(const string &name,
                                                         account_name_type issuer = STEEMIT_WITNESS_ACCOUNT,
                                                         uint16_t market_fee_percent = 100 /*1%*/,
                                                         uint16_t flags = charge_market_fee);

            const asset_object &create_user_issued_asset(const string &name);

            const asset_object &create_user_issued_asset(const asset_name_type &name, const account_object &issuer,
                                                         uint16_t flags);

            void issue_uia(const account_object &recipient, const asset<0, 17, 0> &amount);

            void issue_uia(account_name_type recipient_id, const asset<0, 17, 0> &amount);

            const account_object &account_create(const string &name, const string &creator,
                                                 const private_key_type &creator_key, const share_type &fee,
                                                 const public_key_type &key, const public_key_type &post_key,
                                                 const string &json_metadata);

            const account_object &account_create(const string &name, const public_key_type &key,
                                                 const public_key_type &post_key);

            const account_object &account_create(const string &name, const public_key_type &key = public_key_type());


            const witness_object &witness_create(const string &owner, const private_key_type &owner_key,
                                                 const string &url, const public_key_type &signing_key,
                                                 const share_type &fee);

            void fund(const string &account_name, const share_type &amount = 500000);

            void fund(const string &account_name, const asset<0, 17, 0> &amount);

            void transfer(const string &from, const string &to, const asset<0, 17, 0> &steem);

            void convert(const string &account_name, const asset<0, 17, 0> &amount);

            void vest(const string &from, const share_type &amount);

            void vest(const string &account, const asset<0, 17, 0> &amount);

            void proxy(const string &account, const string &proxy);

            void set_price_feed(const price<0, 17, 0> &new_price);

            void print_market(const string &syma, const string &symb) const;

            string pretty(const asset<0, 17, 0> &a) const;

            void print_limit_order(const limit_order_object &cur) const;

            void print_call_orders() const;

            void print_joint_market(const string &syma, const string &symb) const;

            int64_t get_balance(account_name_type account, const asset_name_type &a) const;

            int64_t get_balance(const account_object &account, const asset_object &a) const;

            void sign(signed_transaction &trx, const fc::ecc::private_key &key);

            vector<operation> get_last_operations(uint32_t ops);

            void validate_database();
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

        namespace test {
            bool _push_block(database &db, const signed_block &b, uint32_t skip_flags = 0);

            void _push_transaction(database &db, const signed_transaction &tx, uint32_t skip_flags = 0);
        }
    }
}
#endif