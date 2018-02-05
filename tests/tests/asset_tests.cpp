#include <boost/test/unit_test.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/database_exceptions.hpp>
#include <golos/version/hardfork.hpp>

#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/asset_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace golos::chain;
using namespace golos::chain::test;

BOOST_FIXTURE_TEST_SUITE(uia_tests, database_fixture)

BOOST_AUTO_TEST_CASE(create_advanced_uia) {
        try {
            asset_name_type test_asset_id = "ADVANCED";
            asset_create_operation<0, 17, 0> creator;
            creator.issuer = account_name_type();
            creator.asset_name = "ADVANCED";
            creator.common_options.max_supply = 100000000;
            creator.precision = 2;
            creator.common_options.market_fee_percent = STEEMIT_MAX_MARKET_FEE_PERCENT / 100; /*1%*/
            creator.common_options.issuer_permissions =
                    charge_market_fee | white_list | override_authority | transfer_restricted | disable_confidential;
            creator.common_options.flags = charge_market_fee | white_list | override_authority | disable_confidential;
            creator.common_options.core_exchange_rate = price<0, 17, 0>({asset<0, 17, 0>(2), asset<0, 17, 0>(1, asset_symbol_type(1))});
            creator.common_options.whitelist_authorities = creator.common_options.blacklist_authorities = {
                    account_name_type()
            };
            trx.operations.push_back(std::move(creator));
            PUSH_TX(db, trx, ~0);

            const asset_object &test_asset = db.get_asset(test_asset_id);
            BOOST_CHECK(test_asset.asset_name == test_asset_id);
            BOOST_CHECK(typename BOOST_IDENTITY_TYPE((asset<0, 17, 0>))(1, test_asset_id) * test_asset.options.core_exchange_rate == typename BOOST_IDENTITY_TYPE((asset<0, 17, 0>))(2));
            BOOST_CHECK(test_asset.options.flags & white_list);
            BOOST_CHECK(test_asset.options.max_supply == 100000000);
            BOOST_CHECK(!test_asset.is_market_issued());
            BOOST_CHECK(test_asset.options.market_fee_percent == STEEMIT_MAX_MARKET_FEE_PERCENT / 100);

            const asset_dynamic_data_object &test_asset_dynamic_data = db.get_asset_dynamic_data(test_asset_id);
            BOOST_CHECK(test_asset_dynamic_data.current_supply == 0);
            BOOST_CHECK(test_asset_dynamic_data.accumulated_fees == 0);
            BOOST_CHECK(test_asset_dynamic_data.fee_pool == 0);
        } catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
}

BOOST_AUTO_TEST_CASE(override_transfer_test) {
        try {
            ACTORS((dan)(eric)(sam));
            const asset_object &advanced = create_user_issued_asset("ADVANCED", sam, override_authority);
            BOOST_TEST_MESSAGE("Issuing 1000 ADVANCED to dan");
            issue_uia(dan, advanced.amount(1000));
            BOOST_TEST_MESSAGE("Checking dan's balance");
            BOOST_REQUIRE_EQUAL(get_balance(dan, advanced), 1000);

            override_transfer_operation<0, 17, 0> otrans;
            otrans.issuer = advanced.issuer;
            otrans.from = dan.name;
            otrans.to = eric.name;
            otrans.amount = advanced.amount(100);
            trx.operations.push_back(otrans);

            BOOST_TEST_MESSAGE("Require throwing without signature");
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, 0), golos::protocol::exceptions::transaction::tx_missing_active_auth<>);
            BOOST_TEST_MESSAGE("Require throwing with dan's signature");
            sign(trx, dan_private_key);
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, 0), golos::protocol::exceptions::transaction::tx_missing_active_auth<>);
            BOOST_TEST_MESSAGE("Pass with issuer's signature");
            trx.signatures.clear();
            sign(trx, sam_private_key);
            PUSH_TX(db, trx, 0);

            BOOST_REQUIRE_EQUAL(get_balance(dan, advanced), 900);
            BOOST_REQUIRE_EQUAL(get_balance(eric, advanced), 100);
        } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(override_transfer_test2) {
        try {
            ACTORS((dan)(eric)(sam));
            const asset_object &advanced = create_user_issued_asset("ADVANCED", sam, 0);
            issue_uia(dan, advanced.amount(1000));
            BOOST_REQUIRE_EQUAL(get_balance(dan, advanced), 1000);

            trx.operations.clear();
            override_transfer_operation<0, 17, 0> otrans;
            otrans.issuer = advanced.issuer;
            otrans.from = dan.name;
            otrans.to = eric.name;
            otrans.amount = advanced.amount(100);
            trx.operations.push_back(otrans);

            BOOST_TEST_MESSAGE("Require throwing without signature");
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, 0), fc::exception);
            BOOST_TEST_MESSAGE("Require throwing with dan's signature");
            sign(trx, dan_private_key);
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, 0), fc::exception);
            BOOST_TEST_MESSAGE("Fail because overide_authority flag is not set");
            trx.signatures.clear();
            sign(trx, sam_private_key);
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, 0), fc::exception);

            BOOST_REQUIRE_EQUAL(get_balance(dan, advanced), 1000);
            BOOST_REQUIRE_EQUAL(get_balance(eric, advanced), 0);
        } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(issue_whitelist_uia) {
        try {
            account_name_type izzy_id = account_create("izzy").name;
            const asset_name_type uia_id = create_user_issued_asset(
                    "ADVANCED", db.get_account(izzy_id), white_list).asset_name;
            account_name_type nathan_id = account_create("nathan").name;
            account_name_type vikram_id = account_create("vikram").name;
            trx.clear();

            asset_issue_operation<0, 17, 0> op;
            op.issuer = db.get_asset(uia_id).issuer;
            op.asset_to_issue = asset<0, 17, 0>(1000, uia_id);
            op.issue_to_account = nathan_id;
            trx.operations.emplace_back(op);
            trx.set_expiration(db.head_block_time() +
                               STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            PUSH_TX(db, trx, ~0);

            BOOST_CHECK(db.is_authorized_asset(db.get_account(nathan_id), db.get_asset(uia_id)));
            BOOST_CHECK_EQUAL(get_balance(nathan_id, uia_id), 1000);

            // Make a whitelist, now it should fail
            {
                BOOST_TEST_MESSAGE("Changing the whitelist authority");
                asset_update_operation<0, 17, 0> uop;
                uop.issuer = izzy_id;
                uop.asset_to_update = uia_id;
                uop.new_options = db.get_asset(uia_id).options;
                uop.new_options.whitelist_authorities.insert(izzy_id);
                trx.operations.back() = uop;
                PUSH_TX(db, trx, ~0);
                BOOST_CHECK(db.get_asset(uia_id).options.whitelist_authorities.find(izzy_id) !=
                            db.get_asset(uia_id).options.whitelist_authorities.end());
            }

            // Fail because there is a whitelist authority and I'm not whitelisted
            trx.operations.back() = op;
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);

            account_whitelist_operation<0, 17, 0> wop;
            wop.authorizing_account = izzy_id;
            wop.account_to_list = vikram_id;
            wop.new_listing = account_whitelist_operation<0, 17, 0>::white_listed;

            trx.operations.back() = wop;
            // Fail because whitelist function is restricted to members only
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);
            trx.operations.clear();
            trx.operations.push_back(wop);
            PUSH_TX(db, trx, ~0);

            // Still fail after an irrelevant account was added
            trx.operations.back() = op;
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);

            wop.account_to_list = nathan_id;
            trx.operations.back() = wop;
            PUSH_TX(db, trx, ~0);
            trx.operations.back() = op;
            BOOST_CHECK_EQUAL(get_balance(nathan_id, uia_id), 1000);
            // Finally succeed when we were whitelisted
            PUSH_TX(db, trx, ~0);
            BOOST_CHECK_EQUAL(get_balance(nathan_id, uia_id), 2000);

        } catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
}

BOOST_AUTO_TEST_CASE(transfer_whitelist_uia) {
        try {
            INVOKE(issue_whitelist_uia);
            const asset_object &advanced = get_asset("ADVANCED");
            const account_object &nathan = get_account("nathan");
            const account_object &dan = account_create("dan");
            account_name_type izzy_id = get_account("izzy").name;
            trx.clear();

            BOOST_TEST_MESSAGE("Atempting to transfer asset ADVANCED from nathan to dan when dan is not whitelisted, should fail");
            transfer_operation<0, 17, 0> op;
            op.from = nathan.name;
            op.to = dan.name;
            op.amount = advanced.amount(100); //({advanced.amount(0), nathan.id, dan.name, advanced.amount(100)});
            trx.operations.push_back(op);
            //Fail because dan is not whitelisted.
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), typename BOOST_IDENTITY_TYPE((golos::chain::exceptions::operations::transfer::to_account_not_whitelisted<0, 17, 0>)));

            BOOST_TEST_MESSAGE("Adding dan to whitelist for asset ADVANCED");
            account_whitelist_operation<0, 17, 0> wop;
            wop.authorizing_account = izzy_id;
            wop.account_to_list = dan.name;
            wop.new_listing = account_whitelist_operation<0, 17, 0>::white_listed;
            trx.operations.back() = wop;
            PUSH_TX(db, trx, ~0);
            BOOST_TEST_MESSAGE("Attempting to transfer from nathan to dan after whitelisting dan, should succeed");
            trx.operations.back() = op;
            PUSH_TX(db, trx, ~0);

            BOOST_CHECK_EQUAL(get_balance(nathan, advanced), 1900);
            BOOST_CHECK_EQUAL(get_balance(dan, advanced), 100);

            BOOST_TEST_MESSAGE("Attempting to blacklist nathan");
            {
                BOOST_TEST_MESSAGE("Changing the blacklist authority");
                asset_update_operation<0, 17, 0> uop;
                uop.issuer = izzy_id;
                uop.asset_to_update = advanced.asset_name;
                uop.new_options = advanced.options;
                uop.new_options.blacklist_authorities.insert(izzy_id);
                trx.operations.back() = uop;
                PUSH_TX(db, trx, ~0);
                BOOST_CHECK(advanced.options.blacklist_authorities.find(izzy_id) !=
                            advanced.options.blacklist_authorities.end());
            }

            wop.new_listing |= account_whitelist_operation<0, 17, 0>::black_listed;
            wop.account_to_list = nathan.name;
            trx.operations.back() = wop;
            PUSH_TX(db, trx, ~0);
            BOOST_CHECK(!(db.is_authorized_asset(nathan, advanced)));

            BOOST_TEST_MESSAGE("Attempting to transfer from nathan after blacklisting, should fail");
            op.amount = advanced.amount(50);
            trx.operations.back() = op;

            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);

            BOOST_TEST_MESSAGE("Attempting to burn from nathan after blacklisting, should fail");
            asset_reserve_operation<0, 17, 0> burn;
            burn.payer = nathan.name;
            burn.amount_to_reserve = advanced.amount(10);
            trx.operations.back() = burn;
            //Fail because nathan is blacklisted
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);
            BOOST_TEST_MESSAGE("Attempting transfer from dan back to nathan, should fail because nathan is blacklisted");
            std::swap(op.from, op.to);
            trx.operations.back() = op;
            //Fail because nathan is blacklisted
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);

            {
                BOOST_TEST_MESSAGE("Changing the blacklist authority to dan");
                asset_update_operation<0, 17, 0> op;
                op.issuer = izzy_id;
                op.asset_to_update = advanced.asset_name;
                op.new_options = advanced.options;
                op.new_options.blacklist_authorities.clear();
                op.new_options.blacklist_authorities.insert(dan.name);
                trx.operations.back() = op;
                PUSH_TX(db, trx, ~0);
                BOOST_CHECK(advanced.options.blacklist_authorities.find(dan.name) !=
                            advanced.options.blacklist_authorities.end());
            }

            BOOST_TEST_MESSAGE("Attempting to transfer from dan back to nathan");
            trx.operations.back() = op;
            PUSH_TX(db, trx, ~0);
            BOOST_CHECK_EQUAL(get_balance(nathan, advanced), 1950);
            BOOST_CHECK_EQUAL(get_balance(dan, advanced), 50);

            BOOST_TEST_MESSAGE("Blacklisting nathan by dan");
            wop.authorizing_account = dan.name;
            wop.account_to_list = nathan.name;
            wop.new_listing = account_whitelist_operation<0, 17, 0>::black_listed;
            trx.operations.back() = wop;
            PUSH_TX(db, trx, ~0);

            trx.operations.back() = op;
            //Fail because nathan is blacklisted
            BOOST_CHECK(!db.is_authorized_asset(nathan, advanced));
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);

            //Remove nathan from committee's whitelist, add him to dan's. This should not authorize him to hold ADVANCED.
            wop.authorizing_account = izzy_id;
            wop.account_to_list = nathan.name;
            wop.new_listing = account_whitelist_operation<0, 17, 0>::no_listing;
            trx.operations.back() = wop;
            PUSH_TX(db, trx, ~0);
            wop.authorizing_account = dan.name;
            wop.account_to_list = nathan.name;
            wop.new_listing = account_whitelist_operation<0, 17, 0>::white_listed;
            trx.operations.back() = wop;
            PUSH_TX(db, trx, ~0);

            trx.operations.back() = op;
            //Fail because nathan is not whitelisted
            BOOST_CHECK(!db.is_authorized_asset(nathan, advanced));
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, trx, ~0), fc::exception);

            burn.payer = dan.name;
            burn.amount_to_reserve = advanced.amount(10);
            trx.operations.back() = burn;
            PUSH_TX(db, trx, ~0);
            BOOST_CHECK_EQUAL(get_balance(dan, advanced), 40);
        } catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
}

/**
 * verify that issuers can halt transfers
 */
BOOST_AUTO_TEST_CASE(transfer_restricted_test) {
        try {
            ACTORS((sam)(alice)(bob));

            BOOST_TEST_MESSAGE("Issuing 1000 UIA to Alice");

            auto _issue_uia = [&](const account_object &recipient, asset<0, 17, 0> amount) {
                asset_issue_operation<0, 17, 0> op;
                op.issuer = db.get_asset(amount.symbol_name()).issuer;
                op.asset_to_issue = amount;
                op.issue_to_account = recipient.name;
                transaction tx;
                tx.operations.push_back(op);
                trx.set_expiration(db.head_block_time() +
                                   STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                PUSH_TX(db, tx, database::skip_authority_check | database::skip_tapos_check |
                                database::skip_transaction_signatures);
            };

            const asset_object &uia = create_user_issued_asset("TXRX", sam, transfer_restricted);
            _issue_uia(alice, uia.amount(1000));

            auto _restrict_xfer = [&](bool xfer_flag) {
                asset_update_operation<0, 17, 0> op;
                op.issuer = sam_id;
                op.asset_to_update = uia.asset_name;
                op.new_options = uia.options;
                if (xfer_flag) {
                    op.new_options.flags |= transfer_restricted;
                } else {
                    op.new_options.flags &= ~transfer_restricted;
                }
                transaction tx;
                tx.operations.push_back(op);
                trx.set_expiration(db.head_block_time() +
                                   STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                PUSH_TX(db, tx, database::skip_authority_check | database::skip_tapos_check |
                                database::skip_transaction_signatures);
            };

            BOOST_TEST_MESSAGE("Enable transfer_restricted, send fails");

            transfer_operation<0, 17, 0> xfer_op;
            xfer_op.from = alice_id;
            xfer_op.to = bob_id;
            xfer_op.amount = uia.amount(100);
            signed_transaction xfer_tx;
            xfer_tx.operations.push_back(xfer_op);
            trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            sign(xfer_tx, alice_private_key);

            _restrict_xfer(true);
            STEEMIT_REQUIRE_THROW(PUSH_TX(db, xfer_tx), typename BOOST_IDENTITY_TYPE((golos::chain::exceptions::operations::transfer::restricted_transfer_asset<0, 17, 0>)));

            BOOST_TEST_MESSAGE("Disable transfer_restricted, send succeeds");

            _restrict_xfer(false);
            PUSH_TX(db, xfer_tx);

            xfer_op.amount = uia.amount(101);

        }
        catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
}

BOOST_AUTO_TEST_CASE(asset_name_test) {
        try {
            ACTORS((alice)(bob));

            auto has_asset = [&](asset_name_type symbol) -> bool {
                const auto &assets_by_symbol = db.get_index<asset_index>().indices().get<by_asset_name>();
                return assets_by_symbol.find(symbol) != assets_by_symbol.end();
            };

            // Alice creates asset "ALPHA"
            BOOST_CHECK(!has_asset("ALPHA"));
            BOOST_CHECK(!has_asset("ALPHA.ONE"));
            create_user_issued_asset("ALPHA", db.get_account(alice_id), 0);
            BOOST_CHECK(has_asset("ALPHA"));
            BOOST_CHECK(!has_asset("ALPHA.ONE"));

            // Nobody can create another asset named ALPHA
            STEEMIT_REQUIRE_THROW(create_user_issued_asset("ALPHA", db.get_account(bob_id), 0), fc::exception);
            BOOST_CHECK(has_asset("ALPHA"));
            BOOST_CHECK(!has_asset("ALPHA.ONE"));
            STEEMIT_REQUIRE_THROW(create_user_issued_asset("ALPHA", db.get_account(alice_id), 0), fc::exception);
            BOOST_CHECK(has_asset("ALPHA"));
            BOOST_CHECK(!has_asset("ALPHA.ONE"));

            // Bob can't create ALPHA.ONE
            STEEMIT_REQUIRE_THROW(create_user_issued_asset("ALPHA.ONE", db.get_account(bob_id), 0), fc::exception);
            BOOST_CHECK(has_asset("ALPHA"));
            BOOST_CHECK(!has_asset("ALPHA.ONE"));

            // Alice can create it
            create_user_issued_asset("ALPHA.ONE", db.get_account(alice_id), 0);
            BOOST_CHECK(has_asset("ALPHA"));
            BOOST_CHECK(has_asset("ALPHA.ONE"));
        }
        catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
}

BOOST_AUTO_TEST_SUITE_END()
