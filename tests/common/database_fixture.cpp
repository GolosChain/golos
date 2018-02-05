#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>

#include <golos/utilities/tempdir.hpp>

#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/asset_object.hpp>
#include <golos/chain/objects/steem_objects.hpp>
#include <golos/chain/objects/history_object.hpp>
#include <golos/account_history/account_history_plugin.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/smart_ref_impl.hpp>

#include "database_fixture.hpp"

//using namespace golos::chain::test;

uint32_t STEEMIT_TESTING_GENESIS_TIMESTAMP = 1431700000;

namespace golos {
    namespace chain {

        using std::cout;
        using std::cerr;

        clean_database_fixture::clean_database_fixture() {
            try {
                int argc = boost::unit_test::framework::master_test_suite().argc;
                char **argv = boost::unit_test::framework::master_test_suite().argv;
                for (int i = 1; i < argc; i++) {
                    const std::string arg = argv[i];
                    if (arg == "--record-assert-trip") {
                        fc::enable_record_assert_trip = true;
                    }
                    if (arg == "--show-test-names") {
                        std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name
                                  << std::endl;
                    }
                }
                auto ahplugin = app.register_plugin<golos::account_history::account_history_plugin>();
                db_plugin = app.register_plugin<golos::plugin::debug_node::debug_node_plugin>();
                init_account_pub_key = init_account_priv_key.get_public_key();

                boost::program_options::variables_map options;

                open_database();

                db_plugin->logging = false;
                ahplugin->plugin_initialize(options);
                db_plugin->plugin_initialize(options);

                generate_block();
                db.set_hardfork(STEEMIT_NUM_HARDFORKS);
                generate_block();

                //ahplugin->plugin_startup();
                db_plugin->plugin_startup();
                vest(STEEMIT_INIT_MINER_NAME, 10000);

                // Fill up the rest of the required miners
                for (int i = STEEMIT_NUM_INIT_MINERS; i < STEEMIT_MAX_WITNESSES; i++) {
                    account_create(STEEMIT_INIT_MINER_NAME + fc::to_string(i), init_account_pub_key);
                    fund(STEEMIT_INIT_MINER_NAME + fc::to_string(i), STEEMIT_MIN_PRODUCER_REWARD.amount.value);
                    witness_create(STEEMIT_INIT_MINER_NAME + fc::to_string(i), init_account_priv_key, "foo.bar",
                                   init_account_pub_key, STEEMIT_MIN_PRODUCER_REWARD.amount);
                }

                validate_database();
            } catch (const fc::exception &e) {
                edump((e.to_detail_string()));
                throw;
            }

            return;
        }

        clean_database_fixture::~clean_database_fixture() {
            try {
                // If we're unwinding due to an exception, don't do any more checks.
                // This way, boost test's last checkpoint tells us approximately where the error was.
                if (!std::uncaught_exception()) {
                    BOOST_CHECK(db.get_node_properties().skip_flags == database::skip_nothing);
                }

                if (data_dir) {
                    db.close();
                }
                return;
            } FC_CAPTURE_AND_RETHROW()
        }

        void clean_database_fixture::resize_shared_mem(uint64_t size) {
            db.wipe(data_dir->path(), data_dir->path(), true);
            int argc = boost::unit_test::framework::master_test_suite().argc;
            char **argv = boost::unit_test::framework::master_test_suite().argv;
            for (int i = 1; i < argc; i++) {
                const std::string arg = argv[i];
                if (arg == "--record-assert-trip") {
                    fc::enable_record_assert_trip = true;
                }
                if (arg == "--show-test-names") {
                    std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name
                              << std::endl;
                }
            }
            init_account_pub_key = init_account_priv_key.get_public_key();

            db.open(data_dir->path(), data_dir->path(), INITIAL_TEST_SUPPLY, size, chainbase::database::read_write);

            boost::program_options::variables_map options;


            generate_block();
            db.set_hardfork(STEEMIT_NUM_HARDFORKS);
            generate_block();

            vest(STEEMIT_INIT_MINER_NAME, 10000);

            // Fill up the rest of the required miners
            for (int i = STEEMIT_NUM_INIT_MINERS; i < STEEMIT_MAX_WITNESSES; i++) {
                account_create(STEEMIT_INIT_MINER_NAME + fc::to_string(i), init_account_pub_key);
                fund(STEEMIT_INIT_MINER_NAME + fc::to_string(i), STEEMIT_MIN_PRODUCER_REWARD.amount.value);
                witness_create(STEEMIT_INIT_MINER_NAME + fc::to_string(i), init_account_priv_key, "foo.bar",
                               init_account_pub_key, STEEMIT_MIN_PRODUCER_REWARD.amount);
            }

            validate_database();
        }

        live_database_fixture::live_database_fixture() {
            try {
                ilog("Loading saved chain");
                _chain_dir = fc::current_path() / "test_blockchain";
                FC_ASSERT(fc::exists(_chain_dir), "Requires blockchain to test on in ./test_blockchain");

                auto ahplugin = app.register_plugin<golos::account_history::account_history_plugin>();
                ahplugin->plugin_initialize(boost::program_options::variables_map());

                db.open(_chain_dir, _chain_dir);

                validate_database();
                generate_block();

                ilog("Done loading saved chain");
            } FC_LOG_AND_RETHROW()
        }

        live_database_fixture::~live_database_fixture() {
            try {
                // If we're unwinding due to an exception, don't do any more checks.
                // This way, boost test's last checkpoint tells us approximately where the error was.
                if (!std::uncaught_exception()) {
                    BOOST_CHECK(db.get_node_properties().skip_flags == database::skip_nothing);
                }

                db.pop_block();
                db.close();
                return;
            } FC_LOG_AND_RETHROW()
        }

        fc::ecc::private_key database_fixture::generate_private_key(string seed) {
            return fc::ecc::private_key::regenerate(fc::sha256::hash(seed));
        }

        string database_fixture::generate_anon_acct_name() {
            // names of the form "anon-acct-x123" ; the "x" is necessary
            //    to workaround issue #46
            return "anon-acct-x" + std::to_string(anon_acct_count++);
        }

        void database_fixture::open_database() {
            if (!data_dir) {
                data_dir = fc::temp_directory(graphene::utilities::temp_directory_path());
                db._log_hardforks = false;
                db.open(data_dir->path(), data_dir->path(), INITIAL_TEST_SUPPLY, 1024 * 1024 * 8,
                        chainbase::database::read_write); // 8 MB file for testing
            }
        }

        void database_fixture::verify_asset_supplies(const database &input_db) {
            //wlog("*** Begin asset supply verification ***");
            const asset_dynamic_data_object &core_asset_data = input_db.get<asset_dynamic_data_object, by_asset_name>(
                    STEEM_SYMBOL_NAME);
            BOOST_CHECK(core_asset_data.fee_pool == 0);

            map<asset_name_type, share_type> total_balances;
            map<asset_name_type, share_type> total_debts;
            share_type core_in_orders;
            share_type reported_core_in_orders;

            for (const account_balance_object &b : input_db.get_index<account_balance_index>().indices()) {
                total_balances[b.asset_name] += b.balance;
            }
            for (const force_settlement_object &s : input_db.get_index<force_settlement_index>().indices()) {
                total_balances[s.balance.symbol_name()] += s.balance.amount;
            }
            for (const account_statistics_object &a : input_db.get_index<account_statistics_index>().indices()) {
                reported_core_in_orders += a.total_core_in_orders;
                total_balances[STEEM_SYMBOL_NAME] += a.pending_fees + a.pending_vested_fees;
            }

            for (const collateral_bid_object &b : input_db.get_index<collateral_bid_index>().indices()) {
                total_balances[b.inv_swan_price.base.symbol_name()] += b.inv_swan_price.base.amount;
            }

            for (const limit_order_object &o : input_db.get_index<limit_order_index>().indices()) {
                asset<0, 17, 0> for_sale = o.amount_for_sale();
                if (for_sale.symbol_name() == STEEM_SYMBOL_NAME) {
                    core_in_orders += for_sale.amount;
                }
                total_balances[for_sale.symbol_name()] += for_sale.amount;
                total_balances[STEEM_SYMBOL_NAME] += o.deferred_fee;
            }
            for (const call_order_object &o : input_db.get_index<call_order_index>().indices()) {
                asset<0, 17, 0> col = o.get_collateral();
                if (col.symbol_name() == STEEM_SYMBOL_NAME) {
                    core_in_orders += col.amount;
                }
                total_balances[col.symbol_name()] += col.amount;
                total_debts[o.get_debt().symbol_name()] += o.get_debt().amount;
            }
            for (const asset_object &asset_obj : input_db.get_index<asset_index>().indices()) {
                const auto &dasset_obj = input_db.get_asset_dynamic_data(asset_obj.asset_name);
                total_balances[asset_obj.asset_name] += dasset_obj.accumulated_fees;
                total_balances[STEEM_SYMBOL_NAME] += dasset_obj.fee_pool;
                if (asset_obj.is_market_issued()) {
                    const auto &bad = input_db.get_asset_bitasset_data(asset_obj.asset_name);
                    total_balances[bad.options.short_backing_asset] += bad.settlement_fund;
                }
                total_balances[asset_obj.asset_name] += dasset_obj.confidential_supply.value;
            }
            //            for (const vesting_balance_object &vbo : input_db.get_index<vesting_balance_index>().indices()) {
            //                total_balances[vbo.balance.symbol_name()] += vbo.balance.amount;
            //            }
            //            for (const fba_accumulator_object &fba : input_db.get_index < simple_index < fba_accumulator_object > >
            //                                                     ()) {
            //                total_balances[asset_symbol_type()] += fba.accumulated_fba_fees;
            //            }

            //            total_balances[STEEM_SYMBOL] += input_db.get_dynamic_global_properties().witness_budget;

            for (const auto &item : total_debts) {
                BOOST_CHECK_EQUAL(input_db.get_asset_dynamic_data(item.first).current_supply.value, item.second.value);
            }

            for (const asset_object &asset_obj : input_db.get_index<asset_index>().indices()) {
                BOOST_CHECK_EQUAL(total_balances[asset_obj.asset_name].value,
                                  input_db.get_asset_dynamic_data(asset_obj.asset_name).current_supply.value);
            }

            BOOST_CHECK_EQUAL(core_in_orders.value, reported_core_in_orders.value);
            //   wlog("***  End  asset supply verification ***");
        }

        void database_fixture::verify_account_history_plugin_index() const {
            return;
            if (skip_key_index_test) {
                return;
            }

            const std::shared_ptr<golos::account_history::account_history_plugin> pin = app.get_plugin<
                    golos::account_history::account_history_plugin>("account_history");
            if (pin->tracked_accounts().empty()) {
                /*
                vector< pair< account_name_type, address > > tuples_from_db;
                const auto& primary_account_idx = db.get_index<account_index>().indices().get<by_id>();
                flat_set< public_key_type > acct_addresses;
                acct_addresses.reserve( 2 * GRAPHENE_DEFAULT_MAX_AUTHORITY_MEMBERSHIP + 2 );

                for( const account_object& acct : primary_account_idx )
                {
                   account_name_type account_id = acct.id;
                   acct_addresses.clear();
                   for( const pair< account_name_type, weight_type >& auth : acct.owner.account_auths )
                   {
                      if( auth.first.type() == key_object_type )
                         acct_addresses.insert(  auth.first );
                   }
                   for( const pair< object_id_type, weight_type >& auth : acct.active.auths )
                   {
                      if( auth.first.type() == key_object_type )
                         acct_addresses.insert( auth.first );
                   }
                   acct_addresses.insert( acct.options.get_memo_key()(db).key_address() );
                   for( const address& addr : acct_addresses )
                      tuples_from_db.emplace_back( account_id, addr );
                }

                vector< pair< account_name_type, address > > tuples_from_index;
                tuples_from_index.reserve( tuples_from_db.size() );
                const auto& key_account_idx =
                   db.get_index<golos::account_history::key_account_index>()
                   .indices().get<golos::account_history::by_key>();

                for( const golos::account_history::key_account_object& key_account : key_account_idx )
                {
                   address addr = key_account.key;
                   for( const account_name_type& account_id : key_account.account_ids )
                      tuples_from_index.emplace_back( account_id, addr );
                }

                // TODO:  use function for common functionality
                {
                   // due to hashed index, account_id's may not be in sorted order...
                   std::sort( tuples_from_db.begin(), tuples_from_db.end() );
                   size_t size_before_uniq = tuples_from_db.size();
                   auto last = std::unique( tuples_from_db.begin(), tuples_from_db.end() );
                   tuples_from_db.erase( last, tuples_from_db.end() );
                   // but they should be unique (multiple instances of the same
                   //  address within an account should have been de-duplicated
                   //  by the flat_set above)
                   BOOST_CHECK( tuples_from_db.size() == size_before_uniq );
                }

                {
                   // (address, account) should be de-duplicated by flat_set<>
                   // in key_account_object
                   std::sort( tuples_from_index.begin(), tuples_from_index.end() );
                   auto last = std::unique( tuples_from_index.begin(), tuples_from_index.end() );
                   size_t size_before_uniq = tuples_from_db.size();
                   tuples_from_index.erase( last, tuples_from_index.end() );
                   BOOST_CHECK( tuples_from_index.size() == size_before_uniq );
                }

                //BOOST_CHECK_EQUAL( tuples_from_db, tuples_from_index );
                bool is_equal = true;
                is_equal &= (tuples_from_db.size() == tuples_from_index.size());
                for( size_t i=0,n=tuples_from_db.size(); i<n; i++ )
                   is_equal &= (tuples_from_db[i] == tuples_from_index[i] );

                bool account_history_plugin_index_ok = is_equal;
                BOOST_CHECK( account_history_plugin_index_ok );
                   */
            }
            return;
        }

        void database_fixture::generate_block(uint32_t skip, const fc::ecc::private_key &key, int miss_blocks) {
            skip |= default_skip;
            db_plugin->debug_generate_blocks(graphene::utilities::key_to_wif(key), 1, skip, miss_blocks);
        }

        void database_fixture::generate_blocks(uint32_t block_count) {
            auto produced = db_plugin->debug_generate_blocks(debug_key, block_count, default_skip, 0);
            BOOST_REQUIRE(produced == block_count);
        }

        void database_fixture::generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks) {
            db_plugin->debug_generate_blocks_until(debug_key, timestamp, miss_intermediate_blocks, default_skip);
            BOOST_REQUIRE((db.head_block_time() - timestamp).to_seconds() < STEEMIT_BLOCK_INTERVAL);
        }

        void database_fixture::force_global_settle(const asset_object &what, const price<0, 17, 0> &p) {
            try {
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.operations.clear();
                asset_global_settle_operation<0, 17, 0> sop;
                sop.issuer = what.issuer;
                sop.asset_to_settle = what.asset_name;
                sop.settle_price = p;
                trx.operations.push_back(sop);
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
                verify_asset_supplies(db);
            } FC_CAPTURE_AND_RETHROW((what)(p))
        }

        void database_fixture::force_settle(const account_object &who, const asset<0, 17, 0> &what) {
            try {
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.operations.clear();
                asset_settle_operation<0, 17, 0> sop;
                sop.account = who.name;
                sop.amount = what;
                trx.operations.push_back(sop);
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
                verify_asset_supplies(db);
            } FC_CAPTURE_AND_RETHROW((who)(what))
        }

        const call_order_object *database_fixture::borrow(const account_object &who, const asset<0, 17, 0> &what,
                                                          asset<0, 17, 0> collateral) {
            try {
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.operations.clear();
                call_order_update_operation<0, 17, 0> update;
                update.funding_account = who.name;
                update.delta_collateral = collateral;
                update.delta_debt = what;
                trx.operations.push_back(update);
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
                verify_asset_supplies(db);

                auto &call_idx = db.get_index<call_order_index>().indices().get<by_account>();
                auto itr = call_idx.find(boost::make_tuple(who.name, what.symbol_name()));
                const call_order_object *call_obj = nullptr;

                if (itr != call_idx.end()) {
                    call_obj = &*itr;
                }
                return call_obj;
            } FC_CAPTURE_AND_RETHROW((who.name)(what)(collateral))
        }

        const limit_order_object *database_fixture::create_sell_order(account_name_type user,
                                                                      const asset<0, 17, 0> &amount,
                                                                      const asset<0, 17, 0> &recv) {
            auto r = create_sell_order(db.get_account(user), amount, recv);
            verify_asset_supplies(db);
            return r;
        }

        const limit_order_object *database_fixture::create_sell_order(const account_object &user,
                                                                      const asset<0, 17, 0> &amount,
                                                                      const asset<0, 17, 0> &recv) {
            //wdump((amount)(recv));
            limit_order_create_operation<0, 17, 0> buy_order;
            buy_order.owner = user.name;
            buy_order.amount_to_sell = amount;
            buy_order.min_to_receive = recv;
            trx.operations.push_back(buy_order);
            trx.validate();
            db.push_transaction(trx, ~0);
            trx.operations.clear();
            verify_asset_supplies(db);
            //wdump((processed));
            return db.find<limit_order_object>(buy_order.order_id);
        }

        asset<0, 17, 0> database_fixture::cancel_limit_order(const limit_order_object &order) {
            limit_order_cancel_operation<0, 17, 0> cancel_order;
            cancel_order.owner = order.seller;
            cancel_order.order_id = order.order_id;
            trx.operations.push_back(cancel_order);
            trx.validate();
            db.push_transaction(trx, ~0);
            trx.operations.clear();
            verify_asset_supplies(db);
            return order.amount_for_sale();
        }

        const asset_object &database_fixture::get_asset(const asset_name_type &symbol) const {
            const auto &idx = db.get_index<asset_index>().indices().get<by_asset_name>();
            const auto itr = idx.find(symbol);
            assert(itr != idx.end());
            return *itr;
        }

        const account_object &database_fixture::get_account(const string &name) const {
            const auto &idx = db.get_index<account_index>().indices().get<by_name>();
            const auto itr = idx.find(name);
            assert(itr != idx.end());
            return *itr;
        }

        const asset_object &database_fixture::create_bitasset(const string &name,
                                                              account_name_type issuer /* = STEEMIT_WITNESS_ACCOUNT */,
                                                              uint16_t market_fee_percent /* = 100 */ /* 1% */,
                                                              uint16_t flags /* = charge_market_fee */
        ) {
            try {
                asset_create_operation<0, 17, 0> creator;
                creator.issuer = issuer;
                creator.asset_name = name;
                creator.common_options.max_supply = STEEMIT_MAX_SHARE_SUPPLY;
                creator.precision = 2;
                creator.common_options.market_fee_percent = market_fee_percent;
                if (issuer == STEEMIT_WITNESS_ACCOUNT) {
                    flags |= witness_fed_asset;
                }
                creator.common_options.issuer_permissions = flags;
                creator.common_options.flags = flags & ~global_settle;
                creator.common_options.core_exchange_rate = price<0, 17, 0>(
                        {asset<0, 17, 0>(1, asset_symbol_type(1)), asset<0, 17, 0>(1)});
                creator.bitasset_opts = bitasset_options();
                trx.operations.push_back(std::move(creator));
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();

                return db.get_asset(creator.asset_name);
            } FC_CAPTURE_AND_RETHROW((name)(flags))
        }

        const asset_object &database_fixture::create_prediction_market(const string &name,
                                                                       account_name_type issuer /* = STEEMIT_WITNESS_ACCOUNT */,
                                                                       uint16_t market_fee_percent /* = 100 */ /* 1% */,
                                                                       uint16_t flags /* = charge_market_fee */
        ) {
            try {
                asset_create_operation<0, 17, 0> creator;
                creator.issuer = issuer;
                creator.asset_name = name;
                creator.common_options.max_supply = STEEMIT_MAX_SHARE_SUPPLY;
                creator.precision = STEEMIT_BLOCKCHAIN_PRECISION_DIGITS;
                creator.common_options.market_fee_percent = market_fee_percent;
                creator.common_options.issuer_permissions = flags | global_settle;
                creator.common_options.flags = flags & ~global_settle;
                if (issuer == STEEMIT_WITNESS_ACCOUNT) {
                    creator.common_options.flags |= witness_fed_asset;
                }
                creator.common_options.core_exchange_rate = price<0, 17, 0>(
                        {asset<0, 17, 0>(1, asset_symbol_type(1)), asset<0, 17, 0>(1)});
                creator.bitasset_opts = bitasset_options();
                creator.is_prediction_market = true;
                trx.operations.push_back(std::move(creator));
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();

                return db.get_asset(creator.asset_name);
            } FC_CAPTURE_AND_RETHROW((name)(flags))
        }

        const asset_object &database_fixture::create_user_issued_asset(const string &name) {
            asset_create_operation<0, 17, 0> creator;
            creator.issuer = account_name_type();
            creator.asset_name = name;
            creator.common_options.max_supply = 0;
            creator.precision = 2;
            creator.common_options.core_exchange_rate = price<0, 17, 0>(
                    {asset<0, 17, 0>(1, asset_symbol_type(1)), asset<0, 17, 0>(1)});
            creator.common_options.max_supply = STEEMIT_MAX_SHARE_SUPPLY;
            creator.common_options.flags = charge_market_fee;
            creator.common_options.issuer_permissions = charge_market_fee;
            trx.operations.push_back(std::move(creator));
            trx.validate();
            db.push_transaction(trx, ~0);
            trx.operations.clear();
            return db.get_asset(creator.asset_name);
        }

        const asset_object &database_fixture::create_user_issued_asset(const asset_name_type &name,
                                                                       const account_object &issuer, uint16_t flags) {
            asset_create_operation<0, 17, 0> creator;
            creator.issuer = issuer.name;
            creator.asset_name = name;
            creator.common_options.max_supply = 0;
            creator.precision = 2;
            creator.common_options.core_exchange_rate = price<0, 17, 0>(
                    {asset<0, 17, 0>(1, asset_symbol_type(1)), asset<0, 17, 0>(1)});
            creator.common_options.max_supply = STEEMIT_MAX_SHARE_SUPPLY;
            creator.common_options.flags = flags;
            creator.common_options.issuer_permissions = flags;
            trx.operations.clear();
            trx.operations.push_back(std::move(creator));
            trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            trx.validate();
            db.push_transaction(trx, ~0);
            trx.operations.clear();

            return db.get_asset(creator.asset_name);
        }

        void database_fixture::issue_uia(const account_object &recipient, const asset<0, 17, 0> &amount) {
            BOOST_TEST_MESSAGE("Issuing UIA");
            asset_issue_operation<0, 17, 0> op;
            op.issuer = db.get_asset(amount.symbol_name()).issuer;
            op.asset_to_issue = amount;
            op.issue_to_account = recipient.name;
            trx.operations.push_back(op);
            db.push_transaction(trx, ~0);
            trx.operations.clear();
        }

        void database_fixture::issue_uia(account_name_type recipient_id, const asset<0, 17, 0> &amount) {
            issue_uia(db.get_account(recipient_id), amount);
        }

        void database_fixture::cover(const account_object &who, const asset<0, 17, 0> &what,
                                     const asset<0, 17, 0> &collateral) {
            try {
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.operations.clear();
                call_order_update_operation<0, 17, 0> update;
                update.funding_account = who.name;
                update.delta_collateral = -collateral;
                update.delta_debt = -what;
                trx.operations.push_back(update);

                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
                verify_asset_supplies(db);
            } FC_CAPTURE_AND_RETHROW((who.name)(what)(collateral))
        }


        void database_fixture::bid_collateral(const account_object &who, const asset<0, 17, 0> &to_bid,
                                              const asset<0, 17, 0> &to_cover) {
            try {
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.operations.clear();
                bid_collateral_operation<0, 17, 0> bid;
                bid.bidder = who.name;
                bid.additional_collateral = to_bid;
                bid.debt_covered = to_cover;
                trx.operations.push_back(bid);
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
                verify_asset_supplies(db);
            } FC_CAPTURE_AND_RETHROW((who.name)(to_bid)(to_cover))
        }

        void database_fixture::update_feed_producers(const asset_object &mia, flat_set<account_name_type> producers) {
            try {
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.operations.clear();
                asset_update_feed_producers_operation<0, 17, 0> op;
                op.asset_to_update = mia.asset_name;
                op.issuer = mia.issuer;
                op.new_feed_producers = std::move(producers);
                trx.operations = {std::move(op)};

                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
                verify_asset_supplies(db);
            } FC_CAPTURE_AND_RETHROW((mia)(producers))
        }

        void database_fixture::publish_feed(const asset_object &mia, const account_object &by,
                                            const price_feed<0, 17, 0> &f) {
            trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            trx.operations.clear();

            asset_publish_feed_operation<0, 17, 0> op;
            op.publisher = by.name;
            op.asset_name = mia.asset_name;
            op.feed = f;
            if (op.feed.core_exchange_rate.is_null()) {
                op.feed.core_exchange_rate = op.feed.settlement_price;
            }
            trx.operations.push_back(std::move(op));

            trx.validate();
            db.push_transaction(trx, ~0);
            trx.operations.clear();
            verify_asset_supplies(db);
        }

        const account_object &database_fixture::account_create(const string &name, const string &creator,
                                                               const private_key_type &creator_key,
                                                               const share_type &fee, const public_key_type &key,
                                                               const public_key_type &post_key,
                                                               const string &json_metadata) {
            try {
                account_create_operation<0, 17, 0> op;
                op.new_account_name = name;
                op.creator = creator;
                op.fee = fee;
                op.owner = authority(1, key, 1);
                op.active = authority(1, key, 1);
                op.posting = authority(1, post_key, 1);
                op.memo_key = key;
                op.json_metadata = json_metadata;

                trx.operations.push_back(op);
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.sign(creator_key, db.get_chain_id());
                trx.validate();
                db.push_transaction(trx, 0);
                trx.operations.clear();
                trx.signatures.clear();

                const account_object &acct = db.get_account(name);

                return acct;
            } FC_CAPTURE_AND_RETHROW((name)(creator))
        }

        const account_object &database_fixture::account_create(const string &name, const public_key_type &key,
                                                               const public_key_type &post_key) {
            try {
                return account_create(name, STEEMIT_INIT_MINER_NAME, init_account_priv_key, 100, key, post_key, "");
            } FC_CAPTURE_AND_RETHROW((name));
        }

        const account_object &database_fixture::account_create(const string &name, const public_key_type &key) {
            return account_create(name, key, key);
        }

        const witness_object &database_fixture::witness_create(const string &owner, const private_key_type &owner_key,
                                                               const string &url, const public_key_type &signing_key,
                                                               const share_type &fee) {
            try {
                witness_update_operation<0, 17, 0> op;
                op.owner = owner;
                op.url = url;
                op.block_signing_key = signing_key;
                op.fee = asset<0, 17, 0>(fee, STEEM_SYMBOL_NAME);

                trx.operations.push_back(op);
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.sign(owner_key, db.get_chain_id());
                trx.validate();
                db.push_transaction(trx, 0);
                trx.operations.clear();
                trx.signatures.clear();

                return db.get_witness(owner);
            } FC_CAPTURE_AND_RETHROW((owner)(url))
        }

        void database_fixture::fund(const string &account_name, const share_type &amount) {
            try {
                transfer(STEEMIT_INIT_MINER_NAME, account_name, amount);

            } FC_CAPTURE_AND_RETHROW((account_name)(amount))
        }

        void database_fixture::fund(const string &account_name, const asset<0, 17, 0> &amount) {
            try {
                db_plugin->debug_update([=](database &db) {
                    db.adjust_balance(db.get_account(account_name), amount);

                    db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object &gpo) {
                        if (amount.symbol_name() == STEEM_SYMBOL_NAME) {
                            gpo.current_supply += amount;
                        } else if (amount.symbol_name() == SBD_SYMBOL_NAME) {
                            gpo.current_sbd_supply += amount;
                        }
                    });

                    if (amount.symbol_name() == SBD_SYMBOL_NAME) {
                        const auto &median_feed = db.get_feed_history();
                        if (median_feed.current_median_history.is_null()) {
                            db.modify(median_feed, [&](feed_history_object &f) {
                                f.current_median_history = price<0, 17, 0>(asset<0, 17, 0>(1, SBD_SYMBOL_NAME),
                                                                           asset<0, 17, 0>(1, STEEM_SYMBOL));
                            });
                        }
                    }

                    db.update_virtual_supply();
                }, default_skip);
            } FC_CAPTURE_AND_RETHROW((account_name)(amount))
        }

        void database_fixture::convert(const string &account_name, const asset<0, 17, 0> &amount) {
            try {
                const account_object &account = db.get_account(account_name);

                if (amount.symbol_name() == STEEM_SYMBOL_NAME) {
                    db.adjust_balance(account, -amount);
                    db.adjust_balance(account, db.to_sbd(amount));
                    db.adjust_supply(-amount);
                    db.adjust_supply(db.to_sbd(amount));
                } else if (amount.symbol_name() == SBD_SYMBOL_NAME) {
                    db.adjust_balance(account, -amount);
                    db.adjust_balance(account, db.to_steem(amount));
                    db.adjust_supply(-amount);
                    db.adjust_supply(db.to_steem(amount));
                }
            } FC_CAPTURE_AND_RETHROW((account_name)(amount))
        }

        void database_fixture::transfer(const string &from, const string &to, const asset<0, 17, 0> &amount) {
            try {
                transfer_operation<0, 17, 0> op;
                op.from = from;
                op.to = to;
                op.amount = amount;

                trx.operations.push_back(op);
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
            } FC_CAPTURE_AND_RETHROW((from)(to)(amount))
        }

        void database_fixture::vest(const string &from, const share_type &amount) {
            try {
                transfer_to_vesting_operation<0, 17, 0> op;
                op.from = from;
                op.to = "";
                op.amount = asset<0, 17, 0>(amount, STEEM_SYMBOL_NAME);

                trx.operations.push_back(op);
                trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                trx.validate();
                db.push_transaction(trx, ~0);
                trx.operations.clear();
            } FC_CAPTURE_AND_RETHROW((from)(amount))
        }

        void database_fixture::vest(const string &account, const asset<0, 17, 0> &amount) {
            if (amount.symbol_name() != STEEM_SYMBOL_NAME) {
                return;
            }

            db_plugin->debug_update([=](database &db) {
                db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object &gpo) {
                    gpo.current_supply += amount;
                });

                db.create_vesting(db.get_account(account), amount);

                db.update_virtual_supply();
            }, default_skip);
        }

        void database_fixture::proxy(const string &account, const string &proxy) {
            try {
                account_witness_proxy_operation<0, 17, 0> op;
                op.account = account;
                op.proxy = proxy;
                trx.operations.push_back(op);
                db.push_transaction(trx, ~0);
                trx.operations.clear();
            } FC_CAPTURE_AND_RETHROW((account)(proxy))
        }

        void database_fixture::set_price_feed(const price<0, 17, 0> &new_price) {
            try {
                for (int i = 1; i < 8; i++) {
                    feed_publish_operation<0, 17, 0> op;
                    op.publisher = STEEMIT_INIT_MINER_NAME + fc::to_string(i);
                    op.exchange_rate = new_price;
                    trx.operations.push_back(op);
                    trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                    db.push_transaction(trx, ~0);
                    trx.operations.clear();
                }
            } FC_CAPTURE_AND_RETHROW((new_price))

            generate_blocks(STEEMIT_BLOCKS_PER_HOUR);
#ifdef STEEMIT_BUILD_TESTNET
            BOOST_REQUIRE(!db.skip_price_feed_limit_check ||
                          db.get(feed_history_object::id_type()).current_median_history ==
                          new_price);
#else
            BOOST_REQUIRE(db.get(feed_history_object::id_type()).current_median_history == new_price);
#endif
        }

        void database_fixture::print_market(const string &syma, const string &symb) const {
            const auto &limit_idx = db.get_index<limit_order_index>();
            const auto &price_idx = limit_idx.indices().get<by_price>();

            cerr << std::fixed;
            cerr.precision(5);
            cerr << std::setw(10) << std::left << "NAME" << " ";
            cerr << std::setw(16) << std::right << "FOR SALE" << " ";
            cerr << std::setw(16) << std::right << "FOR WHAT" << " ";
            cerr << std::setw(10) << std::right << "PRICE (S/W)" << " ";
            cerr << std::setw(10) << std::right << "1/PRICE (W/S)" << "\n";
            cerr << string(70, '=') << std::endl;
            auto cur = price_idx.begin();
            while (cur != price_idx.end()) {
                cerr << std::setw(10) << std::left << db.get_account(cur->seller).name.operator std::string() << " ";
                cerr << std::setw(10) << std::right << cur->for_sale.value << " ";
                cerr << std::setw(5) << std::left
                     << db.get_asset(cur->amount_for_sale().symbol_name()).asset_name.operator std::string() << " ";
                cerr << std::setw(10) << std::right << cur->amount_to_receive().amount.value << " ";
                cerr << std::setw(5) << std::left
                     << db.get_asset(cur->amount_to_receive().symbol_name()).asset_name.operator std::string() << " ";
                cerr << std::setw(10) << std::right << cur->sell_price.to_real() << " ";
                cerr << std::setw(10) << std::right << (~cur->sell_price).to_real() << " ";
                cerr << "\n";
                ++cur;
            }
        }

        string database_fixture::pretty(const asset<0, 17, 0> &a) const {
            std::stringstream ss;
            ss << a.amount.value << " ";
            ss << db.get_asset(a.symbol_name()).asset_name.operator std::string();
            return ss.str();
        }

        void database_fixture::print_limit_order(const limit_order_object &cur) const {
            std::cout << std::setw(10) << db.get_account(cur.seller).name.operator std::string() << " ";
            std::cout << std::setw(10) << "LIMIT" << " ";
            std::cout << std::setw(16) << pretty(cur.amount_for_sale()) << " ";
            std::cout << std::setw(16) << pretty(cur.amount_to_receive()) << " ";
            std::cout << std::setw(16) << cur.sell_price.to_real() << " ";
        }

        void database_fixture::print_call_orders() const {
            cout << std::fixed;
            cout.precision(5);
            cout << std::setw(10) << std::left << "NAME" << " ";
            cout << std::setw(10) << std::right << "TYPE" << " ";
            cout << std::setw(16) << std::right << "DEBT" << " ";
            cout << std::setw(16) << std::right << "COLLAT" << " ";
            cout << std::setw(16) << std::right << "CALL PRICE(D/C)" << " ";
            cout << std::setw(16) << std::right << "~CALL PRICE(C/D)" << " ";
            cout << std::setw(16) << std::right << "SWAN(D/C)" << " ";
            cout << std::setw(16) << std::right << "SWAN(C/D)" << "\n";
            cout << string(70, '=');

            for (const call_order_object &o : db.get_index<call_order_index>().indices()) {
                std::cout << "\n";
                cout << std::setw(10) << std::left << db.get_account(o.borrower).name.operator std::string() << " ";
                cout << std::setw(16) << std::right << pretty(o.get_debt()) << " ";
                cout << std::setw(16) << std::right << pretty(o.get_collateral()) << " ";
                cout << std::setw(16) << std::right << o.call_price.to_real() << " ";
                cout << std::setw(16) << std::right << (~o.call_price).to_real() << " ";
                cout << std::setw(16) << std::right << (o.get_debt() / o.get_collateral()).to_real() << " ";
                cout << std::setw(16) << std::right << (~(o.get_debt() / o.get_collateral())).to_real() << " ";
            }
            std::cout << "\n";
        }

        void database_fixture::print_joint_market(const string &syma, const string &symb) const {
            cout << std::fixed;
            cout.precision(5);

            cout << std::setw(10) << std::left << "NAME" << " ";
            cout << std::setw(10) << std::right << "TYPE" << " ";
            cout << std::setw(16) << std::right << "FOR SALE" << " ";
            cout << std::setw(16) << std::right << "FOR WHAT" << " ";
            cout << std::setw(16) << std::right << "PRICE (S/W)" << "\n";
            cout << string(70, '=');

            const auto &limit_idx = db.get_index<limit_order_index>();
            const auto &limit_price_idx = limit_idx.indices().get<by_price>();

            auto limit_itr = limit_price_idx.begin();
            while (limit_itr != limit_price_idx.end()) {
                std::cout << std::endl;
                print_limit_order(*limit_itr);
                ++limit_itr;
            }
        }

        int64_t database_fixture::get_balance(account_name_type account, const asset_name_type &a) const {
            return db.get_balance(account, a).amount.value;
        }

        int64_t database_fixture::get_balance(const account_object &account, const asset_object &a) const {
            return db.get_balance(account.name, a.asset_name).amount.value;
        }

        void database_fixture::sign(signed_transaction &trx, const fc::ecc::private_key &key) {
            trx.sign(key, db.get_chain_id());
        }

        vector<operation> database_fixture::get_last_operations(uint32_t num_ops) {
            vector<operation> ops;
            const auto &acc_hist_idx = db.get_index<account_history_index>().indices().get<by_id>();
            auto itr = acc_hist_idx.end();

            while (itr != acc_hist_idx.begin() && ops.size() < num_ops) {
                itr--;
                ops.push_back(fc::raw::unpack<golos::chain::operation>(db.get(itr->op).serialized_op));
            }

            return ops;
        }

        void database_fixture::validate_database() {
            try {
                db.validate_invariants();
            } FC_LOG_AND_RETHROW();
        }

        namespace test {

            bool _push_block(database &db, const signed_block &b, uint32_t skip_flags /* = 0 */ ) {
                return db.push_block(b, skip_flags);
            }

            void _push_transaction(database &db, const signed_transaction &tx, uint32_t skip_flags /* = 0 */ ) {
                try {
                    db.push_transaction(tx, skip_flags);
                } FC_CAPTURE_AND_RETHROW((tx))
            }

        } // golos::chain::test
    }
} // golos::chain
