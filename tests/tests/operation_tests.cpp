#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <golos/protocol/exceptions.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/hardfork.hpp>
#include <golos/chain/steem_objects.hpp>

#include <golos/api/account_api_object.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/io/json.hpp>

#include "database_fixture.hpp"
#include "comment_reward.hpp"
#include "helpers.hpp"
#include "comment_reward.hpp"


#include <cmath>
#include <iostream>
#include <stdexcept>
#include <algorithm>

using namespace golos;
using namespace golos::api;
using namespace golos::chain;
using namespace golos::protocol;
using golos::plugins::social_network::comment_content_object;
using std::string;


#define BAD_UTF8_STRING "\xc3\x28"


BOOST_FIXTURE_TEST_SUITE(operation_tests, clean_database_fixture)

    BOOST_AUTO_TEST_CASE(account_create_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_create_validate");
            account_create_operation op;

            private_key_type priv_key = generate_private_key("temp_key");

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.fee = ASSET("10.000 GOLOS");
            op.new_account_name = "bob";
            op.creator = STEEMIT_INIT_MINER_NAME;
            op.owner = authority(1, priv_key.get_public_key(), 1);
            op.active = authority(2, priv_key.get_public_key(), 2);
            op.memo_key = priv_key.get_public_key();
            op.json_metadata = "{\"foo\":\"bar\"}";
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- failed when 'new_account_name' is empty");
            op.new_account_name = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "new_account_name"));

            BOOST_TEST_MESSAGE("--- failed when 'fee' not in GOLOS");
            op.new_account_name = "bob";
            op.fee = ASSET("10.000 GBG");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "fee"));

            BOOST_TEST_MESSAGE("--- failed when 'fee' is negative");
            op.fee = ASSET("-10.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "fee"));

            BOOST_TEST_MESSAGE("--- failed when 'json_metadata' is invalid");
            op.fee = ASSET("10.000 GOLOS");
            op.json_metadata = "[}";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "json_metadata"));

        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_create_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_create_authorities");

            signed_transaction tx;
            ACTORS((alice));

            private_key_type priv_key = generate_private_key("temp_key");

            account_create_operation op;
            op.fee = asset(10, STEEM_SYMBOL);
            op.new_account_name = "bob";
            op.creator = STEEMIT_INIT_MINER_NAME;
            op.owner = authority(1, priv_key.get_public_key(), 1);
            op.active = authority(2, priv_key.get_public_key(), 2);
            op.memo_key = priv_key.get_public_key();
            op.json_metadata = "{\"foo\":\"bar\"}";

            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            BOOST_TEST_MESSAGE("--- Test failure when no signatures");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test success with witness signature");
            tx.sign(init_account_priv_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate signatures");
            tx.operations.clear();
            tx.signatures.clear();
            op.new_account_name = "sam";
            tx.operations.push_back(op);
            tx.sign(init_account_priv_key, db->get_chain_id());
            tx.sign(init_account_priv_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by an additional signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(init_account_priv_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by a signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_create_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_create_apply");

            signed_transaction tx;
            private_key_type priv_key = generate_private_key("alice");

            const account_object &init = db->get_account(STEEMIT_INIT_MINER_NAME);
            asset init_starting_balance = init.balance;

            const auto &gpo = db->get_dynamic_global_properties();

            account_create_operation op;

            op.fee = asset(100, STEEM_SYMBOL);
            op.new_account_name = "alice";
            op.creator = STEEMIT_INIT_MINER_NAME;
            op.owner = authority(1, priv_key.get_public_key(), 1);
            op.active = authority(2, priv_key.get_public_key(), 2);
            op.memo_key = priv_key.get_public_key();
            op.json_metadata = "{\"foo\":\"bar\"}";

            BOOST_TEST_MESSAGE("--- Test normal account creation");
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            tx.sign(init_account_priv_key, db->get_chain_id());
            tx.validate();
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const account_object &acct = db->get_account("alice");
            const account_authority_object &acct_auth = db->get<account_authority_object, by_account>("alice");

            auto vest_shares = gpo.total_vesting_shares;
            auto vests = gpo.total_vesting_fund_steem;

            BOOST_CHECK_EQUAL(acct.name, "alice");
            BOOST_CHECK_EQUAL(acct_auth.owner, authority(1, priv_key.get_public_key(), 1));
            BOOST_CHECK_EQUAL(acct_auth.active, authority(2, priv_key.get_public_key(), 2));
            BOOST_CHECK_EQUAL(acct.memo_key, priv_key.get_public_key());
            BOOST_CHECK_EQUAL(acct.proxy, "");
            BOOST_CHECK_EQUAL(acct.created, db->head_block_time());
            BOOST_CHECK_EQUAL(acct.balance.amount.value, ASSET("0.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(acct.sbd_balance.amount.value, ASSET("0.000 GBG").amount.value);
            BOOST_CHECK_EQUAL(acct.id._id, acct_auth.id._id);

            auto meta = db->find<account_metadata_object, by_account>(acct.name);
            BOOST_CHECK(nullptr != meta);
            if (nullptr != meta) {
                BOOST_CHECK_EQUAL(to_string(meta->json_metadata), op.json_metadata);
            }

            /// because init_witness has created vesting shares and blocks have been produced, 100 STEEM is worth less than 100 vesting shares due to rounding
            BOOST_CHECK_EQUAL(acct.vesting_shares.amount.value, (op.fee * (vest_shares / vests)).amount.value);
            BOOST_CHECK_EQUAL(acct.vesting_withdraw_rate.amount.value, ASSET("0.000000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(acct.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL((init_starting_balance - ASSET("0.100 GOLOS")).amount.value, init.balance.amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure of duplicate account creation");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(object_already_exist, "account", "alice")));

            BOOST_CHECK_EQUAL(acct.name, "alice");
            BOOST_CHECK_EQUAL(acct_auth.owner, authority(1, priv_key.get_public_key(), 1));
            BOOST_CHECK_EQUAL(acct_auth.active, authority(2, priv_key.get_public_key(), 2));
            BOOST_CHECK_EQUAL(acct.memo_key, priv_key.get_public_key());
            BOOST_CHECK_EQUAL(acct.proxy, "");
            BOOST_CHECK_EQUAL(acct.created, db->head_block_time());
            BOOST_CHECK_EQUAL(acct.balance.amount.value, ASSET("0.000 GOLOS ").amount.value);
            BOOST_CHECK_EQUAL(acct.sbd_balance.amount.value, ASSET("0.000 GBG").amount.value);
            BOOST_CHECK_EQUAL(acct.vesting_shares.amount.value, (op.fee * (vest_shares / vests)).amount.value);
            BOOST_CHECK_EQUAL(acct.vesting_withdraw_rate.amount.value, ASSET("0.000000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(acct.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL((init_starting_balance - ASSET("0.100 GOLOS")).amount.value, init.balance.amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when creator cannot cover fee");
            tx.signatures.clear();
            tx.operations.clear();
            op.fee = asset(db->get_account(STEEMIT_INIT_MINER_NAME).balance.amount + 1, STEEM_SYMBOL);
            op.new_account_name = "bob";
            tx.operations.push_back(op);
            tx.sign(init_account_priv_key, db->get_chain_id());

            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, STEEMIT_INIT_MINER_NAME, "fund", op.fee.to_string())));
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_update_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_update_validate");

            ACTORS((alice))

            account_update_operation op;
            op.account = "alice";
            op.posting = authority();
            op.posting->weight_threshold = 1;
            op.posting->add_authorities("abcdefghijklmnopq", 1);
            BOOST_CHECK_NO_THROW(op.validate());

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "account", "abcdefghijklmnop"))); // droped 17-th symbol 'q'

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_update_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_update_authorities");

            ACTORS((alice)(bob))
            private_key_type active_key = generate_private_key("new_key");

            db->modify(db->get<account_authority_object, by_account>("alice"), [&](account_authority_object &a) {
                a.active = authority(1, active_key.get_public_key(), 1);
            });

            account_update_operation op;
            op.account = "alice";
            op.json_metadata = "{\"success\":true}";

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            BOOST_TEST_MESSAGE("  GOLOS when owner authority is not updated ---");
            BOOST_TEST_MESSAGE("--- Test failure when no signature");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when wrong signature");
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when containing additional incorrect signature");
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when containing duplicate signatures");
            tx.signatures.clear();
            tx.sign(active_key, db->get_chain_id());
            tx.sign(active_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success on active key");
            tx.signatures.clear();
            tx.sign(active_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Test success on owner key alone");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, database::skip_transaction_dupe_check));

            BOOST_TEST_MESSAGE("  GOLOS when owner authority is updated ---");
            BOOST_TEST_MESSAGE("--- Test failure when updating the owner authority with an active key");
            tx.signatures.clear();
            tx.operations.clear();
            op.owner = authority(1, active_key.get_public_key(), 1);
            tx.operations.push_back(op);
            tx.sign(active_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_owner_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when owner key and active key are present");
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_owner_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate owner keys are present");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success when updating the owner authority with an owner key");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_update_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_update_apply");

            ACTORS((alice))
            private_key_type new_private_key = generate_private_key("new_key");

            BOOST_TEST_MESSAGE("--- Test normal update");

            account_update_operation op;
            op.account = "alice";
            op.owner = authority(1, new_private_key.get_public_key(), 1);
            op.active = authority(2, new_private_key.get_public_key(), 2);
            op.memo_key = new_private_key.get_public_key();
            op.json_metadata = "{\"bar\":\"foo\"}";

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const account_object &acct = db->get_account("alice");
            const account_authority_object &acct_auth = db->get<account_authority_object, by_account>("alice");

            BOOST_CHECK_EQUAL(acct.name, "alice");
            BOOST_CHECK_EQUAL(acct_auth.owner, authority(1, new_private_key.get_public_key(), 1));
            BOOST_CHECK_EQUAL(acct_auth.active, authority(2, new_private_key.get_public_key(), 2));
            BOOST_CHECK_EQUAL(acct.memo_key, new_private_key.get_public_key());

            auto meta = db->find<account_metadata_object, by_account>(acct.name);
            BOOST_CHECK(nullptr != meta);
            if (nullptr != meta) {
                BOOST_CHECK_EQUAL(to_string(meta->json_metadata), op.json_metadata);
            }

            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when updating a non-existent account");
            tx.operations.clear();
            tx.signatures.clear();
            op.account = "bob";
            tx.operations.push_back(op);
            tx.sign(new_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(missing_object, "authority", "bob"));
            validate_database();


            BOOST_TEST_MESSAGE("--- Test failure when account authority does not exist");
            tx.clear();
            op = account_update_operation();
            op.account = "alice";
            op.posting = authority();
            op.posting->weight_threshold = 1;
            op.posting->add_authorities("dave", 1);
            tx.operations.push_back(op);
            tx.sign(new_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "account", "dave")));
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(comment_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: comment_validate");

            comment_operation op;
            op.author = "alice";
            op.permlink = "lorem";
            op.parent_author = "";
            op.parent_permlink = "ipsum";
            op.title = "Lorem Ipsum";
            op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
            op.json_metadata = "{\"foo\":\"bar\"}";

            BOOST_TEST_MESSAGE("--- success on valid operation");
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- failed when 'title' too large");
            op.title = std::string(256, ' ');
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "title"));

            BOOST_TEST_MESSAGE("--- failed when 'body' is empty");
            op.title = "Lorem Ipsum";
            op.body = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "body"));

            BOOST_TEST_MESSAGE("--- failed when 'author' is empty");
            op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
            op.author = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "author"));

            BOOST_TEST_MESSAGE("--- failed when 'permlink' too large");
            op.author = "alice";
            op.permlink = std::string(STEEMIT_MAX_PERMLINK_LENGTH, ' ');
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "permlink"));

            BOOST_TEST_MESSAGE("--- failed when 'parent_permlink' too large");
            op.permlink = "lorem";
            op.parent_permlink = std::string(STEEMIT_MAX_PERMLINK_LENGTH, ' ');
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "parent_permlink"));

            BOOST_TEST_MESSAGE("--- failed when 'json_metadata' is not valid json");
            op.parent_permlink = "ipsum";
            op.json_metadata = "{1:\"string\"}";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "json_metadata"));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(comment_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: comment_authorities");

            ACTORS((alice)(bob));
            generate_blocks(60 / STEEMIT_BLOCK_INTERVAL);

            comment_operation op;
            op.author = "alice";
            op.permlink = "lorem";
            op.parent_author = "";
            op.parent_permlink = "ipsum";
            op.title = "Lorem Ipsum";
            op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
            op.json_metadata = "{\"foo\":\"bar\"}";

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            BOOST_TEST_MESSAGE("--- Test failure when no signatures");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_posting_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate signatures");
            tx.sign(alice_post_key, db->get_chain_id());
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success with post signature");
            tx.signatures.clear();
            tx.sign(alice_post_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by an additional signature not in the creator's authority");
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by a signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_posting_auth, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(comment_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: comment_apply");

            ACTORS((alice)(bob)(sam))
            generate_blocks(60 / STEEMIT_BLOCK_INTERVAL);

            BOOST_CHECK(db->has_index<golos::plugins::social_network::comment_last_update_index>());

            const auto& clu_idx = db->get_index<golos::plugins::social_network::comment_last_update_index>().indices().get<golos::plugins::social_network::by_comment>();

            comment_operation op;
            op.author = "alice";
            op.permlink = "lorem";
            op.parent_author = "";
            op.parent_permlink = "ipsum";
            op.title = "Lorem Ipsum";
            op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
            op.json_metadata = "{\"foo\":\"bar\"}";

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            BOOST_TEST_MESSAGE("--- Test Alice posting a root comment");
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const comment_object &alice_comment = db->get_comment("alice", string("lorem"));
            const comment_content_object& alice_content = sn_plugin->get_comment_content(alice_comment.id);

            BOOST_CHECK_EQUAL(alice_comment.author, op.author);
            BOOST_CHECK_EQUAL(to_string(alice_comment.permlink), op.permlink);
            BOOST_CHECK_EQUAL(to_string(alice_comment.parent_permlink), op.parent_permlink);

            auto alice_clu_itr = clu_idx.find(alice_comment.id);
            BOOST_CHECK(alice_clu_itr != clu_idx.end());
            BOOST_CHECK_EQUAL(alice_clu_itr->last_update, db->head_block_time());

            BOOST_CHECK_EQUAL(alice_comment.created, db->head_block_time());
            BOOST_CHECK_EQUAL(alice_comment.net_rshares.value, 0);
            BOOST_CHECK_EQUAL(alice_comment.abs_rshares.value, 0);
            BOOST_CHECK_EQUAL(alice_comment.cashout_time,
                          fc::time_point_sec(db->head_block_time() + fc::seconds(STEEMIT_CASHOUT_WINDOW_SECONDS)));

            BOOST_CHECK_EQUAL( to_string( alice_content.title ), op.title );
            BOOST_CHECK_EQUAL( to_string( alice_content.body ), op.body );
            //BOOST_CHECK_EQUAL( alice_content.json_metadata, op.json_metadata );

            validate_database();

            BOOST_TEST_MESSAGE("--- Test Bob posting a comment on a non-existent comment");
            op.author = "bob";
            op.permlink = "ipsum";
            op.parent_author = "alice";
            op.parent_permlink = "foobar";

            tx.signatures.clear();
            tx.operations.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "comment", make_comment_id("alice", "foobar"))));

            BOOST_TEST_MESSAGE("--- Test Bob posting a comment on Alice's comment");
            op.parent_permlink = "lorem";

            tx.signatures.clear();
            tx.operations.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const comment_object &bob_comment = db->get_comment("bob", string("ipsum"));

            BOOST_CHECK_EQUAL(bob_comment.author, op.author);
            BOOST_CHECK_EQUAL(to_string(bob_comment.permlink), op.permlink);
            BOOST_CHECK_EQUAL(bob_comment.parent_author, op.parent_author);
            BOOST_CHECK_EQUAL(to_string(bob_comment.parent_permlink), op.parent_permlink);

            auto bob_clu_itr = clu_idx.find(bob_comment.id);
            BOOST_CHECK(bob_clu_itr != clu_idx.end());
            BOOST_CHECK_EQUAL(bob_clu_itr->last_update, db->head_block_time());

            BOOST_CHECK_EQUAL(bob_comment.created, db->head_block_time());
            BOOST_CHECK_EQUAL(bob_comment.net_rshares.value, 0);
            BOOST_CHECK_EQUAL(bob_comment.abs_rshares.value, 0);
            BOOST_CHECK_EQUAL(bob_comment.cashout_time, bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
            BOOST_CHECK_EQUAL(bob_comment.root_comment, alice_comment.id);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test Sam posting a comment on Bob's comment");

            op.author = "sam";
            op.permlink = "dolor";
            op.parent_author = "bob";
            op.parent_permlink = "ipsum";

            tx.signatures.clear();
            tx.operations.clear();
            tx.operations.push_back(op);
            tx.sign(sam_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const comment_object &sam_comment = db->get_comment("sam", string("dolor"));

            BOOST_CHECK_EQUAL(sam_comment.author, op.author);
            BOOST_CHECK_EQUAL(to_string(sam_comment.permlink), op.permlink);
            BOOST_CHECK_EQUAL(sam_comment.parent_author, op.parent_author);
            BOOST_CHECK_EQUAL(to_string(sam_comment.parent_permlink), op.parent_permlink);

            auto sam_clu_itr = clu_idx.find(sam_comment.id);
            BOOST_CHECK(sam_clu_itr != clu_idx.end());
            BOOST_CHECK_EQUAL(sam_clu_itr->last_update, db->head_block_time());

            BOOST_CHECK_EQUAL(sam_comment.created, db->head_block_time());
            BOOST_CHECK_EQUAL(sam_comment.net_rshares.value, 0);
            BOOST_CHECK_EQUAL(sam_comment.abs_rshares.value, 0);
            BOOST_CHECK_EQUAL(sam_comment.cashout_time, sam_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
            BOOST_CHECK_EQUAL(sam_comment.root_comment, alice_comment.id);
            validate_database();

            generate_blocks(60 * 5 / STEEMIT_BLOCK_INTERVAL + 1);

            BOOST_TEST_MESSAGE("--- Test modifying a comment");
            const auto &mod_sam_comment = db->get_comment("sam", string("dolor"));
            const auto &mod_bob_comment = db->get_comment("bob", string("ipsum"));
            const auto &mod_alice_comment = db->get_comment("alice", string("lorem"));
            fc::time_point_sec created = mod_sam_comment.created;

            db->modify(mod_sam_comment, [&](comment_object &com) {
                com.net_rshares = 10;
                com.abs_rshares = 10;
                com.children_rshares2 = db->calculate_vshares(10);
            });

            db->modify(mod_bob_comment, [&](comment_object &com) {
                com.children_rshares2 = db->calculate_vshares(10);
            });

            db->modify(mod_alice_comment, [&](comment_object &com) {
                com.children_rshares2 = db->calculate_vshares(10);
            });

            db->modify(db->get_dynamic_global_properties(), [&](dynamic_global_property_object &o) {
                o.total_reward_shares2 = db->calculate_vshares(10);
            });

            tx.signatures.clear();
            tx.operations.clear();
            op.title = "foo";
            op.body = "bar";
            op.json_metadata = "{\"bar\":\"foo\"}";
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(sam_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(mod_sam_comment.author, op.author);
            BOOST_CHECK_EQUAL(to_string(mod_sam_comment.permlink), op.permlink);
            BOOST_CHECK_EQUAL(mod_sam_comment.parent_author, op.parent_author);
            BOOST_CHECK_EQUAL(to_string(mod_sam_comment.parent_permlink), op.parent_permlink);

            auto mod_sam_clu_itr = clu_idx.find(mod_sam_comment.id);
            BOOST_CHECK(mod_sam_clu_itr != clu_idx.end());
            BOOST_CHECK_EQUAL(mod_sam_clu_itr->last_update, db->head_block_time());

            BOOST_CHECK_EQUAL(mod_sam_comment.created, created);
            BOOST_CHECK_EQUAL(mod_sam_comment.cashout_time, mod_sam_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure posting withing 1 minute");

            op.permlink = "sit";
            op.parent_author = "";
            op.parent_permlink = "test";
            tx.operations.clear();
            tx.signatures.clear();
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            tx.sign(sam_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            generate_blocks(STEEMIT_POSTS_WINDOW / STEEMIT_BLOCK_INTERVAL - 1);

            op.permlink = "amet";
            tx.operations.clear();
            tx.signatures.clear();
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            tx.sign(sam_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(bandwidth_exception, golos::bandwidth_exception::post_bandwidth)));

            validate_database();

            generate_block();
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(vote_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: vote_validate");

            vote_operation op;
            op.voter = "bob";
            op.author = "alice";
            op.permlink = "test";
            op.weight = 1000;

            BOOST_TEST_MESSAGE("failure when 'voter' is empty");
            op.voter = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(golos::invalid_parameter, "voter"));

            BOOST_TEST_MESSAGE("failure when 'author' is empty");
            op.voter = "bob";
            op.author = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(golos::invalid_parameter, "author"));

            BOOST_TEST_MESSAGE("failure when 'weight' is too small");
            op.author = "alice";
            op.weight = -STEEMIT_100_PERCENT-1;
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(golos::invalid_parameter, "weight"));

            BOOST_TEST_MESSAGE("failure when 'weight' is too mush");
            op.weight = STEEMIT_100_PERCENT+1;
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(golos::invalid_parameter, "weight"));

            BOOST_TEST_MESSAGE("success with positive 'weight'");
            op.weight = STEEMIT_100_PERCENT;
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("success with negative 'weight'");
            op.weight = -STEEMIT_100_PERCENT;
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("failure when 'perlink' invalid");
            op.weight = 1000;
            op.permlink = std::string(STEEMIT_MAX_PERMLINK_LENGTH, ' ');
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(golos::invalid_parameter, "permlink"));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(vote_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: vote_authorities");

            vote_operation op;
            op.voter = "bob";
            op.author = "alice";
            op.permlink = "test";
            op.weight = 1000;

            CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(vote_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: vote_apply");

            ACTORS((alice)(bob)(sam)(dave))
            generate_block();

            vest("alice", ASSET("10.000 GOLOS"));
            validate_database();
            vest("bob", ASSET("10.000 GOLOS"));
            vest("sam", ASSET("10.000 GOLOS"));
            vest("dave", ASSET("10.000 GOLOS"));
            generate_block();

            const auto &vote_idx = db->get_index<comment_vote_index>().indices().get<by_comment_voter>();

            {
                const auto &alice = db->get_account("alice");

                signed_transaction tx;
                comment_operation comment_op;
                comment_op.author = "alice";
                comment_op.permlink = "foo";
                comment_op.parent_permlink = "test";
                comment_op.title = "bar";
                comment_op.body = "foo bar";
                tx.operations.push_back(comment_op);
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                BOOST_TEST_MESSAGE("--- Testing voting on a non-existent comment");

                tx.operations.clear();
                tx.signatures.clear();

                vote_operation op;
                op.voter = "alice";
                op.author = "bob";
                op.permlink = "foo";
                op.weight = STEEMIT_100_PERCENT;
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());

                GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                    CHECK_ERROR(tx_invalid_operation, 0,
                        CHECK_ERROR(missing_object, "comment", make_comment_id("bob","foo"))));

                validate_database();

                BOOST_TEST_MESSAGE("--- Testing voting with a weight of 0");

                op.author = "alice";
                op.weight = (int16_t)0;
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());

                GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                    CHECK_ERROR(tx_invalid_operation, 0,
                        CHECK_ERROR(invalid_parameter, "weight")));

                validate_database();

                BOOST_TEST_MESSAGE("--- Testing success");

                auto old_voting_power = alice.voting_power;

                op.weight = STEEMIT_100_PERCENT;
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());

                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                auto &alice_comment = db->get_comment("alice", string("foo"));
                auto itr = vote_idx.find(std::make_tuple(alice_comment.id, alice.id));
                int64_t max_vote_denom =
                        (db->get_dynamic_global_properties().vote_regeneration_per_day *
                         STEEMIT_VOTE_REGENERATION_SECONDS) / (60 * 60 * 24);

                BOOST_CHECK_EQUAL(alice.voting_power, old_voting_power -
                                                      ((old_voting_power + max_vote_denom - 1) /
                                                      max_vote_denom));
                BOOST_CHECK_EQUAL(alice.last_vote_time, db->head_block_time());
                BOOST_CHECK_EQUAL(alice_comment.net_rshares.value, alice.vesting_shares.amount.value *
                                                                   (old_voting_power - alice.voting_power) /
                                                                   STEEMIT_100_PERCENT);
                BOOST_CHECK_EQUAL(alice_comment.cashout_time, alice_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
                BOOST_CHECK_EQUAL(itr->rshares, alice.vesting_shares.amount.value *
                                                (old_voting_power - alice.voting_power) /
                                                STEEMIT_100_PERCENT);
                BOOST_CHECK(itr != vote_idx.end());
                validate_database();

                BOOST_TEST_MESSAGE("--- Test reduced power for quick voting");

                generate_blocks(db->head_block_time() + STEEMIT_MIN_VOTE_INTERVAL_SEC);

                old_voting_power = db->get_account("alice").voting_power;

                comment_op.author = "bob";
                comment_op.permlink = "foo";
                comment_op.title = "bar";
                comment_op.body = "foo bar";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(comment_op);
                tx.sign(bob_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                op.weight = STEEMIT_100_PERCENT / 2;
                op.voter = "alice";
                op.author = "bob";
                op.permlink = "foo";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                const auto &bob_comment = db->get_comment("bob", string("foo"));
                itr = vote_idx.find(std::make_tuple(bob_comment.id, alice.id));

                BOOST_CHECK_EQUAL(db->get_account("alice").voting_power,
                              old_voting_power -
                              ((old_voting_power + max_vote_denom - 1) *
                               STEEMIT_100_PERCENT /
                               (2 * max_vote_denom * STEEMIT_100_PERCENT)));
                BOOST_CHECK_EQUAL(bob_comment.net_rshares.value,
                              alice.vesting_shares.amount.value *
                              (old_voting_power -
                               db->get_account("alice").voting_power) /
                              STEEMIT_100_PERCENT);
                BOOST_CHECK_EQUAL(bob_comment.cashout_time, bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
                BOOST_CHECK(itr != vote_idx.end());
                validate_database();

                BOOST_TEST_MESSAGE("--- Test payout time extension on vote");

                old_voting_power = db->get_account("bob").voting_power;
                auto old_abs_rshares = db->get_comment("alice", string("foo")).abs_rshares.value;

                generate_blocks(db->head_block_time() + fc::seconds((STEEMIT_CASHOUT_WINDOW_SECONDS / 2)), true);

                const auto &new_bob = db->get_account("bob");
                const auto &new_alice_comment = db->get_comment("alice", string("foo"));

                op.weight = STEEMIT_100_PERCENT;
                op.voter = "bob";
                op.author = "alice";
                op.permlink = "foo";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx.sign(bob_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                itr = vote_idx.find(std::make_tuple(new_alice_comment.id, new_bob.id));

                BOOST_CHECK_EQUAL(new_bob.voting_power, STEEMIT_100_PERCENT -
                                                      ((STEEMIT_100_PERCENT +
                                                        max_vote_denom - 1) /
                                                       max_vote_denom));
                BOOST_CHECK_EQUAL(new_alice_comment.net_rshares.value,
                                  old_abs_rshares +
                                  new_bob.vesting_shares.amount.value *
                                  (old_voting_power - new_bob.voting_power) /
                                  STEEMIT_100_PERCENT);
                BOOST_CHECK_EQUAL(new_alice_comment.cashout_time, new_alice_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
                BOOST_CHECK(itr != vote_idx.end());
                validate_database();

                BOOST_TEST_MESSAGE("--- Test negative vote");

                const auto &new_sam = db->get_account("sam");
                const auto &new_bob_comment = db->get_comment("bob", string("foo"));

                old_abs_rshares = new_bob_comment.abs_rshares.value;

                op.weight = -1 * STEEMIT_100_PERCENT / 2;
                op.voter = "sam";
                op.author = "bob";
                op.permlink = "foo";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(sam_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                itr = vote_idx.find(std::make_tuple(new_bob_comment.id, new_sam.id));
                auto sam_weight /*= ( ( uint128_t( new_sam.vesting_shares.amount.value ) ) / 400 + 1 ).to_uint64();*/
                        = ((uint128_t(new_sam.vesting_shares.amount.value) *
                            ((STEEMIT_100_PERCENT + max_vote_denom - 1) /
                             (2 * max_vote_denom))) /
                           STEEMIT_100_PERCENT).to_uint64();

                BOOST_CHECK_EQUAL(new_sam.voting_power, STEEMIT_100_PERCENT -
                                                      ((STEEMIT_100_PERCENT +
                                                        max_vote_denom - 1) /
                                                       (2 * max_vote_denom)));
                BOOST_CHECK_EQUAL(new_bob_comment.net_rshares.value, (int64_t)(old_abs_rshares - sam_weight));
                BOOST_CHECK_EQUAL(new_bob_comment.abs_rshares.value, (int64_t)(old_abs_rshares + sam_weight));
                BOOST_CHECK_EQUAL(new_bob_comment.cashout_time, new_bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
                BOOST_CHECK(itr != vote_idx.end());
                validate_database();

                BOOST_TEST_MESSAGE("--- Test nested voting on nested comments");

                old_abs_rshares = new_alice_comment.children_abs_rshares.value;
                int64_t regenerated_power =
                        (STEEMIT_100_PERCENT *
                            (db->head_block_time() - db->get_account("alice").last_vote_time).to_seconds()) /
                        STEEMIT_VOTE_REGENERATION_SECONDS;
                int64_t used_power =
                        (db->get_account("alice").voting_power + regenerated_power + max_vote_denom - 1) /
                        max_vote_denom;

                comment_op.author = "sam";
                comment_op.permlink = "foo";
                comment_op.title = "bar";
                comment_op.body = "foo bar";
                comment_op.parent_author = "alice";
                comment_op.parent_permlink = "foo";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(comment_op);
                tx.sign(sam_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                auto old_rshares2 = db->get_comment("alice", string("foo")).children_rshares2;

                op.weight = STEEMIT_100_PERCENT;
                op.voter = "alice";
                op.author = "sam";
                op.permlink = "foo";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());
                db->push_transaction(tx, 0);

                auto new_rshares = (
                        (fc::uint128_t(db->get_account("alice").vesting_shares.amount.value) *
                         used_power) / STEEMIT_100_PERCENT).to_uint64();

                BOOST_CHECK_EQUAL(
                        db->get_comment("alice", string("foo")).children_rshares2,
                        db->get_comment("sam", string("foo")).children_rshares2 + old_rshares2);
                BOOST_CHECK_EQUAL(
                        db->get_comment("alice", string( "foo" )).cashout_time,
                        db->get_comment("alice", string( "foo" )).created + STEEMIT_CASHOUT_WINDOW_SECONDS);

                validate_database();

                BOOST_TEST_MESSAGE("--- Test increasing vote rshares");

                generate_blocks(db->head_block_time() + STEEMIT_MIN_VOTE_INTERVAL_SEC);

                auto new_alice = db->get_account("alice");
                auto alice_bob_vote = vote_idx.find(std::make_tuple(new_bob_comment.id, new_alice.id));
                auto old_vote_rshares = alice_bob_vote->rshares;
                auto old_net_rshares = new_bob_comment.net_rshares.value;
                old_abs_rshares = new_bob_comment.abs_rshares.value;
                used_power =
                        ((STEEMIT_1_PERCENT * 25 * (new_alice.voting_power) /
                          STEEMIT_100_PERCENT) + max_vote_denom - 1) /
                        max_vote_denom;
                auto alice_voting_power = new_alice.voting_power - used_power;

                op.voter = "alice";
                op.weight = STEEMIT_1_PERCENT * 25;
                op.author = "bob";
                op.permlink = "foo";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
                alice_bob_vote = vote_idx.find(std::make_tuple(new_bob_comment.id, new_alice.id));

                new_rshares = (
                        (fc::uint128_t(new_alice.vesting_shares.amount.value) *
                         used_power) / STEEMIT_100_PERCENT).to_uint64();

                BOOST_CHECK_EQUAL(new_bob_comment.net_rshares, old_net_rshares - old_vote_rshares + new_rshares);
                BOOST_CHECK_EQUAL(new_bob_comment.abs_rshares, old_abs_rshares + new_rshares);
                BOOST_CHECK_EQUAL(new_bob_comment.cashout_time, new_bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
                BOOST_CHECK_EQUAL(alice_bob_vote->rshares, (int64_t)new_rshares);
                BOOST_CHECK_EQUAL(alice_bob_vote->last_update, db->head_block_time());
                BOOST_CHECK_EQUAL(alice_bob_vote->vote_percent, op.weight);
                BOOST_CHECK_EQUAL(db->get_account("alice").voting_power, alice_voting_power);
                validate_database();

                BOOST_TEST_MESSAGE("--- Test decreasing vote rshares");

                generate_blocks(db->head_block_time() + STEEMIT_MIN_VOTE_INTERVAL_SEC);

                old_vote_rshares = new_rshares;
                old_net_rshares = new_bob_comment.net_rshares.value;
                old_abs_rshares = new_bob_comment.abs_rshares.value;
                used_power = (uint64_t(STEEMIT_1_PERCENT) * 75 *
                              uint64_t(alice_voting_power)) /
                             STEEMIT_100_PERCENT;
                used_power = (used_power + max_vote_denom - 1) / max_vote_denom;
                alice_voting_power -= used_power;

                op.weight = STEEMIT_1_PERCENT * -75;
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
                alice_bob_vote = vote_idx.find(std::make_tuple(new_bob_comment.id, new_alice.id));

                new_rshares = (
                        (fc::uint128_t(new_alice.vesting_shares.amount.value) *
                         used_power) / STEEMIT_100_PERCENT).to_uint64();

                BOOST_CHECK_EQUAL(new_bob_comment.net_rshares, old_net_rshares - old_vote_rshares - new_rshares);
                BOOST_CHECK_EQUAL(new_bob_comment.abs_rshares, old_abs_rshares + new_rshares);
                BOOST_CHECK_EQUAL(new_bob_comment.cashout_time, new_bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
                BOOST_CHECK_EQUAL(alice_bob_vote->rshares, -1 * (int64_t)new_rshares);
                BOOST_CHECK_EQUAL(alice_bob_vote->last_update, db->head_block_time());
                BOOST_CHECK_EQUAL(alice_bob_vote->vote_percent, op.weight);
                BOOST_CHECK_EQUAL(db->get_account("alice").voting_power, alice_voting_power);
                validate_database();

                BOOST_TEST_MESSAGE("--- Test changing a vote to 0 weight (aka: removing a vote)");

                generate_blocks(db->head_block_time() + STEEMIT_MIN_VOTE_INTERVAL_SEC);

                old_vote_rshares = alice_bob_vote->rshares;
                old_net_rshares = new_bob_comment.net_rshares.value;
                old_abs_rshares = new_bob_comment.abs_rshares.value;

                op.weight = 0;
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
                alice_bob_vote = vote_idx.find(std::make_tuple(new_bob_comment.id, new_alice.id));

                BOOST_CHECK_EQUAL(new_bob_comment.net_rshares, old_net_rshares - old_vote_rshares);
                BOOST_CHECK_EQUAL(new_bob_comment.abs_rshares, old_abs_rshares);
                BOOST_CHECK_EQUAL(new_bob_comment.cashout_time, new_bob_comment.created + STEEMIT_CASHOUT_WINDOW_SECONDS);
                BOOST_CHECK_EQUAL(alice_bob_vote->rshares, 0);
                BOOST_CHECK_EQUAL(alice_bob_vote->last_update, db->head_block_time());
                BOOST_CHECK_EQUAL(alice_bob_vote->vote_percent, op.weight);
                BOOST_CHECK_EQUAL(db->get_account("alice").voting_power, alice_voting_power);
                validate_database();

                BOOST_TEST_MESSAGE("--- Test failure when increasing rshares within lockout period");

                generate_blocks(fc::time_point_sec(
                        (new_bob_comment.cashout_time - STEEMIT_UPVOTE_LOCKOUT).sec_since_epoch() +
                        STEEMIT_BLOCK_INTERVAL), true);

                op.weight = STEEMIT_100_PERCENT;
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());

                GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                    CHECK_ERROR(tx_invalid_operation, 0,
                        CHECK_ERROR(logic_exception, logic_exception::cannot_vote_within_last_minute_before_payout)));
                validate_database();

                BOOST_TEST_MESSAGE("--- Test success when reducing rshares within lockout period");

                op.weight = -1 * STEEMIT_100_PERCENT;
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
                validate_database();

                BOOST_TEST_MESSAGE("--- Test failure with a new vote within lockout period");

                op.weight = STEEMIT_100_PERCENT;
                op.voter = "sam";
                tx.operations.clear();
                tx.signatures.clear();
                tx.operations.push_back(op);
                tx.sign(sam_private_key, db->get_chain_id());
                GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                    CHECK_ERROR(tx_invalid_operation, 0,
                        CHECK_ERROR(logic_exception, logic_exception::cannot_vote_within_last_minute_before_payout)));
                validate_database();
            }
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_validate");

            transfer_operation op;
            op.from = "alice";
            op.to = "bob";
            op.amount = ASSET("10.000 GOLOS");
            op.memo = "memo string";

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- failure when 'from' is empty");
            op.from = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "from"));

            BOOST_TEST_MESSAGE("--- failure when 'to' is empty");
            op.from = "alice";
            op.to = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "to"));

            BOOST_TEST_MESSAGE("--- failure when 'amount' is negative");
            op.to = "bob";
            op.amount = ASSET("-10.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "amount"));

            BOOST_TEST_MESSAGE("--- failure when 'amount' in GESTS");
            op.amount = ASSET("10.000000 GESTS");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "amount"));

            BOOST_TEST_MESSAGE("--- failure when 'memo' too large");
            op.amount = ASSET("10.000 GOLOS");
            op.memo = std::string(STEEMIT_MAX_MEMO_SIZE, ' ');
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "memo"));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_authorities) {
        try {
            ACTORS((alice)(bob))
            fund("alice", 10000);

            BOOST_TEST_MESSAGE("Testing: transfer_authorities");

            transfer_operation op;
            op.from = "alice";
            op.to = "bob";
            op.amount = ASSET("2.500 GOLOS");

            signed_transaction tx;
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            BOOST_TEST_MESSAGE("--- Test failure when no signatures");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by a signature not in the account's authority");
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate signatures");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by an additional signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success with witness signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(signature_stripping) {
        try {
            // Alice, Bob and Sam all have 2-of-3 multisig on corp.
            // Legitimate tx signed by (Alice, Bob) goes through.
            // Sam shouldn't be able to add or remove signatures to get the transaction to process multiple times.

            ACTORS((alice)(bob)(sam)(corp))
            fund("corp", 10000);

            account_update_operation update_op;
            update_op.account = "corp";
            update_op.active = authority(2, "alice", 1, "bob", 1, "sam", 1);

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(update_op);

            tx.sign(corp_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            tx.operations.clear();
            tx.signatures.clear();

            transfer_operation transfer_op;
            transfer_op.from = "corp";
            transfer_op.to = "sam";
            transfer_op.amount = ASSET("1.000 GOLOS");

            tx.operations.push_back(transfer_op);

            tx.sign(alice_private_key, db->get_chain_id());
            signature_type alice_sig = tx.signatures.back();
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            tx.sign(bob_private_key, db->get_chain_id());
            signature_type bob_sig = tx.signatures.back();
            tx.sign(sam_private_key, db->get_chain_id());
            signature_type sam_sig = tx.signatures.back();
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            tx.signatures.clear();
            tx.signatures.push_back(alice_sig);
            tx.signatures.push_back(bob_sig);
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            tx.signatures.clear();
            tx.signatures.push_back(alice_sig);
            tx.signatures.push_back(sam_sig);
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_transaction, 0));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_apply");

            ACTORS((alice)(bob))
            fund("alice", 10000);

            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("10.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value, ASSET(" 0.000 GOLOS").amount.value);

            signed_transaction tx;
            transfer_operation op;

            op.from = "alice";
            op.to = "bob";
            op.amount = ASSET("5.000 GOLOS");

            BOOST_TEST_MESSAGE("--- Test normal transaction");
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(alice.balance.amount.value,  ASSET("5.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,  ASSET("5.000 GOLOS").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Generating a block");
            generate_block();

            const auto &new_alice = db->get_account("alice");
            const auto &new_bob = db->get_account("bob");

            BOOST_CHECK_EQUAL(new_alice.balance.amount.value,  ASSET("5.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_bob.balance.amount.value,  ASSET("5.000 GOLOS").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test invalid amount (less digits after dot)");
            tx.signatures.clear();
            tx.operations.clear();
            op.amount = ASSET("2.00 GOLOS");
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "amount")));

            BOOST_CHECK_EQUAL(new_alice.balance.amount.value,  ASSET("5.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_bob.balance.amount.value,  ASSET("5.000 GOLOS").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test emptying an account");
            tx.signatures.clear();
            tx.operations.clear();
            op.amount = ASSET("5.000 GOLOS");
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, database::skip_transaction_dupe_check));

            BOOST_CHECK_EQUAL(new_alice.balance.amount.value,  ASSET("0.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_bob.balance.amount.value,  ASSET("10.000 GOLOS").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test transferring non-existent funds");
            tx.signatures.clear();
            tx.operations.clear();
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "alice", "fund", "5.000 GOLOS")));

            BOOST_CHECK_EQUAL(new_alice.balance.amount.value,  ASSET("0.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_bob.balance.amount.value,  ASSET("10.000 GOLOS").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test transferring to non-existent account");
            tx.signatures.clear();
            tx.operations.clear();
            op.to = "sam";
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "account", "sam")));

        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_to_vesting_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_to_vesting_validate");

            transfer_to_vesting_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.from = "alice";
            op.to = "";
            op.amount = ASSET("5.000 GOLOS");
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- failed when 'from' is empty");
            op.from = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "from"));

            BOOST_TEST_MESSAGE("--- failed when 'amount' is negative");
            op.from = "alice";
            op.amount = ASSET("-2.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "amount"));

            BOOST_TEST_MESSAGE("--- failed when 'amount' have invalid symbol");
            op.amount = ASSET("2.000000 GESTS");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "amount"));

            BOOST_TEST_MESSAGE("--- failed when 'to' is invalid");
            op.amount = ASSET("5.000 GOLOS");
            op.to = "a";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "to"));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_to_vesting_authorities) {
        try {
            ACTORS((alice)(bob))
            fund("alice", 10000);

            BOOST_TEST_MESSAGE("Testing: transfer_to_vesting_authorities");

            transfer_to_vesting_operation op;
            op.from = "alice";
            op.to = "bob";
            op.amount = ASSET("2.500 GOLOS");

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            BOOST_TEST_MESSAGE("--- Test failure when no signatures");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by a signature not in the account's authority");
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate signatures");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by an additional signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success with from signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_to_vesting_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_to_vesting_apply");

            ACTORS((alice)(bob))
            fund("alice", 10000);

            const auto &gpo = db->get_dynamic_global_properties();

            BOOST_CHECK_EQUAL(alice.balance, ASSET("10.000 GOLOS"));

            auto shares = asset(gpo.total_vesting_shares.amount, VESTS_SYMBOL);
            auto vests = asset(gpo.total_vesting_fund_steem.amount, STEEM_SYMBOL);
            auto alice_shares = alice.vesting_shares;
            auto bob_shares = bob.vesting_shares;

            transfer_to_vesting_operation op;
            op.from = "alice";
            op.to = "";
            op.amount = ASSET("7.500 GOLOS");

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            auto new_vest = op.amount * (shares / vests);
            shares += new_vest;
            vests += op.amount;
            alice_shares += new_vest;

            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("2.500 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.vesting_shares.amount.value, alice_shares.amount.value);
            BOOST_CHECK_EQUAL(gpo.total_vesting_fund_steem.amount.value, vests.amount.value);
            BOOST_CHECK_EQUAL(gpo.total_vesting_shares.amount.value, shares.amount.value);
            validate_database();

            op.to = "bob";
            op.amount = asset(2000, STEEM_SYMBOL);
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            new_vest = asset((op.amount * (shares / vests)).amount, VESTS_SYMBOL);
            shares += new_vest;
            vests += op.amount;
            bob_shares += new_vest;

            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("0.500 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.vesting_shares.amount.value, alice_shares.amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value, ASSET("0.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.vesting_shares.amount.value, bob_shares.amount.value);
            BOOST_CHECK_EQUAL(gpo.total_vesting_fund_steem.amount.value, vests.amount.value);
            BOOST_CHECK_EQUAL(gpo.total_vesting_shares.amount.value, shares.amount.value);
            validate_database();

            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "alice", "fund", "2.000 GOLOS")));

            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("0.500 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.vesting_shares.amount.value, alice_shares.amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value, ASSET("0.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.vesting_shares.amount.value, bob_shares.amount.value);
            BOOST_CHECK_EQUAL(gpo.total_vesting_fund_steem.amount.value, vests.amount.value);
            BOOST_CHECK_EQUAL(gpo.total_vesting_shares.amount.value, shares.amount.value);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(withdraw_vesting_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: withdraw_vesting_validate");

            withdraw_vesting_operation op;

            BOOST_TEST_MESSAGE("--- success with valid parameters");
            op.account = "alice";
            op.vesting_shares = ASSET("0.001000 GESTS");
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- failed when 'account' is empty");
            op.account = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "account"));

            BOOST_TEST_MESSAGE("--- failed when 'vesting_shares' not in GESTS");
            op.account = "alice";
            op.vesting_shares = ASSET("1.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "vesting_shares"));

            BOOST_TEST_MESSAGE("--- failed when 'vesting_shares' is negative");
            op.vesting_shares = ASSET("-1.000000 GESTS");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "vesting_shares"));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(withdraw_vesting_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: withdraw_vesting_authorities");

            ACTORS((alice)(bob))
            fund("alice", 10000);
            vest("alice", 10000);

            withdraw_vesting_operation op;
            op.account = "alice";
            op.vesting_shares = ASSET("0.001000 GESTS");

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            BOOST_TEST_MESSAGE("--- Test failure when no signature.");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test success with account signature");
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, database::skip_transaction_dupe_check));

            BOOST_TEST_MESSAGE("--- Test failure with duplicate signature");
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with additional incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(withdraw_vesting_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: withdraw_vesting_apply");

            ACTORS((alice))
            generate_block();
            vest("alice", ASSET("10.000 GOLOS"));

            BOOST_TEST_MESSAGE("--- Test withdraw of existing GESTS");

            {
                const auto &alice = db->get_account("alice");

                withdraw_vesting_operation op;
                op.account = "alice";
                op.vesting_shares = asset(alice.vesting_shares.amount / 2, VESTS_SYMBOL);

                auto old_vesting_shares = alice.vesting_shares;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                BOOST_CHECK_EQUAL(alice.vesting_shares.amount.value, old_vesting_shares.amount.value);
                BOOST_CHECK_EQUAL(alice.vesting_withdraw_rate.amount.value,
                              (old_vesting_shares.amount /
                               (STEEMIT_VESTING_WITHDRAW_INTERVALS * 2)).value);
                BOOST_CHECK_EQUAL(alice.to_withdraw.value, op.vesting_shares.amount.value);
                BOOST_CHECK_EQUAL(alice.next_vesting_withdrawal, db->head_block_time() + STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS);
                validate_database();

                BOOST_TEST_MESSAGE("--- Test changing vesting withdrawal");
                tx.operations.clear();
                tx.signatures.clear();

                op.vesting_shares = asset(alice.vesting_shares.amount / 3, VESTS_SYMBOL);
                tx.operations.push_back(op);
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                BOOST_CHECK_EQUAL(alice.vesting_shares.amount.value, old_vesting_shares.amount.value);
                BOOST_CHECK_EQUAL(alice.vesting_withdraw_rate.amount.value,
                              (old_vesting_shares.amount /
                               (STEEMIT_VESTING_WITHDRAW_INTERVALS * 3)).value);
                BOOST_CHECK_EQUAL(alice.to_withdraw.value, op.vesting_shares.amount.value);
                BOOST_CHECK_EQUAL(alice.next_vesting_withdrawal,
                              db->head_block_time() +
                              STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS);
                validate_database();

                BOOST_TEST_MESSAGE("--- Test withdrawing more vests than available");
                tx.operations.clear();
                tx.signatures.clear();

                op.vesting_shares = asset(alice.vesting_shares.amount * 2, VESTS_SYMBOL);
                tx.operations.push_back(op);
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx.sign(alice_private_key, db->get_chain_id());
                GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                    CHECK_ERROR(tx_invalid_operation, 0,
                        CHECK_ERROR(insufficient_funds, "alice", "having vesting shares", asset(alice.vesting_shares.amount * 2, VESTS_SYMBOL).to_string())));

                BOOST_CHECK_EQUAL(alice.vesting_shares.amount.value, old_vesting_shares.amount.value);
                BOOST_CHECK_EQUAL(alice.vesting_withdraw_rate.amount.value,
                              (old_vesting_shares.amount /
                               (STEEMIT_VESTING_WITHDRAW_INTERVALS * 3)).value);
                BOOST_CHECK_EQUAL(alice.next_vesting_withdrawal,
                              db->head_block_time() +
                              STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS);
                validate_database();

                BOOST_TEST_MESSAGE("--- Test withdrawing 0 to reset vesting withdraw");
                tx.operations.clear();
                tx.signatures.clear();

                op.vesting_shares = asset(0, VESTS_SYMBOL);
                tx.operations.push_back(op);
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

                BOOST_CHECK_EQUAL(alice.vesting_shares.amount.value, old_vesting_shares.amount.value);
                BOOST_CHECK_EQUAL(alice.vesting_withdraw_rate.amount.value, 0);
                BOOST_CHECK_EQUAL(alice.to_withdraw.value, 0);
                BOOST_CHECK_EQUAL(alice.next_vesting_withdrawal, fc::time_point_sec::maximum());

                BOOST_TEST_MESSAGE("--- Test cancelling a withdraw when below the account creation fee");
                op.vesting_shares = alice.vesting_shares;
                tx.clear();
                tx.operations.push_back(op);
                tx.sign(alice_private_key, db->get_chain_id());
                BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
                generate_block();
            }

            db_plugin->debug_update([=](database &db) {
                auto &wso = db.get_witness_schedule_object();

                db.modify(wso, [&](witness_schedule_object &w) {
                    w.median_props.account_creation_fee = ASSET("10.000 GOLOS");
                });

                db.modify(db.get_dynamic_global_properties(), [&](dynamic_global_property_object &gpo) {
                    gpo.current_supply +=
                            wso.median_props.account_creation_fee -
                            ASSET("0.001 GOLOS") - gpo.total_vesting_fund_steem;
                    gpo.total_vesting_fund_steem =
                            wso.median_props.account_creation_fee -
                            ASSET("0.001 GOLOS");
                });

                db.update_virtual_supply();
            }, database::skip_witness_signature);

            withdraw_vesting_operation op;
            signed_transaction tx;
            op.account = "alice";
            op.vesting_shares = ASSET("0.000000 GESTS");
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(db->get_account("alice").vesting_withdraw_rate,
                          ASSET("0.000000 GESTS"));
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }


    BOOST_AUTO_TEST_CASE(witness_update_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: withness_update_validate");

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            witness_update_operation op;
            op.owner = "bob";
            op.url = "http://localhost";
            op.fee = ASSET_GOLOS(100);
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, url, "-");
            CHECK_PARAM_VALID(op, url, string(STEEMIT_MAX_WITNESS_URL_LENGTH, ' '));
            CHECK_PARAM_VALID(op, url, u8"тест");
            CHECK_PARAM_VALID(op, fee, ASSET_GOLOS(0));

            BOOST_TEST_MESSAGE("--- failure when owner is empty");
            CHECK_PARAM_INVALID(op, owner, "");

            BOOST_TEST_MESSAGE("--- failure when url is empty or too long or have invalid utf8");
            CHECK_PARAM_INVALID(op, url, "");
            // CHECK_PARAM_INVALID(op, url, string(1+STEEMIT_MAX_WITNESS_URL_LENGTH, ' '));
            CHECK_PARAM_INVALID(op, url, BAD_UTF8_STRING);

            BOOST_TEST_MESSAGE("--- failure when fee in not GOLOS or negative");
            CHECK_PARAM_INVALID(op, fee, ASSET_GBG(1));
            CHECK_PARAM_INVALID(op, fee, ASSET_GESTS(1));
            CHECK_PARAM_INVALID(op, fee, ASSET_GOLOS(-1));

            // TODO: chain_properties_17
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(witness_update_authorities) { try {
        BOOST_TEST_MESSAGE("Testing: witness_update_authorities");
        witness_update_operation op;
        op.owner = "bob";
        op.url = "http://localhost";
        // op.fee = ASSET_GOLOS(1);
        // op.block_signing_key = signing_key.get_public_key();

        CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"bob"}), account_name_set());

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(witness_update_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: witness_update_apply");

            ACTORS((alice))
            fund("alice", 10000);
            private_key_type signing_key = generate_private_key("new_key");

            BOOST_TEST_MESSAGE("--- Test upgrading an account to a witness");
            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            witness_update_operation op;
            op.owner = "alice";
            op.url = "foo.bar";
            op.fee = ASSET("1.000 GOLOS");
            op.block_signing_key = signing_key.get_public_key();
            op.props.account_creation_fee = asset(STEEMIT_MIN_ACCOUNT_CREATION_FEE + 10, STEEM_SYMBOL);
            op.props.maximum_block_size = STEEMIT_MIN_BLOCK_SIZE_LIMIT + 100;
            tx.operations.push_back(op);

            chain_properties_update_operation op1;
            op1.owner = op.owner;
            op1.props = op.props;
            tx.operations.push_back(op1);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const witness_object& w = db->get_witness("alice");
            BOOST_CHECK_EQUAL(w.owner, "alice");
            BOOST_CHECK_EQUAL(w.created, db->head_block_time());
            BOOST_CHECK_EQUAL(to_string(w.url), op.url);
            BOOST_CHECK_EQUAL(w.signing_key, op.block_signing_key);
            BOOST_CHECK_EQUAL(w.props.account_creation_fee, op.props.account_creation_fee);
            BOOST_CHECK_EQUAL(w.props.maximum_block_size, op.props.maximum_block_size);
            BOOST_CHECK_EQUAL(w.total_missed, 0u);
            BOOST_CHECK_EQUAL(w.last_aslot, 0u);
            BOOST_CHECK_EQUAL(w.last_confirmed_block_num, 0u);
            BOOST_CHECK_EQUAL(w.pow_worker, 0u);
            BOOST_CHECK_EQUAL(w.votes.value, 0u);
            BOOST_CHECK_EQUAL(w.virtual_last_update, 0);
            BOOST_CHECK_EQUAL(w.virtual_position, 0);
            BOOST_CHECK_EQUAL(w.virtual_scheduled_time, fc::uint128_t::max_value());
            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("10.000 GOLOS").amount.value); // No fee
            validate_database();

            BOOST_TEST_MESSAGE("--- Test updating a witness");
            tx.clear();
            op.url = "bar.foo";
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(w.owner, "alice");
            BOOST_CHECK_EQUAL(w.created, db->head_block_time());
            BOOST_CHECK_EQUAL(to_string(w.url), "bar.foo");
            BOOST_CHECK_EQUAL(w.signing_key, op.block_signing_key);
            BOOST_CHECK_EQUAL(w.props.account_creation_fee, op.props.account_creation_fee);
            BOOST_CHECK_EQUAL(w.props.maximum_block_size, op.props.maximum_block_size);
            BOOST_CHECK_EQUAL(w.total_missed, 0u);
            BOOST_CHECK_EQUAL(w.last_aslot, 0u);
            BOOST_CHECK_EQUAL(w.last_confirmed_block_num, 0);
            BOOST_CHECK_EQUAL(w.pow_worker, 0);
            BOOST_CHECK_EQUAL(w.votes.value, 0);
            BOOST_CHECK_EQUAL(w.virtual_last_update, 0);
            BOOST_CHECK_EQUAL(w.virtual_position, 0);
            BOOST_CHECK_EQUAL(w.virtual_scheduled_time, fc::uint128_t::max_value());
            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("10.000 GOLOS").amount.value);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(chain_properties_update_validate) { try {
        BOOST_TEST_MESSAGE("!NOT READY! Testing: chain_properties_update_validate");
        // TODO
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(chain_properties_update_authorities) { try {
        BOOST_TEST_MESSAGE("!NOT READY! Testing: chain_properties_update_authorities");
        // TODO
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(chain_properties_update_apply) { try {
        BOOST_TEST_MESSAGE("!NOT READY! Testing: chain_properties_update_apply");
        // TODO
    } FC_LOG_AND_RETHROW() }


    BOOST_AUTO_TEST_CASE(account_witness_vote_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_witness_vote_validate");
            account_witness_vote_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.account = "bob";
            op.witness = "alice";
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failed when 'account' is empty");
            CHECK_PARAM_INVALID(op, account, "");

            BOOST_TEST_MESSAGE("--- failed when 'witness' is empty");
            CHECK_PARAM_INVALID(op, witness, "");

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_witness_vote_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_witness_vote_authorities");

            ACTORS((alice)(bob)(sam))

            fund("alice", 1000);
            private_key_type alice_witness_key = generate_private_key("alice_witness");
            witness_create("alice", alice_private_key, "foo.bar", alice_witness_key.get_public_key(), 1000);

            account_witness_vote_operation op;
            op.account = "bob";
            op.witness = "alice";

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            BOOST_TEST_MESSAGE("--- Test failure when no signatures");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by a signature not in the account's authority");
            tx.sign(bob_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate signatures");
            tx.signatures.clear();
            tx.sign(bob_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by an additional signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(bob_private_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success with witness signature");
            tx.signatures.clear();
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Test failure with proxy signature");
            proxy("bob", "sam");
            tx.signatures.clear();
            tx.sign(sam_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_witness_vote_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_witness_vote_apply");

            ACTORS((alice)(bob)(sam))
            fund("alice", 5000);
            vest("alice", 5000);
            fund("sam", 1000);

            private_key_type sam_witness_key = generate_private_key("sam_key");
            witness_create("sam", sam_private_key, "foo.bar", sam_witness_key.get_public_key(), 1000);
            const witness_object &sam_witness = db->get_witness("sam");

            const auto &witness_vote_idx = db->get_index<witness_vote_index>().indices().get<by_witness_account>();

            BOOST_TEST_MESSAGE("--- Test normal vote");
            account_witness_vote_operation op;
            op.account = "alice";
            op.witness = "sam";
            op.approve = true;

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(sam_witness.votes, alice.vesting_shares.amount);
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, alice.id)) !=
                    witness_vote_idx.end());
            validate_database();

            BOOST_TEST_MESSAGE("--- Test revoke vote");
            op.approve = false;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
            BOOST_CHECK_EQUAL(sam_witness.votes.value, 0);
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, alice.id)) ==
                    witness_vote_idx.end());

            BOOST_TEST_MESSAGE("--- Test failure when attempting to revoke a non-existent vote");

            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::witness_vote_does_not_exist)));

            BOOST_CHECK_EQUAL(sam_witness.votes.value, 0);
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, alice.id)) ==
                    witness_vote_idx.end());

            BOOST_TEST_MESSAGE("--- Test proxied vote");
            proxy("alice", "bob");
            tx.operations.clear();
            tx.signatures.clear();
            op.approve = true;
            op.account = "bob";
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(sam_witness.votes, (bob.proxied_vsf_votes_total() + bob.vesting_shares.amount));
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, bob.id)) !=
                    witness_vote_idx.end());
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, alice.id)) ==
                    witness_vote_idx.end());

            BOOST_TEST_MESSAGE("--- Test vote from a proxied account");
            tx.operations.clear();
            tx.signatures.clear();
            op.account = "alice";
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::cannot_vote_when_route_are_set)));

            BOOST_CHECK_EQUAL(sam_witness.votes, (bob.proxied_vsf_votes_total() + bob.vesting_shares.amount));
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, bob.id)) !=
                    witness_vote_idx.end());
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, alice.id)) ==
                    witness_vote_idx.end());

            BOOST_TEST_MESSAGE("--- Test revoke proxied vote");
            tx.operations.clear();
            tx.signatures.clear();
            op.account = "bob";
            op.approve = false;
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(sam_witness.votes.value, 0);
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, bob.id)) ==
                    witness_vote_idx.end());
            BOOST_CHECK(
                    witness_vote_idx.find(std::make_tuple(sam_witness.id, alice.id)) ==
                    witness_vote_idx.end());

            BOOST_TEST_MESSAGE("--- Test failure when voting for a non-existent account");
            tx.operations.clear();
            tx.signatures.clear();
            op.witness = "dave";
            op.approve = true;
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "witness", "dave")));

            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when voting for an account that is not a witness");
            tx.operations.clear();
            tx.signatures.clear();
            op.witness = "alice";
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "witness", "alice")));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_witness_proxy_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_witness_proxy_validate");
            account_witness_proxy_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.account = "bob";
            op.proxy = "alice";
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failed when 'account' is empty");
            CHECK_PARAM_INVALID(op, account, "");

            BOOST_TEST_MESSAGE("--- success when 'proxy' is empty");
            CHECK_PARAM_VALID(op, proxy, "");

            BOOST_TEST_MESSAGE("--- failed when 'proxy' not valid");
            CHECK_PARAM_INVALID(op, proxy, "a");
            CHECK_PARAM_INVALID(op, proxy, "bob");

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_witness_proxy_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_witness_proxy_authorities");

            ACTORS((alice)(bob))

            account_witness_proxy_operation op;
            op.account = "bob";
            op.proxy = "alice";

            signed_transaction tx;
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            BOOST_TEST_MESSAGE("--- Test failure when no signatures");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by a signature not in the account's authority");
            tx.sign(bob_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate signatures");
            tx.signatures.clear();
            tx.sign(bob_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by an additional signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(bob_private_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success with witness signature");
            tx.signatures.clear();
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Test failure with proxy signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_witness_proxy_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_witness_proxy_apply");

            ACTORS((alice)(bob)(sam)(dave))
            fund("alice", 1000);
            vest("alice", 1000);
            fund("bob", 3000);
            vest("bob", 3000);
            fund("sam", 5000);
            vest("sam", 5000);
            fund("dave", 7000);
            vest("dave", 7000);

            BOOST_TEST_MESSAGE("--- Test setting proxy to another account from self.");
            // bob -> alice

            account_witness_proxy_operation op;
            op.account = "bob";
            op.proxy = "alice";

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(bob_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(bob.proxy, "alice");
            BOOST_CHECK_EQUAL(bob.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(alice.proxy, STEEMIT_PROXY_TO_SELF_ACCOUNT);
            BOOST_CHECK_EQUAL(alice.proxied_vsf_votes_total(),
                          bob.vesting_shares.amount);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test changing proxy");
            // bob->sam

            tx.operations.clear();
            tx.signatures.clear();
            op.proxy = "sam";
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(bob.proxy, "sam");
            BOOST_CHECK_EQUAL(bob.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(alice.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(sam.proxy, STEEMIT_PROXY_TO_SELF_ACCOUNT);
            BOOST_CHECK_EQUAL(sam.proxied_vsf_votes_total().value, bob.vesting_shares.amount);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when changing proxy to existing proxy");

            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::proxy_must_change)));

            BOOST_CHECK_EQUAL(bob.proxy, "sam");
            BOOST_CHECK_EQUAL(bob.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(sam.proxy, STEEMIT_PROXY_TO_SELF_ACCOUNT);
            BOOST_CHECK_EQUAL(sam.proxied_vsf_votes_total(), bob.vesting_shares.amount);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test adding a grandparent proxy");
            // bob->sam->dave

            tx.operations.clear();
            tx.signatures.clear();
            op.proxy = "dave";
            op.account = "sam";
            tx.operations.push_back(op);
            tx.sign(sam_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(bob.proxy, "sam");
            BOOST_CHECK_EQUAL(bob.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(sam.proxy, "dave");
            BOOST_CHECK_EQUAL(sam.proxied_vsf_votes_total(), bob.vesting_shares.amount);
            BOOST_CHECK_EQUAL(dave.proxy, STEEMIT_PROXY_TO_SELF_ACCOUNT);
            BOOST_CHECK_EQUAL(dave.proxied_vsf_votes_total(),
                          (sam.vesting_shares + bob.vesting_shares).amount);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test adding a grandchild proxy");
            // alice
            // bob-> sam-> dave

            tx.operations.clear();
            tx.signatures.clear();
            op.proxy = "sam";
            op.account = "alice";
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(alice.proxy, "sam");
            BOOST_CHECK_EQUAL(alice.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(bob.proxy, "sam");
            BOOST_CHECK_EQUAL(bob.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(sam.proxy, "dave");
            BOOST_CHECK_EQUAL(sam.proxied_vsf_votes_total(),
                          (bob.vesting_shares + alice.vesting_shares).amount);
            BOOST_CHECK_EQUAL(dave.proxy, STEEMIT_PROXY_TO_SELF_ACCOUNT);
            BOOST_CHECK_EQUAL(dave.proxied_vsf_votes_total(),
                          (sam.vesting_shares + bob.vesting_shares +
                           alice.vesting_shares).amount);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test removing a grandchild proxy");
            // alice->sam->dave

            tx.operations.clear();
            tx.signatures.clear();
            op.proxy = STEEMIT_PROXY_TO_SELF_ACCOUNT;
            op.account = "bob";
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(alice.proxy, "sam");
            BOOST_CHECK_EQUAL(alice.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(bob.proxy, STEEMIT_PROXY_TO_SELF_ACCOUNT);
            BOOST_CHECK_EQUAL(bob.proxied_vsf_votes_total().value, 0);
            BOOST_CHECK_EQUAL(sam.proxy, "dave");
            BOOST_CHECK_EQUAL(sam.proxied_vsf_votes_total(), alice.vesting_shares.amount);
            BOOST_CHECK_EQUAL(dave.proxy, STEEMIT_PROXY_TO_SELF_ACCOUNT);
            BOOST_CHECK_EQUAL(dave.proxied_vsf_votes_total(),
                          (sam.vesting_shares + alice.vesting_shares).amount);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test votes are transferred when a proxy is added");
            account_witness_vote_operation vote;
            vote.account = "bob";
            vote.witness = STEEMIT_INIT_MINER_NAME;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(vote);
            tx.sign(bob_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            tx.operations.clear();
            tx.signatures.clear();
            op.account = "alice";
            op.proxy = "bob";
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(db->get_witness(STEEMIT_INIT_MINER_NAME).votes,
                          (alice.vesting_shares + bob.vesting_shares).amount);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test votes are removed when a proxy is removed");
            op.proxy = STEEMIT_PROXY_TO_SELF_ACCOUNT;
            tx.signatures.clear();
            tx.operations.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(db->get_witness(STEEMIT_INIT_MINER_NAME).votes, bob.vesting_shares.amount);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(custom_authorities) {
        custom_operation op;
        op.required_auths.insert("alice");
        op.required_auths.insert("bob");
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"alice","bob"}), account_name_set());
    }

    BOOST_AUTO_TEST_CASE(custom_json_authorities) {
        custom_json_operation op;
        op.required_auths.insert("alice");
        op.required_posting_auths.insert("bob");
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"alice"}), account_name_set({"bob"}));
    }

    BOOST_AUTO_TEST_CASE(custom_json_validate) {
        custom_json_operation op;

        GOLOS_CHECK_ERROR_PROPS(op.validate(),
            CHECK_ERROR(invalid_parameter, "required_auths"));

        op.required_auths.insert("alice");
        op.id = "id";
        op.json = "{}";
        CHECK_OP_VALID(op);

        CHECK_PARAM_INVALID(op, id, std::string(33, 's'));
        CHECK_PARAM_INVALID(op, json, "{]");
    }

    BOOST_AUTO_TEST_CASE(custom_binary_authorities) {
        ACTORS((alice))

        auto alice_authority = db->get<account_authority_object, by_account>("alice").posting;

        custom_binary_operation op;
        op.required_owner_auths.insert("alice");
        op.required_active_auths.insert("bob");
        op.required_posting_auths.insert("sam");
        op.required_auths.push_back(alice_authority);

        CHECK_OP_AUTHS(op, account_name_set({"alice"}), account_name_set({"bob"}), account_name_set({"sam"}));

        vector<authority> expected = {alice_authority};
        vector<authority> auths;
        op.get_required_authorities(auths);
        BOOST_CHECK_EQUAL(auths, expected);
    }

    BOOST_AUTO_TEST_CASE(custom_binary_validate) {
        custom_binary_operation op;

        GOLOS_CHECK_ERROR_PROPS(op.validate(),
            CHECK_ERROR(invalid_parameter, "required_owner_auths"));

        op.required_active_auths.insert("alice");
        op.id = "id";
        CHECK_OP_VALID(op);

        CHECK_PARAM_INVALID(op, id, std::string(33, 's'));
        
        authority auth;
        auth.add_authority("a", 1);
        op.required_auths.push_back(auth);
        GOLOS_CHECK_ERROR_PROPS(op.validate(),
            CHECK_ERROR(invalid_parameter, "required_auths"));
    }

    BOOST_AUTO_TEST_CASE(feed_publish_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: feed_publish_validate");
            feed_publish_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.publisher = "alice";
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failed when 'publisher' is empty");
            CHECK_PARAM_INVALID(op, publisher, "");

            BOOST_TEST_MESSAGE("--- failed when 'exchange_rate' not for GOLOS/GBG market");
            CHECK_PARAM_INVALID_LOGIC(op, exchange_rate, price(ASSET("1.000000 GESTS"), ASSET("1.000 GBG")),
                    price_feed_must_be_for_golos_gbg_market);

            BOOST_TEST_MESSAGE("--- failed when 'exchange_rate' is invalid");
            CHECK_PARAM_INVALID(op, exchange_rate, price(ASSET("0.000 GOLOS"), ASSET("1.000 GBG")));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(feed_publish_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: feed_publish_authorities");

            ACTORS((alice)(bob))
            fund("alice", 10000);
            witness_create("alice", alice_private_key, "foo.bar", alice_private_key.get_public_key(), 1000);

            feed_publish_operation op;
            op.publisher = "alice";
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            BOOST_TEST_MESSAGE("--- Test failure when no signature.");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure with incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure with duplicate signature");
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with additional incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success with witness account signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, database::skip_transaction_dupe_check));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(feed_publish_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: feed_publish_apply");

            ACTORS((alice))
            fund("alice", 10000);
            witness_create("alice", alice_private_key, "foo.bar", alice_private_key.get_public_key(), 1000);

            BOOST_TEST_MESSAGE("--- Test publishing price feed");
            feed_publish_operation op;
            op.publisher = "alice";
            op.exchange_rate = price(ASSET("1000.000 GOLOS"), ASSET("1.000 GBG")); // 1000 STEEM : 1 SBD

            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            witness_object &alice_witness = const_cast< witness_object & >( db->get_witness("alice"));

            BOOST_CHECK_EQUAL(alice_witness.sbd_exchange_rate, op.exchange_rate);
            BOOST_CHECK_EQUAL(alice_witness.last_sbd_exchange_update, db->head_block_time());
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure publishing to non-existent witness");

            tx.operations.clear();
            tx.signatures.clear();
            op.publisher = "bob";
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            // In this case missing_object thrown at authority checking step
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(missing_object, "authority", "bob"));
            validate_database();

            BOOST_TEST_MESSAGE("--- Test updating price feed");

            tx.operations.clear();
            tx.signatures.clear();
            op.exchange_rate = price(ASSET(" 1500.000 GOLOS"), ASSET("1.000 GBG"));
            op.publisher = "alice";
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            alice_witness = const_cast< witness_object & >( db->get_witness("alice"));
            BOOST_CHECK_LT(std::abs(alice_witness.sbd_exchange_rate.to_real() -
                                   op.exchange_rate.to_real()), 0.0000005);
            BOOST_CHECK_EQUAL(alice_witness.last_sbd_exchange_update, db->head_block_time());
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(convert_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: convert_validate");
            convert_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.owner = "alice";
            op.amount = ASSET("10.000 GBG");
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failed when 'owner' is empty");
            CHECK_PARAM_INVALID(op, owner, "");

            BOOST_TEST_MESSAGE("--- failed when 'amount' is invalid");
            CHECK_PARAM_INVALID(op, amount, ASSET("10.000000 GESTS"));
            CHECK_PARAM_INVALID(op, amount, ASSET("-10.000 GBG"));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(convert_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: convert_authorities");

            ACTORS((alice)(bob))
            fund("alice", 10000);

            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            convert("alice", ASSET("2.500 GOLOS"));

            validate_database();

            convert_operation op;
            op.owner = "alice";
            op.amount = ASSET("2.500 GBG");

            signed_transaction tx;
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            BOOST_TEST_MESSAGE("--- Test failure when no signatures");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0), 
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by a signature not in the account's authority");
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0), 
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test failure when duplicate signatures");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0), 
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure when signed by an additional signature not in the creator's authority");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0), 
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test success with owner signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(convert_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: convert_apply");
            ACTORS((alice)(bob));
            fund("alice", 10000);
            fund("bob", 10000);

            convert_operation op;
            signed_transaction tx;

            const auto &convert_request_idx = db->get_index<convert_request_index>().indices().get<by_owner>();

            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            convert("alice", ASSET("2.500 GOLOS"));
            convert("bob", ASSET("7.000 GOLOS"));

            const auto &new_alice = db->get_account("alice");
            const auto &new_bob = db->get_account("bob");

            BOOST_TEST_MESSAGE("--- Test failure when account does not have the required GOLOS (invalid parameter, only GBG)");
            op.owner = "bob";
            op.amount = ASSET("5.000 GOLOS");
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            // Convert operation only available for GBG
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "amount")));

            BOOST_CHECK_EQUAL(new_bob.balance.amount.value, ASSET("3.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_bob.sbd_balance.amount.value, ASSET("7.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when account does not have the required GBG");
            op.owner = "alice";
            op.amount = ASSET("5.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "alice", "fund", "5.000 GBG")));

            BOOST_CHECK_EQUAL(new_alice.balance.amount.value, ASSET("7.500 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_alice.sbd_balance.amount.value, ASSET("2.500 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when account does not exist");
            op.owner = "sam";
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(missing_object, "authority", "sam"));

            BOOST_TEST_MESSAGE("--- Test success converting GBG to GOLOS");
            op.owner = "bob";
            op.amount = ASSET("3.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK_EQUAL(new_bob.balance.amount.value, ASSET("3.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_bob.sbd_balance.amount.value, ASSET("4.000 GBG").amount.value);

            auto convert_request = convert_request_idx.find(std::make_tuple(op.owner, op.requestid));
            BOOST_CHECK(convert_request != convert_request_idx.end());
            BOOST_CHECK_EQUAL(convert_request->owner, op.owner);
            BOOST_CHECK_EQUAL(convert_request->requestid, op.requestid);
            BOOST_CHECK_EQUAL(convert_request->amount.amount.value, op.amount.amount.value);
            //BOOST_CHECK_EQUAL( convert_request->premium, 100000 );
            BOOST_CHECK_EQUAL(convert_request->conversion_date, db->head_block_time() + STEEMIT_CONVERSION_DELAY);

            BOOST_TEST_MESSAGE("--- Test failure from repeated id");
            op.amount = ASSET("2.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(object_already_exist, "convert_request", make_convert_request_id("bob", 0))));

            BOOST_CHECK_EQUAL(new_bob.balance.amount.value, ASSET("3.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(new_bob.sbd_balance.amount.value, ASSET("4.000 GBG").amount.value);

            convert_request = convert_request_idx.find(std::make_tuple(op.owner, op.requestid));
            BOOST_CHECK(convert_request != convert_request_idx.end());
            BOOST_CHECK_EQUAL(convert_request->owner, op.owner);
            BOOST_CHECK_EQUAL(convert_request->requestid, op.requestid);
            BOOST_CHECK_EQUAL(convert_request->amount.amount.value, ASSET("3.000 GBG").amount.value);
            //BOOST_CHECK_EQUAL( convert_request->premium, 100000 );
            BOOST_CHECK_EQUAL(convert_request->conversion_date, db->head_block_time() + STEEMIT_CONVERSION_DELAY);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_create_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_create_validate");
            limit_order_create_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.owner = "alice";
            op.amount_to_sell = ASSET("1.000 GOLOS");
            op.min_to_receive = ASSET("1.000 GBG");
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failed when 'owner' is empty");
            CHECK_PARAM_INVALID(op, owner, "");

            BOOST_TEST_MESSAGE("--- failed when 'amount_to_sell' is negative");
            CHECK_PARAM_VALIDATION_FAIL(op, amount_to_sell, ASSET_GOLOS(-1),
                CHECK_ERROR(invalid_parameter, "price"));

            BOOST_TEST_MESSAGE("--- failed when symbol not GBG or GOLOS");
            CHECK_PARAM_INVALID_LOGIC(op, min_to_receive, ASSET_GESTS(1),
                    limit_order_must_be_for_golos_gbg_market);

            BOOST_TEST_MESSAGE("--- failed when 'min_to_receive' is negative");
            op.amount_to_sell = ASSET("1.000 GOLOS");
            op.min_to_receive = ASSET("-1.000 GBG");
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "price"));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_create_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_create_authorities");

            ACTORS((alice)(bob))
            fund("alice", 10000);

            limit_order_create_operation op;
            op.owner = "alice";
            op.amount_to_sell = ASSET("1.000 GOLOS");
            op.min_to_receive = ASSET("1.000 GBG");

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            BOOST_TEST_MESSAGE("--- Test failure when no signature.");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test success with account signature");
            tx.sign(alice_private_key, db->get_chain_id());
            db->push_transaction(tx, database::skip_transaction_dupe_check);

            BOOST_TEST_MESSAGE("--- Test failure with duplicate signature");
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with additional incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_create_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_create_apply");

            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            ACTORS((alice)(bob))
            fund("alice", 1000000);
            fund("bob", 1000000);
            convert("bob", ASSET("1000.000 GOLOS"));

            const auto &limit_order_idx = db->get_index<limit_order_index>().indices().get<by_account>();

            BOOST_TEST_MESSAGE("--- Test failure when account does not have required funds");
            limit_order_create_operation op;
            signed_transaction tx;

            op.owner = "bob";
            op.orderid = 1;
            op.amount_to_sell = ASSET("10.000 GOLOS");
            op.min_to_receive = ASSET("10.000 GBG");
            op.fill_or_kill = false;
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "bob", "fund", "10.000 GOLOS")));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("bob", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(bob.balance.amount.value, ASSET("0.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value, ASSET("100.0000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when amount to receive is 0");

            op.owner = "alice";
            op.min_to_receive = ASSET("0.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "price")));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("alice", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("1000.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value, ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when amount to sell is 0");

            op.amount_to_sell = ASSET("0.000 GOLOS");
            op.min_to_receive = ASSET("10.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "price")));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("alice", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("1000.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value, ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test success creating limit order that will not be filled");

            op.amount_to_sell = ASSET("10.000 GOLOS");
            op.min_to_receive = ASSET("15.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            auto limit_order = limit_order_idx.find(std::make_tuple("alice", op.orderid));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, op.owner);
            BOOST_CHECK_EQUAL(limit_order->orderid, op.orderid);
            BOOST_CHECK_EQUAL(limit_order->for_sale, op.amount_to_sell.amount);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(op.amount_to_sell / op.min_to_receive));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value, ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure creating limit order with duplicate id");

            op.amount_to_sell = ASSET("20.000 GOLOS");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(object_already_exist, "limit_order", make_limit_order_id("alice", 1))));

            limit_order = limit_order_idx.find(std::make_tuple("alice", op.orderid));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, op.owner);
            BOOST_CHECK_EQUAL(limit_order->orderid, op.orderid);
            BOOST_CHECK_EQUAL(limit_order->for_sale, 10000);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("10.000 GOLOS"), op.min_to_receive));
            BOOST_CHECK_EQUAL(limit_order->get_market(), std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value, ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test sucess killing an order that will not be filled");

            op.orderid = 2;
            op.fill_or_kill = true;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::cancelling_not_filled_order)));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("alice", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value, ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value, ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test having a partial match to limit order");
            // Alice has order for 15 SBD at a price of 2:3
            // Fill 5 STEEM for 7.5 SBD

            op.owner = "bob";
            op.orderid = 1;
            op.amount_to_sell = ASSET("7.500 GBG");
            op.min_to_receive = ASSET("5.000 GOLOS");
            op.fill_or_kill = false;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            auto recent_ops = get_last_operations(1);
            auto fill_order_op = recent_ops[0].get<fill_order_operation>();

            limit_order = limit_order_idx.find(std::make_tuple("alice", 1));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "alice");
            BOOST_CHECK_EQUAL(limit_order->orderid, op.orderid);
            BOOST_CHECK_EQUAL(limit_order->for_sale, 5000);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("10.000 GOLOS"), ASSET("15.000 GBG")));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("bob", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("7.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("5.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("992.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(fill_order_op.open_owner, "alice");
            BOOST_CHECK_EQUAL(fill_order_op.open_orderid, 1u);
            BOOST_CHECK_EQUAL(fill_order_op.open_pays.amount.value,
                          ASSET("5.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(fill_order_op.current_owner, "bob");
            BOOST_CHECK_EQUAL(fill_order_op.current_orderid, 1);
            BOOST_CHECK_EQUAL(fill_order_op.current_pays.amount.value,
                          ASSET("7.500 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test filling an existing order fully, but the new order partially");

            op.amount_to_sell = ASSET("15.000 GBG");
            op.min_to_receive = ASSET("10.000 GOLOS");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            limit_order = limit_order_idx.find(std::make_tuple("bob", 1));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "bob");
            BOOST_CHECK_EQUAL(limit_order->orderid, 1);
            BOOST_CHECK_EQUAL(limit_order->for_sale.value, 7500);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("15.000 GBG"), ASSET("10.000 GOLOS")));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 1)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("15.000 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("10.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("977.500 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test filling an existing order and new order fully");

            op.owner = "alice";
            op.orderid = 3;
            op.amount_to_sell = ASSET("5.000 GOLOS");
            op.min_to_receive = ASSET("7.500 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 3)) ==
                          limit_order_idx.end());
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("bob", 1)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("985.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("22.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("15.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("977.500 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test filling limit order with better order when partial order is better.");

            op.owner = "alice";
            op.orderid = 4;
            op.amount_to_sell = ASSET("10.000 GOLOS");
            op.min_to_receive = ASSET("11.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            op.owner = "bob";
            op.orderid = 4;
            op.amount_to_sell = ASSET("12.000 GBG");
            op.min_to_receive = ASSET("10.000 GOLOS");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            limit_order = limit_order_idx.find(std::make_tuple("bob", 4));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 4)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "bob");
            BOOST_CHECK_EQUAL(limit_order->orderid, 4);
            BOOST_CHECK_EQUAL(limit_order->for_sale.value, 1000);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("12.000 GBG"), ASSET("10.000 GOLOS")));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("975.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("33.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("25.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("965.500 GBG").amount.value);
            validate_database();

            limit_order_cancel_operation can;
            can.owner = "bob";
            can.orderid = 4;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(can);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Test filling limit order with better order when partial order is worse.");

            op.owner = "alice";
            op.orderid = 5;
            op.amount_to_sell = ASSET("20.000 GOLOS");
            op.min_to_receive = ASSET("22.000 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            op.owner = "bob";
            op.orderid = 5;
            op.amount_to_sell = ASSET("12.000 GBG");
            op.min_to_receive = ASSET("10.000 GOLOS");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            limit_order = limit_order_idx.find(std::make_tuple("alice", 5));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("bob", 5)) == limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "alice");
            BOOST_CHECK_EQUAL(limit_order->orderid, 5);
            BOOST_CHECK_EQUAL(limit_order->for_sale.value, 9091);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("20.000 GOLOS"), ASSET("22.000 GBG")));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("955.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("45.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("35.909 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("954.500 GBG").amount.value);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_create2_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_create2_validate");
            limit_order_create2_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.owner = "alice";
            op.amount_to_sell = ASSET("1.000 GOLOS");
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- failed when 'owner' is empty");
            op.owner = "";
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "owner"));

            BOOST_TEST_MESSAGE("--- failed when 'exchange_rate' is invalid");
            op.owner = "alice";
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GOLOS"));
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "exchange_rate"));

            BOOST_TEST_MESSAGE("--- failed when symbol not GBG or GOLOS");
            op.amount_to_sell = ASSET("1.000000 GESTS");
            op.exchange_rate = price(ASSET("1.000000 GESTS"), ASSET("1.000 GBG"));
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(logic_exception, logic_exception::limit_order_must_be_for_golos_gbg_market));

            BOOST_TEST_MESSAGE("--- failed when zero amount to sell");
            op.amount_to_sell = ASSET("0.000 GOLOS");
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "amount_to_sell"));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_create2_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_create2_authorities");

            ACTORS((alice)(bob))
            fund("alice", 10000);

            limit_order_create2_operation op;
            op.owner = "alice";
            op.amount_to_sell = ASSET("1.000 GOLOS");
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            BOOST_TEST_MESSAGE("--- Test failure when no signature.");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test success with account signature");
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, database::skip_transaction_dupe_check));

            BOOST_TEST_MESSAGE("--- Test failure with duplicate signature");
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with additional incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_create2_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_create2_apply");

            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            ACTORS((alice)(bob))
            fund("alice", 1000000);
            fund("bob", 1000000);
            convert("bob", ASSET("1000.000 GOLOS"));

            const auto &limit_order_idx = db->get_index<limit_order_index>().indices().get<by_account>();

            BOOST_TEST_MESSAGE("--- Test failure when account does not have required funds");
            limit_order_create2_operation op;
            signed_transaction tx;

            op.owner = "bob";
            op.orderid = 1;
            op.amount_to_sell = ASSET("10.000 GOLOS");
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
            op.fill_or_kill = false;
            tx.operations.push_back(op);
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "bob", "fund", "10.000 GOLOS")));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("bob", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("0.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("100.0000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when price is 0");

            op.owner = "alice";
            op.exchange_rate = price(ASSET("0.000 GOLOS"), ASSET("1.000 GBG"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "exchange_rate")));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("alice", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("1000.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure when amount to sell is 0");

            op.amount_to_sell = ASSET("0.000 GOLOS");
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "amount_to_sell")));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("alice", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("1000.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test success creating limit order that will not be filled");

            op.amount_to_sell = ASSET("10.000 GOLOS");
            op.exchange_rate = price(ASSET("2.000 GOLOS"), ASSET("3.000 GBG"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            auto limit_order = limit_order_idx.find(std::make_tuple("alice", op.orderid));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, op.owner);
            BOOST_CHECK_EQUAL(limit_order->orderid, op.orderid);
            BOOST_CHECK_EQUAL(limit_order->for_sale, op.amount_to_sell.amount);
            BOOST_CHECK_EQUAL(limit_order->sell_price, op.exchange_rate);
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test failure creating limit order with duplicate id");

            op.amount_to_sell = ASSET("20.000 GOLOS");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(object_already_exist, "limit_order", make_limit_order_id("alice", 1))));

            limit_order = limit_order_idx.find(std::make_tuple("alice", op.orderid));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, op.owner);
            BOOST_CHECK_EQUAL(limit_order->orderid, op.orderid);
            BOOST_CHECK_EQUAL(limit_order->for_sale, 10000);
            BOOST_CHECK_EQUAL(limit_order->sell_price, op.exchange_rate);
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test sucess killing an order that will not be filled");

            op.orderid = 2;
            op.fill_or_kill = true;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::cancelling_not_filled_order)));

            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("alice", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("0.000 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test having a partial match to limit order");
            // Alice has order for 15 SBD at a price of 2:3
            // Fill 5 STEEM for 7.5 SBD

            op.owner = "bob";
            op.orderid = 1;
            op.amount_to_sell = ASSET("7.500 GBG");
            op.exchange_rate = price(ASSET("3.000 GBG"), ASSET("2.000 GOLOS"));
            op.fill_or_kill = false;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            auto recent_ops = get_last_operations(1);
            auto fill_order_op = recent_ops[0].get<fill_order_operation>();

            limit_order = limit_order_idx.find(std::make_tuple("alice", 1));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "alice");
            BOOST_CHECK_EQUAL(limit_order->orderid, op.orderid);
            BOOST_CHECK_EQUAL(limit_order->for_sale, 5000);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("2.000 GOLOS"), ASSET("3.000 GBG")));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK(
                    limit_order_idx.find(std::make_tuple("bob", op.orderid)) ==
                    limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("7.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("5.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("992.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(fill_order_op.open_owner, "alice");
            BOOST_CHECK_EQUAL(fill_order_op.open_orderid, 1);
            BOOST_CHECK_EQUAL(fill_order_op.open_pays.amount.value,
                          ASSET("5.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(fill_order_op.current_owner, "bob");
            BOOST_CHECK_EQUAL(fill_order_op.current_orderid, 1);
            BOOST_CHECK_EQUAL(fill_order_op.current_pays.amount.value,
                          ASSET("7.500 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test filling an existing order fully, but the new order partially");

            op.amount_to_sell = ASSET("15.000 GBG");
            op.exchange_rate = price(ASSET("3.000 GBG"), ASSET("2.000 GOLOS"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            limit_order = limit_order_idx.find(std::make_tuple("bob", 1));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "bob");
            BOOST_CHECK_EQUAL(limit_order->orderid, 1);
            BOOST_CHECK_EQUAL(limit_order->for_sale.value, 7500);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("3.000 GBG"), ASSET("2.000 GOLOS")));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 1)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("990.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("15.000 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("10.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("977.500 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test filling an existing order and new order fully");

            op.owner = "alice";
            op.orderid = 3;
            op.amount_to_sell = ASSET("5.000 GOLOS");
            op.exchange_rate = price(ASSET("2.000 GOLOS"), ASSET("3.000 GBG"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 3)) ==
                          limit_order_idx.end());
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("bob", 1)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("985.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("22.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("15.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("977.500 GBG").amount.value);
            validate_database();

            BOOST_TEST_MESSAGE("--- Test filling limit order with better order when partial order is better.");

            op.owner = "alice";
            op.orderid = 4;
            op.amount_to_sell = ASSET("10.000 GOLOS");
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.100 GBG"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            op.owner = "bob";
            op.orderid = 4;
            op.amount_to_sell = ASSET("12.000 GBG");
            op.exchange_rate = price(ASSET("1.200 GBG"), ASSET("1.000 GOLOS"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            limit_order = limit_order_idx.find(std::make_tuple("bob", 4));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 4)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "bob");
            BOOST_CHECK_EQUAL(limit_order->orderid, 4);
            BOOST_CHECK_EQUAL(limit_order->for_sale.value, 1000);
            BOOST_CHECK_EQUAL(limit_order->sell_price, op.exchange_rate);
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("975.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("33.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("25.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("965.500 GBG").amount.value);
            validate_database();

            limit_order_cancel_operation can;
            can.owner = "bob";
            can.orderid = 4;
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(can);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Test filling limit order with better order when partial order is worse.");

            op.owner = "alice";
            op.orderid = 5;
            op.amount_to_sell = ASSET("20.000 GOLOS");
            op.exchange_rate = price(ASSET("1.000 GOLOS"), ASSET("1.100 GBG"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            op.owner = "bob";
            op.orderid = 5;
            op.amount_to_sell = ASSET("12.000 GBG");
            op.exchange_rate = price(ASSET("1.200 GBG"), ASSET("1.000 GOLOS"));
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            limit_order = limit_order_idx.find(std::make_tuple("alice", 5));
            BOOST_CHECK(limit_order != limit_order_idx.end());
            BOOST_CHECK(limit_order_idx.find(std::make_tuple("bob", 5)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(limit_order->seller, "alice");
            BOOST_CHECK_EQUAL(limit_order->orderid, 5);
            BOOST_CHECK_EQUAL(limit_order->for_sale.value, 9091);
            BOOST_CHECK_EQUAL(limit_order->sell_price,
                          price(ASSET("1.000 GOLOS"), ASSET("1.100 GBG")));
            BOOST_CHECK_EQUAL(limit_order->get_market(),
                          std::make_pair(SBD_SYMBOL, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("955.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("45.500 GBG").amount.value);
            BOOST_CHECK_EQUAL(bob.balance.amount.value,
                          ASSET("35.909 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(bob.sbd_balance.amount.value,
                          ASSET("954.500 GBG").amount.value);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_cancel_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_cancel_validate");
            limit_order_cancel_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.owner = "alice";
            op.orderid = 1;
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failure when owner is empty");
            CHECK_PARAM_INVALID(op, owner, "");

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_cancel_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_cancel_authorities");

            ACTORS((alice)(bob))
            fund("alice", 10000);

            limit_order_create_operation c;
            c.owner = "alice";
            c.orderid = 1;
            c.amount_to_sell = ASSET("1.000 GOLOS");
            c.min_to_receive = ASSET("1.000 GBG");

            signed_transaction tx;
            tx.operations.push_back(c);
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            limit_order_cancel_operation op;
            op.owner = "alice";
            op.orderid = 1;

            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);

            BOOST_TEST_MESSAGE("--- Test failure when no signature.");
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            BOOST_TEST_MESSAGE("--- Test success with account signature");
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, database::skip_transaction_dupe_check));

            BOOST_TEST_MESSAGE("--- Test failure with duplicate signature");
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_duplicate_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with additional incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_irrelevant_sig, 0));

            BOOST_TEST_MESSAGE("--- Test failure with incorrect signature");
            tx.signatures.clear();
            tx.sign(alice_post_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, database::skip_transaction_dupe_check),
                CHECK_ERROR(tx_missing_active_auth, 0));

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(limit_order_cancel_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: limit_order_cancel_apply");

            ACTORS((alice))
            fund("alice", 10000);

            const auto &limit_order_idx = db->get_index<limit_order_index>().indices().get<by_account>();

            BOOST_TEST_MESSAGE("--- Test cancel non-existent order");

            limit_order_cancel_operation op;
            signed_transaction tx;

            op.owner = "alice";
            op.orderid = 5;
            tx.operations.push_back(op);
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "limit_order", make_limit_order_id("alice", 5))));

            BOOST_TEST_MESSAGE("--- Test cancel order");

            limit_order_create_operation create;
            create.owner = "alice";
            create.orderid = 5;
            create.amount_to_sell = ASSET("5.000 GOLOS");
            create.min_to_receive = ASSET("7.500 GBG");
            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(create);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 5)) !=
                          limit_order_idx.end());

            tx.operations.clear();
            tx.signatures.clear();
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_CHECK(limit_order_idx.find(std::make_tuple("alice", 5)) ==
                          limit_order_idx.end());
            BOOST_CHECK_EQUAL(alice.balance.amount.value,
                          ASSET("10.000 GOLOS").amount.value);
            BOOST_CHECK_EQUAL(alice.sbd_balance.amount.value,
                          ASSET("0.000 GBG").amount.value);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(pow_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: pow_validate");
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(pow_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: pow_authorities");
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(pow_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: pow_apply");
        }
        FC_LOG_AND_RETHROW()
    }

//-------------------------------------------------------------
    BOOST_AUTO_TEST_SUITE(recovery)

#define TRIVIAL_AUTHORITY authority(0, "acc0", 1)
#define IMPOSSIBLE_AUTHORITY authority(3, "acc1", 1, "acc2", 1)
#define INVALID_AUTHORITY authority(2, "good1", 1, "2bad", 1)

    BOOST_AUTO_TEST_SUITE(request_account_recovery)

    BOOST_AUTO_TEST_CASE(request_account_recovery_validate) { try {
        BOOST_TEST_MESSAGE("Testing: request_account_recovery_validate");
        request_account_recovery_operation op;
        op.recovery_account = "alice";
        op.account_to_recover = "bob";
        op.new_owner_authority = authority(1, "sam", 1);
        BOOST_TEST_MESSAGE("--- success on valid params");
        CHECK_OP_VALID(op);
        CHECK_PARAM_VALID(op, new_owner_authority, authority(3, "one", 1, "two", 2));
        CHECK_PARAM_VALID(op, new_owner_authority, authority());
        CHECK_PARAM_VALID(op, new_owner_authority, TRIVIAL_AUTHORITY);
        CHECK_PARAM_VALID(op, new_owner_authority, IMPOSSIBLE_AUTHORITY);

        BOOST_TEST_MESSAGE("--- fail when recovery_account or account_to_recover invalid");
        CHECK_PARAM_INVALID(op, recovery_account, "");
        CHECK_PARAM_INVALID(op, account_to_recover, "");

        BOOST_TEST_MESSAGE("--- fail when new_owner_authority invalid");
        // TODO: create separate test for authority::validate()
        CHECK_PARAM_INVALID(op, new_owner_authority, authority(1, "no", 1));
        CHECK_PARAM_INVALID(op, new_owner_authority, authority(2, "good", 1, "no", 1));
        CHECK_PARAM_INVALID(op, new_owner_authority, INVALID_AUTHORITY);

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(request_account_recovery_authorities) { try {
        BOOST_TEST_MESSAGE("Testing: request_account_recovery_authorities");
        request_account_recovery_operation op;
        op.recovery_account = "alice";
        op.account_to_recover = "bob";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"alice"}), account_name_set());
    } FC_LOG_AND_RETHROW() }

    // tested in account_recovery and change_recovery_account test cases
    // BOOST_AUTO_TEST_CASE(request_account_recovery_apply) { try {
    // } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_SUITE_END() // request_account_recovery


    BOOST_AUTO_TEST_SUITE(recover_account)

    BOOST_AUTO_TEST_CASE(recover_account_validate) { try {
        BOOST_TEST_MESSAGE("Testing: recover_account_validate");
        recover_account_operation op;
        op.account_to_recover = "bob";
        op.new_owner_authority = authority(1, "alice", 1);
        op.recent_owner_authority = authority(1, "sam", 1);
        // TODO: key-based authorities
        BOOST_TEST_MESSAGE("--- success on valid params");
        CHECK_OP_VALID(op);
        CHECK_PARAM_VALID(op, recent_owner_authority, authority());
        CHECK_PARAM_VALID(op, recent_owner_authority, TRIVIAL_AUTHORITY);

        BOOST_TEST_MESSAGE("--- fail when account_to_recover is invalid");
        CHECK_PARAM_INVALID(op, account_to_recover, "");

        BOOST_TEST_MESSAGE("--- fail when new_owner_authority is bad");
        CHECK_PARAM_INVALID_LOGIC(op, new_owner_authority, authority(1, "sam", 1), cannot_set_recent_recovery);
        CHECK_PARAM_INVALID(op, new_owner_authority, authority());
        CHECK_PARAM_INVALID(op, new_owner_authority, TRIVIAL_AUTHORITY);
        CHECK_PARAM_INVALID(op, new_owner_authority, IMPOSSIBLE_AUTHORITY);
        CHECK_PARAM_INVALID(op, new_owner_authority, INVALID_AUTHORITY);

        BOOST_TEST_MESSAGE("--- fail when recent_owner_authority is bad");
        CHECK_PARAM_INVALID(op, recent_owner_authority, IMPOSSIBLE_AUTHORITY);
        CHECK_PARAM_INVALID(op, recent_owner_authority, INVALID_AUTHORITY);

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(recover_account_authorities) { try {
        BOOST_TEST_MESSAGE("Testing: recover_account_authorities");
        recover_account_operation op;
        op.account_to_recover = "alice";
        op.new_owner_authority = authority(1, "bob", 1);
        op.recent_owner_authority = authority(1, "sam", 1);
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set());
        const auto bob_and_sam = vector<authority>{op.new_owner_authority, op.recent_owner_authority};
        vector<authority> req;
        op.get_required_authorities(req);
        BOOST_CHECK_EQUAL(bob_and_sam, req);
        // TODO: maybe here can be some more complex checks (like multisig)
    } FC_LOG_AND_RETHROW() }

    // tested in account_recovery and change_recovery_account test cases
    // BOOST_AUTO_TEST_CASE(recover_account_apply) { try {
    // } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_SUITE_END() // recover_account


    BOOST_AUTO_TEST_SUITE(change_recovery_account)

    BOOST_AUTO_TEST_CASE(change_recovery_account_validate) { try {
        BOOST_TEST_MESSAGE("Testing: change_recovery_account_validate");
        change_recovery_account_operation op;
        op.account_to_recover = "alice";
        op.new_recovery_account = "bob";
        BOOST_TEST_MESSAGE("--- success on valid params");
        CHECK_OP_VALID(op);

        BOOST_TEST_MESSAGE("--- fail when account_to_recover or new_recovery_account is invalid");
        CHECK_PARAM_INVALID(op, account_to_recover, "");
        CHECK_PARAM_INVALID(op, new_recovery_account, "");

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(change_recovery_account_authorities) { try {
        BOOST_TEST_MESSAGE("Testing: change_recovery_account_authorities");
        change_recovery_account_operation op;
        op.account_to_recover = "alice";
        op.new_recovery_account = "bob";
        CHECK_OP_AUTHS(op, account_name_set({"alice"}), account_name_set(), account_name_set());
    } FC_LOG_AND_RETHROW() }

    // tested in change_recovery_account test cases
    // BOOST_AUTO_TEST_CASE(change_recovery_account_apply) { try {
    // } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_SUITE_END() // change_recovery_account


    BOOST_AUTO_TEST_CASE(account_recovery) {
        try {
            BOOST_TEST_MESSAGE("Testing: account recovery");
            ACTORS((alice));
            fund("alice", 1000000);

            BOOST_TEST_MESSAGE("--- Creating account bob with alice");
            account_create_operation acc_create;
            acc_create.fee = ASSET("10.000 GOLOS");
            acc_create.creator = "alice";
            acc_create.new_account_name = "bob";
            acc_create.owner = authority(1, generate_private_key("bob_owner").get_public_key(), 1);
            acc_create.active = authority(1, generate_private_key("bob_active").get_public_key(), 1);
            acc_create.posting = authority(1, generate_private_key("bob_posting").get_public_key(), 1);
            acc_create.memo_key = generate_private_key("bob_memo").get_public_key();
            acc_create.json_metadata = "";

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, acc_create));
            const auto& bob_auth = db->get<account_authority_object, by_account>("bob");
            BOOST_CHECK_EQUAL(bob_auth.owner, acc_create.owner);

            BOOST_TEST_MESSAGE("--- Changing bob's owner authority");
            account_update_operation acc_update;
            acc_update.account = "bob";
            acc_update.owner = authority(1, generate_private_key("bad_key").get_public_key(), 1);
            acc_update.memo_key = acc_create.memo_key;
            acc_update.json_metadata = "";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, generate_private_key("bob_owner"), acc_update));
            BOOST_CHECK_EQUAL(bob_auth.owner, *acc_update.owner);

            BOOST_TEST_MESSAGE("--- Creating recover request for bob with alice");
            request_account_recovery_operation request;
            request.recovery_account = "alice";
            request.account_to_recover = "bob";
            request.new_owner_authority = authority(1, generate_private_key("new_key").get_public_key(), 1);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, request));
            BOOST_CHECK_EQUAL(bob_auth.owner, *acc_update.owner);

            BOOST_TEST_MESSAGE("--- Recovering bob's account with original owner auth and new secret");
            generate_blocks(db->head_block_time() + STEEMIT_OWNER_UPDATE_LIMIT);
            recover_account_operation recover;
            recover.account_to_recover = "bob";
            recover.new_owner_authority = request.new_owner_authority;
            recover.recent_owner_authority = acc_create.owner;

            tx.clear();
            tx.operations.push_back(recover);
            tx.sign(generate_private_key("bob_owner"), db->get_chain_id());
            tx.sign(generate_private_key("new_key"), db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const auto& owner1 = db->get<account_authority_object, by_account>("bob").owner;
            BOOST_CHECK_EQUAL(owner1, recover.new_owner_authority);

            BOOST_TEST_MESSAGE("--- Creating new recover request for a bogus key");
            request.new_owner_authority = authority(1, generate_private_key("foo bar").get_public_key(), 1);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, request));

            BOOST_TEST_MESSAGE("--- Testing failure when bob does not have new authority");
            generate_blocks(db->head_block_time() + STEEMIT_OWNER_UPDATE_LIMIT + fc::seconds(STEEMIT_BLOCK_INTERVAL));
            recover.new_owner_authority = authority(1, generate_private_key("idontknow").get_public_key(), 1);

            tx.clear();
            tx.operations.push_back(recover);
            tx.sign(generate_private_key("bob_owner"), db->get_chain_id());
            tx.sign(generate_private_key("idontknow"), db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::authority_does_not_match_request)));

            const auto& owner2 = db->get<account_authority_object, by_account>("bob").owner;
            BOOST_CHECK_EQUAL(owner2, authority(1, generate_private_key("new_key").get_public_key(), 1));

            BOOST_TEST_MESSAGE("--- Testing failure when bob does not have old authority");
            recover.recent_owner_authority = authority(1, generate_private_key("idontknow").get_public_key(), 1);
            recover.new_owner_authority = authority(1, generate_private_key("foo bar").get_public_key(), 1);

            tx.clear();
            tx.operations.push_back(recover);
            tx.sign(generate_private_key("foo bar"), db->get_chain_id());
            tx.sign(generate_private_key("idontknow"), db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::no_recent_authority_in_history)));

            const auto& owner3 = db->get<account_authority_object, by_account>("bob").owner;
            BOOST_CHECK_EQUAL(owner3, authority(1, generate_private_key("new_key").get_public_key(), 1));

            BOOST_TEST_MESSAGE("--- Testing using the same old owner auth again for recovery");
            recover.recent_owner_authority = authority(1, generate_private_key("bob_owner").get_public_key(), 1);
            recover.new_owner_authority = authority(1, generate_private_key("foo bar").get_public_key(), 1);

            tx.clear();
            tx.operations.push_back(recover);
            tx.sign(generate_private_key("bob_owner"), db->get_chain_id());
            tx.sign(generate_private_key("foo bar"), db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const auto& owner4 = db->get<account_authority_object, by_account>("bob").owner;
            BOOST_CHECK_EQUAL(owner4, recover.new_owner_authority);

            BOOST_TEST_MESSAGE("--- Creating a recovery request that will expire");
            request.new_owner_authority = authority(1, generate_private_key("expire").get_public_key(), 1);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, request));

            const auto& request_idx = db->get_index<account_recovery_request_index>().indices();
            auto req_itr = request_idx.begin();
            BOOST_CHECK_EQUAL(req_itr->account_to_recover, "bob");
            BOOST_CHECK_EQUAL(req_itr->new_owner_authority,
                authority(1, generate_private_key("expire").get_public_key(), 1));
            BOOST_CHECK_EQUAL(req_itr->expires,
                db->head_block_time() + STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD);
            auto expires = req_itr->expires;
            ++req_itr;
            BOOST_CHECK(req_itr == request_idx.end());

            generate_blocks(time_point_sec(expires - STEEMIT_BLOCK_INTERVAL), true);
            const auto& new_request_idx = db->get_index<account_recovery_request_index>().indices();
            BOOST_CHECK(new_request_idx.begin() != new_request_idx.end());
            generate_block();
            BOOST_CHECK(new_request_idx.begin() == new_request_idx.end());

            recover.new_owner_authority = authority(1, generate_private_key("expire").get_public_key(), 1);
            recover.recent_owner_authority = authority(1, generate_private_key("bob_owner").get_public_key(), 1);
            tx.clear();
            tx.operations.push_back(recover);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(generate_private_key("expire"), db->get_chain_id());
            tx.sign(generate_private_key("bob_owner"), db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::no_active_recovery_request)));

            const auto& owner5 = db->get<account_authority_object, by_account>("bob").owner;
            BOOST_CHECK_EQUAL(owner5, authority(1, generate_private_key("foo bar").get_public_key(), 1));

            BOOST_TEST_MESSAGE("--- Expiring owner authority history");
            acc_update.owner = authority(1, generate_private_key("new_key").get_public_key(), 1);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, generate_private_key("foo bar"), acc_update));

            generate_blocks(db->head_block_time() +
                (STEEMIT_OWNER_AUTH_RECOVERY_PERIOD - STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD));
            generate_block();
            request.new_owner_authority = authority(1, generate_private_key("last key").get_public_key(), 1);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, request));

            recover.new_owner_authority = request.new_owner_authority;
            recover.recent_owner_authority = authority(1, generate_private_key("bob_owner").get_public_key(), 1);
            tx.clear();
            tx.operations.push_back(recover);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(generate_private_key("bob_owner"), db->get_chain_id());
            tx.sign(generate_private_key("last key"), db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::no_recent_authority_in_history)));

            const auto& owner6 = db->get<account_authority_object, by_account>("bob").owner;
            BOOST_CHECK_EQUAL(owner6, authority(1, generate_private_key("new_key").get_public_key(), 1));

            recover.recent_owner_authority = authority(1, generate_private_key("foo bar").get_public_key(), 1);
            tx.clear();
            tx.operations.push_back(recover);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(generate_private_key("foo bar"), db->get_chain_id());
            tx.sign(generate_private_key("last key"), db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            const auto& owner7 = db->get<account_authority_object, by_account>("bob").owner;
            BOOST_CHECK_EQUAL(owner7, authority(1, generate_private_key("last key").get_public_key(), 1));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(vesting_withdraw_reset_on_recovery) {
        try {
            BOOST_TEST_MESSAGE("Testing: vesting withdraw reset on account recover");
            ACTORS((alice));
            fund("alice", 1000000);

            account_create_operation acc_create;

            BOOST_TEST_MESSAGE("--- Creating account bob with alice");

            const auto bob_private_key = generate_private_key("bob_active");
            const auto bob_owner_authority = authority(1, generate_private_key("bob_owner").get_public_key(), 1);

            acc_create.fee = ASSET("10.000 GOLOS");
            acc_create.creator = "alice";
            acc_create.new_account_name = "bob";
            acc_create.owner = bob_owner_authority;
            acc_create.active = authority(1, bob_private_key.get_public_key(), 1);
            acc_create.posting = authority(1, generate_private_key("bob_posting").get_public_key(), 1);
            acc_create.memo_key = generate_private_key("bob_memo").get_public_key();
            acc_create.json_metadata = "";

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, acc_create));

            tx.clear();

            BOOST_TEST_MESSAGE("--- Creating account chewy with alice");
            acc_create.fee = ASSET("0.000 GOLOS");
            acc_create.creator = "alice";
            acc_create.new_account_name = "chewy";
            acc_create.owner = authority(1, generate_private_key("chewy_owner").get_public_key(), 1);
            acc_create.active = authority(1, generate_private_key("chewy_active").get_public_key(), 1);
            acc_create.posting = authority(1, generate_private_key("chewy_posting").get_public_key(), 1);
            acc_create.memo_key = generate_private_key("chewy_memo").get_public_key();
            acc_create.json_metadata = "";

            tx.operations.push_back(acc_create);

            BOOST_TEST_MESSAGE("--- Creating account donna with alice");
            acc_create.fee = ASSET("0.000 GOLOS");
            acc_create.creator = "alice";
            acc_create.new_account_name = "donna";
            acc_create.owner = authority(1, generate_private_key("donna_owner").get_public_key(), 1);
            acc_create.active = authority(1, generate_private_key("donna_active").get_public_key(), 1);
            acc_create.posting = authority(1, generate_private_key("donna_posting").get_public_key(), 1);
            acc_create.memo_key = generate_private_key("donna_memo").get_public_key();
            acc_create.json_metadata = "";

            tx.operations.push_back(acc_create);
            tx.sign(alice_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Set vesting withdraw routes");

            set_withdraw_vesting_route_operation to_chewy;
            to_chewy.from_account = "bob";
            to_chewy.to_account = "chewy";
            to_chewy.percent = 50 * STEEMIT_1_PERCENT;

            set_withdraw_vesting_route_operation to_donna;

            to_donna.from_account = "bob";
            to_donna.to_account = "donna";
            to_donna.percent = 50 * STEEMIT_1_PERCENT;

            tx.clear();

            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, to_chewy, to_donna));

            BOOST_TEST_MESSAGE("--- Start vesting withdraw");

            withdraw_vesting_operation withdraw_vesting_op;
            withdraw_vesting_op.account = "bob";
            withdraw_vesting_op.vesting_shares = asset(db->get_account("bob").vesting_shares.amount / 2, VESTS_SYMBOL);

            tx.clear();
            tx.operations.push_back(withdraw_vesting_op);

            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            tx.sign(bob_private_key, db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- Generating block up to first withdrawal");

            generate_blocks(db->head_block_time() + STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS, true);

            {
                const auto& bob = db->get_account("bob");

                BOOST_CHECK_NE(bob.vesting_withdraw_rate.amount.value, 0);
                BOOST_CHECK_NE(bob.to_withdraw.value, 0);
                BOOST_CHECK_NE(bob.withdrawn, 0);
                BOOST_CHECK_NE(bob.next_vesting_withdrawal, fc::time_point_sec::maximum());
            }

            BOOST_TEST_MESSAGE("--- Changing bob's owner authority");

            account_update_operation acc_update;

            acc_update.account = "bob";
            acc_update.owner = authority(1, generate_private_key("bad_key").get_public_key(), 1);
            acc_update.memo_key = acc_create.memo_key;
            acc_update.json_metadata = "";

            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, generate_private_key("bob_owner"), acc_update));

            BOOST_TEST_MESSAGE("--- Creating recover request for bob with alice");

            request_account_recovery_operation request;
            request.recovery_account = "alice";
            request.account_to_recover = "bob";
            request.new_owner_authority = authority(1, generate_private_key("new_key").get_public_key(), 1);

            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, request));

            BOOST_TEST_MESSAGE("--- Recovering bob's account with original owner auth and new secret");

            recover_account_operation recover;
            recover.account_to_recover = "bob";
            recover.new_owner_authority = request.new_owner_authority;
            recover.recent_owner_authority = bob_owner_authority;

            tx.clear();
            tx.operations.push_back(recover);

            tx.sign(generate_private_key("bob_owner"), db->get_chain_id());
            tx.sign(generate_private_key("new_key"), db->get_chain_id());

            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            generate_blocks(db->head_block_time() + STEEMIT_OWNER_UPDATE_LIMIT);

            BOOST_TEST_MESSAGE("--- Verify chewy and donna are not vesting receivers.");

            {
                const auto& bob = db->get_account("bob");
                const auto& chewy = db->get_account("chewy");
                const auto& donna = db->get_account("donna");

                const auto& withdraw_index = db->get_index<withdraw_vesting_route_index>().indices().get<by_withdraw_route>();
                auto it = withdraw_index.upper_bound(bob.id);

                while (it != withdraw_index.end() && it->from_account == bob.id) {
                    BOOST_CHECK_NE(it->to_account, chewy.id);
                    BOOST_CHECK_NE(it->to_account, donna.id);
                }
            }

            BOOST_TEST_MESSAGE("--- Verify withdraw is stopped.");

            {
                const auto& bob = db->get_account("bob");

                BOOST_CHECK_EQUAL(bob.vesting_withdraw_rate.amount.value, 0);
                BOOST_CHECK_EQUAL(bob.to_withdraw.value, 0);
                BOOST_CHECK_EQUAL(bob.withdrawn, 0);
                BOOST_CHECK_EQUAL(bob.next_vesting_withdrawal, fc::time_point_sec::maximum());
            }
         }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(change_recovery_account_apply) {
        try {
            using fc::ecc::private_key;
            BOOST_TEST_MESSAGE("Testing change_recovery_account_operation");
            ACTORS((alice)(sam))

            auto change_recovery_account = [&](const string& account_to_recover, const string& new_recovery_account) {
                change_recovery_account_operation op;
                op.account_to_recover = account_to_recover;
                op.new_recovery_account = new_recovery_account;
                signed_transaction tx;
                push_tx_with_ops(tx, alice_private_key, op);
            };

            auto recover_account = [&](
                const string& account_to_recover,
                const private_key& new_owner_key,
                const private_key& recent_owner_key
            ) {
                recover_account_operation op;
                op.account_to_recover = account_to_recover;
                op.new_owner_authority = authority(1, public_key_type(new_owner_key.get_public_key()), 1);
                op.recent_owner_authority = authority(1, public_key_type(recent_owner_key.get_public_key()), 1);
                signed_transaction tx;
                tx.operations.push_back(op);
                tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                tx.sign(recent_owner_key, db->get_chain_id());
                // only Alice -> throw
                GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0), CHECK_ERROR(tx_missing_other_auth, 0));
                tx.signatures.clear();
                tx.sign(new_owner_key, db->get_chain_id());
                // only Sam -> throw
                GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx, 0), CHECK_ERROR(tx_missing_other_auth, 0));
                tx.sign(recent_owner_key, db->get_chain_id());
                // Alice+Sam -> OK
                db->push_transaction(tx, 0);
            };

            auto request_account_recovery = [&](
                const string& recovery_account,
                const private_key& recovery_account_key,
                const string& account_to_recover,
                const public_key_type& new_owner_key
            ) {
                request_account_recovery_operation op;
                op.recovery_account = recovery_account;
                op.account_to_recover = account_to_recover;
                op.new_owner_authority = authority(1, new_owner_key, 1);
                signed_transaction tx;
                push_tx_with_ops(tx, recovery_account_key, op);
            };

            auto change_owner = [&](
                const string& account,
                const private_key& old_private_key,
                const public_key_type& new_public_key
            ) {
                account_update_operation op;
                op.account = account;
                op.owner = authority(1, new_public_key, 1);
                signed_transaction tx;
                BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, old_private_key, op));
            };

            BOOST_TEST_MESSAGE("--- if either/both users do not exist, we shouldn't allow it");
            GOLOS_CHECK_ERROR_PROPS(change_recovery_account("alice", "nobody"),
                CHECK_ERROR(tx_invalid_operation, 0, CHECK_ERROR(missing_object, "account", "nobody")));
            GOLOS_CHECK_ERROR_PROPS(change_recovery_account("haxer", "sam"),
                CHECK_ERROR(missing_object, "authority", "haxer"));
            GOLOS_CHECK_ERROR_PROPS(change_recovery_account("haxer", "nobody"),
                CHECK_ERROR(missing_object, "authority", "haxer"));
            BOOST_CHECK_NO_THROW(change_recovery_account("alice", "sam"));

            fc::ecc::private_key alice_priv1 = fc::ecc::private_key::regenerate(fc::sha256::hash("alice_k1"));
            fc::ecc::private_key alice_priv2 = fc::ecc::private_key::regenerate(fc::sha256::hash("alice_k2"));
            public_key_type alice_pub1 = public_key_type(alice_priv1.get_public_key());

            generate_blocks(
                db->head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD - fc::seconds(STEEMIT_BLOCK_INTERVAL),
                true);

            BOOST_TEST_MESSAGE("--- cannot request account recovery until recovery account is approved");
            GOLOS_CHECK_ERROR_PROPS(request_account_recovery("sam", sam_private_key, "alice", alice_pub1),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::cannot_recover_if_not_partner)));
            generate_blocks(1);

            BOOST_TEST_MESSAGE("--- cannot finish account recovery until requested");
            GOLOS_CHECK_ERROR_PROPS(recover_account("alice", alice_priv1, alice_private_key),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::no_active_recovery_request)));
            // do the request
            BOOST_CHECK_NO_THROW(request_account_recovery("sam", sam_private_key, "alice", alice_pub1));

            BOOST_TEST_MESSAGE("--- can't recover with the current owner key");
            GOLOS_CHECK_ERROR_PROPS(recover_account("alice", alice_priv1, alice_private_key),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::no_recent_authority_in_history)));

            BOOST_TEST_MESSAGE("--- success after we change it!");
            BOOST_CHECK_NO_THROW(change_owner("alice", alice_private_key, public_key_type(alice_priv2.get_public_key())));
            BOOST_CHECK_NO_THROW(recover_account("alice", alice_priv1, alice_private_key));
        }
        FC_LOG_AND_RETHROW()
    }
    BOOST_AUTO_TEST_SUITE_END() // recovery


//#define CALCULATE_NONCES

    BOOST_AUTO_TEST_CASE(pow2_op) {
        return; // FIXME: broken test
        try {
            uint32_t target = db->get_pow_summary_target();
            BOOST_REQUIRE(target == 0xfe000000);

            pow2_operation pow;
            equihash_pow work;

            auto alice_private_key = generate_private_key("alice");
            auto alice_public_key = alice_private_key.get_public_key();

            auto old_block_id = db->head_block_id();

#ifdef CALCULATE_NONCES
            uint64_t nonce = 0;
      do
      {
         nonce++;
         work.create( db->head_block_id(), "alice", nonce );
         idump( (work.proof.is_valid())(work.pow_summary)(target) );
      } while( !work.proof.is_valid() || work.pow_summary >= target );
      uint64_t nonce1 = nonce;
      idump( (nonce1) );
#else
            //uint64_t nonce1 = 98;
            //uint64_t nonce1 = 79;
#endif

            generate_block();

#ifdef CALCULATE_NONCES
      do
      {
         nonce++;
         work.create( db->head_block_id(), "alice", nonce );
         idump( (work.proof.is_valid())(work.pow_summary)(target) );
      } while( !work.proof.is_valid() || work.pow_summary < target );
      uint64_t nonce2 = nonce;
      idump( (nonce2) );
#else
            uint64_t nonce2 = 36;
#endif

#ifdef CALCULATE_NONCES
      do
      {
         nonce++;
         work.create( db->head_block_id(), "alice", nonce );
         idump( (work.proof.is_valid())(work.pow_summary)(target) );
      } while( !work.proof.is_valid() || work.pow_summary >= target );
      uint64_t nonce3 = nonce;
      idump( (nonce3) );
#else
            uint64_t nonce3 = 357;
#endif

#ifdef CALCULATE_NONCES
      do
      {
         nonce++;
         work.create( db->head_block_id(), "alice", nonce );
         idump( (work.proof.is_valid())(work.pow_summary)(target) );
      } while( !work.proof.is_valid() || work.pow_summary >= target );
      uint64_t nonce4 = nonce;
      idump( (nonce4) );
#else
            uint64_t nonce4 = 394;
#endif

            // Test with nonce that doesn't match work, should fail
            BOOST_TEST_MESSAGE("Testing pow with nonce that doesn't match work");
            work.create(db->head_block_id(), "alice", nonce3);
            work.input.nonce = nonce4;
            work.prev_block = db->head_block_id();
            pow.work = work;
            STEEMIT_REQUIRE_THROW(pow.validate(), fc::exception);

            BOOST_TEST_MESSAGE("Testing failure on insufficient work");
            signed_transaction tx;
            work.create(db->head_block_id(), "alice", nonce2);
            work.prev_block = db->head_block_id();
            pow.work = work;
            pow.new_owner_key = alice_public_key;
            tx.clear();
            tx.operations.push_back(pow);
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());
            pow.validate();
            STEEMIT_REQUIRE_THROW(db->push_transaction(tx, 0), fc::exception);

            // Test without owner key, should fail on new account
            BOOST_TEST_MESSAGE("Submit pow without a new owner key");

            tx.operations.clear();
            tx.signatures.clear();

            work.create(db->head_block_id(), "alice", nonce3);
            work.prev_block = db->head_block_id();
            pow.work = work;
            pow.new_owner_key.reset();
            tx.operations.push_back(pow);
            pow.validate();
            STEEMIT_REQUIRE_THROW(db->push_transaction(tx, 0), fc::exception);

            tx.sign(alice_private_key, db->get_chain_id());
            STEEMIT_REQUIRE_THROW(db->push_transaction(tx, 0), fc::exception);

            // Test success when adding owner key
            BOOST_TEST_MESSAGE("Testing success");
            tx.operations.clear();
            tx.signatures.clear();
            work.create(db->head_block_id(), "alice", nonce3);
            work.prev_block = db->head_block_id();
            pow.work = work;
            pow.new_owner_key = alice_public_key;
            tx.operations.push_back(pow);
            tx.sign(alice_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);

            const auto &alice = db->get_account("alice");
            const auto &alice_auth_obj = db->get<account_authority_object, by_account>("alice");
            authority alice_auth(1, alice_public_key, 1);
            BOOST_REQUIRE(alice_auth_obj.owner == alice_auth);
            BOOST_REQUIRE(alice_auth_obj.active == alice_auth);
            BOOST_REQUIRE(alice_auth_obj.posting == alice_auth);
            BOOST_REQUIRE(alice.memo_key == alice_public_key);

            const auto &alice_witness = db->get_witness("alice");
            BOOST_REQUIRE(alice_witness.pow_worker == 0);

            // Test failure when account is in queue
            BOOST_TEST_MESSAGE("Test failure when account is already in queue");
            tx.operations.clear();
            tx.signatures.clear();
            work.prev_block = db->head_block_id();
            pow.work = work;

            tx.operations.push_back(pow);
            tx.sign(alice_private_key, db->get_chain_id());
            STEEMIT_REQUIRE_THROW(db->push_transaction(tx, 0), fc::exception);

            generate_block();
            STEEMIT_REQUIRE_THROW(db->push_transaction(tx, 0), fc::exception);

            ACTORS((bob))
            generate_block();

            target = db->get_pow_summary_target();

#ifdef CALCULATE_NONCES
            nonce = nonce4;
      do
      {
         nonce++;
         work.create( db->head_block_id(), "bob", nonce );
         idump( (work.proof.is_valid())(work.pow_summary)(target) );
      } while( !work.proof.is_valid() || work.pow_summary >= target );
      uint64_t nonce5 = nonce;
      idump( (nonce5) );
#else
            uint32_t nonce5 = 364;
#endif

            BOOST_TEST_MESSAGE("Submit pow from existing account without witness object.");

            tx.operations.clear();
            tx.signatures.clear();

            work.create(db->head_block_id(), "bob", nonce5);
            work.prev_block = db->head_block_id();
            pow.work = work;
            pow.new_owner_key.reset();
            tx.operations.push_back(pow);
            tx.sign(bob_private_key, db->get_chain_id());
            pow.validate();
            STEEMIT_REQUIRE_THROW(db->push_transaction(tx, 0), fc::exception);


            BOOST_TEST_MESSAGE("Submit pow from existing account with witness object.");

            witness_create("bob", bob_private_key, "bob.com", bob_private_key.get_public_key(), 0);
            pow.validate();
            db->push_transaction(tx, 0);

            const auto &bob_witness = db->get_witness("bob");
            BOOST_REQUIRE(bob_witness.pow_worker == 1);

            auto sam_private_key = generate_private_key("sam");
            auto sam_public_key = sam_private_key.get_public_key();
            auto dave_private_key = generate_private_key("dave");
            auto dave_public_key = dave_private_key.get_public_key();

            target = db->get_pow_summary_target();

#ifdef CALCULATE_NONCES
      do
      {
         nonce++;
         work.create( old_block_id, "sam", nonce );
         idump( (work.proof.is_valid())(work.pow_summary)(target) );
      } while( !work.proof.is_valid() || work.pow_summary >= target );
      uint64_t nonce6 = nonce;
      idump( (nonce6) );
#else
            uint64_t nonce6 = 404;
#endif

#ifdef CALCULATE_NONCES
      do
      {
         nonce++;
         work.create( old_block_id, "dave", nonce );
         idump( (work.proof.is_valid())(work.pow_summary)(target) );
      } while( !work.proof.is_valid() || work.pow_summary >= target );
      uint64_t nonce7 = nonce;
      idump( (nonce7) );
#else
            uint64_t nonce7 = 496;
#endif

            // Test with wrong previous block id
            BOOST_TEST_MESSAGE("Submit pow with an old block id");
            tx.clear();
            work.create(old_block_id, "sam", nonce6);
            work.prev_block = db->head_block_id();
            pow.work = work;
            pow.new_owner_key = sam_public_key;
            tx.operations.push_back(pow);
            tx.set_expiration(
                    db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(sam_private_key, db->get_chain_id());
            pow.validate();
            db->push_transaction(tx, 0);


            BOOST_TEST_MESSAGE("Test failure when block hashed on is past the last irreversible block threshold");

            generate_blocks(100);

            tx.operations.clear();
            tx.signatures.clear();

            work.create(old_block_id, "dave", nonce7);
            work.prev_block = db->head_block_id();
            pow.work = work;
            pow.new_owner_key = dave_public_key;
            tx.operations.push_back(pow);
            tx.sign(dave_private_key, db->get_chain_id());
            pow.validate();
            STEEMIT_REQUIRE_THROW(db->push_transaction(tx, 0), fc::exception);
        }
        FC_LOG_AND_RETHROW()
    }


    BOOST_AUTO_TEST_SUITE(prove_authority)
    // Technically it can be called, but will fail early due disabled challenge_authority_operation
    BOOST_AUTO_TEST_CASE(prove_authority_validate) { try {
        BOOST_TEST_MESSAGE("Testing: prove_authority_validate");
        prove_authority_operation op;
        op.challenged = "bob";
        CHECK_OP_VALID(op);
        CHECK_PARAM_INVALID(op, challenged, "");
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(prove_authority_authorities) { try {
        BOOST_TEST_MESSAGE("Testing: prove_authority_authorities");
        prove_authority_operation op;
        op.challenged = "bob";
        op.require_owner = true;
        CHECK_OP_AUTHS(op, account_name_set({"bob"}), account_name_set(), account_name_set());
        op.require_owner = false;
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"bob"}), account_name_set());
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(prove_authority_apply) { try {
        BOOST_TEST_MESSAGE("Testing: prove_authority_apply");
        ACTOR(bob)
        prove_authority_operation op;
        op.challenged = "bob";
        signed_transaction tx;
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(logic_exception, logic_exception::account_is_not_challeneged)));
        validate_database();
    } FC_LOG_AND_RETHROW() }
    BOOST_AUTO_TEST_SUITE_END() // prove_authority


//-------------------------------------------------------------
    BOOST_AUTO_TEST_SUITE(escrow)

    BOOST_AUTO_TEST_SUITE(escrow_transfer)

    BOOST_AUTO_TEST_CASE(escrow_transfer_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_transfer_validate");

            escrow_transfer_operation op;
            op.from = "alice";
            op.to = "bob";
            op.sbd_amount = ASSET("1.000 GBG");
            op.steem_amount = ASSET("1.000 GOLOS");
            op.escrow_id = 0;
            op.agent = "sam";
            op.fee = ASSET("0.100 GOLOS");
            op.json_meta = "";
            op.ratification_deadline = db->head_block_time() + 100;
            op.escrow_expiration = db->head_block_time() + 200;
            BOOST_TEST_MESSAGE("--- success on valid parameters");
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, sbd_amount, ASSET_GBG(0));
            CHECK_PARAM_VALID(op, steem_amount, ASSET_GOLOS(0));
            CHECK_PARAM_VALID(op, fee, ASSET_GOLOS(0));
            CHECK_PARAM_VALID(op, fee, ASSET_GBG(0));
            CHECK_PARAM_VALID(op, fee, ASSET_GBG(1));

            BOOST_TEST_MESSAGE("--- failure when asset symbols invalid");
            CHECK_PARAM_INVALID(op, sbd_amount, ASSET_GOLOS(1));
            CHECK_PARAM_INVALID(op, sbd_amount, ASSET_GESTS(1));
            CHECK_PARAM_INVALID(op, sbd_amount, ASSET("1.00 BAD"));
            CHECK_PARAM_INVALID(op, steem_amount, ASSET_GBG(1));
            CHECK_PARAM_INVALID(op, steem_amount, ASSET_GESTS(1));
            CHECK_PARAM_INVALID(op, steem_amount, ASSET("1.00 BAD"));
            CHECK_PARAM_INVALID(op, fee, ASSET_GESTS(1));
            CHECK_PARAM_INVALID(op, fee, ASSET("1.00 BAD"));

            BOOST_TEST_MESSAGE("--- failure when sbd == 0 and steem == 0");
            op.sbd_amount.amount = 0;
            CHECK_PARAM_INVALID_LOGIC(op, steem_amount, ASSET_GOLOS(0), escrow_no_amount_set);

            BOOST_TEST_MESSAGE("--- failure when asset amount < 0");
            op.sbd_amount.amount = 0;
            CHECK_PARAM_INVALID(op, fee, ASSET_GBG(-1));
            CHECK_PARAM_INVALID(op, fee, ASSET_GOLOS(-1));
            CHECK_PARAM_INVALID(op, sbd_amount, ASSET_GBG(-1));
            CHECK_PARAM_INVALID(op, steem_amount, ASSET_GOLOS(-1));

            BOOST_TEST_MESSAGE("--- failure when ratification deadline == escrow expiration");
            CHECK_PARAM_INVALID_LOGIC(op, ratification_deadline, op.escrow_expiration, escrow_wrong_time_limits);

            BOOST_TEST_MESSAGE("--- failure when ratification deadline > escrow expiration");
            CHECK_PARAM_INVALID_LOGIC(op, ratification_deadline, op.escrow_expiration + 1, escrow_wrong_time_limits);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_transfer_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_transfer_authorities");
            escrow_transfer_operation op;
            op.from = "alice";
            op.to = "bob";
            op.sbd_amount = ASSET("1.000 GBG");
            op.steem_amount = ASSET("1.000 GOLOS");
            op.escrow_id = 0;
            op.agent = "sam";
            op.fee = ASSET("0.100 GOLOS");
            op.json_meta = "";
            op.ratification_deadline = db->head_block_time() + 100;
            op.escrow_expiration = db->head_block_time() + 200;
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"alice"}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_transfer_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_transfer_apply");
            ACTORS((alice)(bob)(sam))
            fund("alice", 10000);

            escrow_transfer_operation op;
            op.from = "alice";
            op.to = "bob";
            op.sbd_amount = ASSET("1.000 GBG");
            op.steem_amount = ASSET("1.000 GOLOS");
            op.escrow_id = 0;
            op.agent = "sam";
            op.fee = ASSET("0.100 GOLOS");
            op.json_meta = "";
            op.ratification_deadline = db->head_block_time() + 100;
            op.escrow_expiration = db->head_block_time() + 200;

            BOOST_TEST_MESSAGE("--- failure when from cannot cover sbd amount");
            signed_transaction tx;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "alice", "fund", "1.000 GBG")));

            BOOST_TEST_MESSAGE("--- falure when from cannot cover amount + fee");
            op.sbd_amount.amount = 0;
            op.steem_amount.amount = 10000;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "alice", "fund", "10.100 GOLOS")));

            BOOST_TEST_MESSAGE("--- failure when ratification deadline is in the past");
            op.steem_amount.amount = 1000;
            op.ratification_deadline = db->head_block_time() - 200;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_time_in_past)));

            BOOST_TEST_MESSAGE("--- failure when expiration is in the past");
            op.escrow_expiration = db->head_block_time() - 100;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_time_in_past)));

            BOOST_TEST_MESSAGE("--- success");
            op.ratification_deadline = db->head_block_time() + 100;
            op.escrow_expiration = db->head_block_time() + 200;
            auto alice_steem_balance = alice.balance - op.steem_amount - op.fee;
            auto alice_sbd_balance = alice.sbd_balance - op.sbd_amount;
            auto bob_steem_balance = bob.balance;
            auto bob_sbd_balance = bob.sbd_balance;
            auto sam_steem_balance = sam.balance;
            auto sam_sbd_balance = sam.sbd_balance;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            const auto& escrow = db->get_escrow(op.from, op.escrow_id);
            BOOST_CHECK_EQUAL(escrow.escrow_id, op.escrow_id);
            BOOST_CHECK_EQUAL(escrow.from, op.from);
            BOOST_CHECK_EQUAL(escrow.to, op.to);
            BOOST_CHECK_EQUAL(escrow.agent, op.agent);
            BOOST_CHECK_EQUAL(escrow.ratification_deadline, op.ratification_deadline);
            BOOST_CHECK_EQUAL(escrow.escrow_expiration, op.escrow_expiration);
            BOOST_CHECK_EQUAL(escrow.sbd_balance, op.sbd_amount);
            BOOST_CHECK_EQUAL(escrow.steem_balance, op.steem_amount);
            BOOST_CHECK_EQUAL(escrow.pending_fee, op.fee);
            BOOST_CHECK(!escrow.to_approved);
            BOOST_CHECK(!escrow.agent_approved);
            BOOST_CHECK(!escrow.disputed);
            BOOST_CHECK_EQUAL(alice.balance, alice_steem_balance);
            BOOST_CHECK_EQUAL(alice.sbd_balance, alice_sbd_balance);
            BOOST_CHECK_EQUAL(bob.balance, bob_steem_balance);
            BOOST_CHECK_EQUAL(bob.sbd_balance, bob_sbd_balance);
            BOOST_CHECK_EQUAL(sam.balance, sam_steem_balance);
            BOOST_CHECK_EQUAL(sam.sbd_balance, sam_sbd_balance);

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }
    BOOST_AUTO_TEST_SUITE_END() // escrow_transfer


    BOOST_AUTO_TEST_SUITE(escrow_approve)

    BOOST_AUTO_TEST_CASE(escrow_approve_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_approve_validate");
            escrow_approve_operation op;
            op.from = "alice";
            op.to = "bob";
            op.agent = "sam";
            op.who = "bob";
            op.escrow_id = 0;
            op.approve = true;
            BOOST_TEST_MESSAGE("--- success when 'who' is 'to'");
            CHECK_PARAM_VALID(op, who, op.to);

            BOOST_TEST_MESSAGE("--- success when 'who' is 'agent'");
            CHECK_PARAM_VALID(op, who, op.agent);

            BOOST_TEST_MESSAGE("--- failure when 'who' is not 'to' or 'agent'");
            CHECK_PARAM_INVALID(op, who, "dave");

            BOOST_TEST_MESSAGE("--- failure when invalid account");
            CHECK_PARAM_INVALID(op, from, "");
            CHECK_PARAM_INVALID(op, to, "");
            CHECK_PARAM_INVALID(op, agent, "");
            CHECK_PARAM_INVALID(op, who, "");
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_approve_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_approve_authorities");
            escrow_approve_operation op;
            op.from = "alice";
            op.to = "bob";
            op.agent = "sam";
            op.who = "bob";
            op.escrow_id = 0;
            op.approve = true;
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({op.who}), account_name_set());
            op.who = "sam";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({op.who}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_approve_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_approve_apply");
            ACTORS((alice)(bob)(sam)(dave))
            fund("alice", 10000);

            escrow_transfer_operation et_op;
            et_op.from = "alice";
            et_op.to = "bob";
            et_op.agent = "sam";
            et_op.steem_amount = ASSET("1.000 GOLOS");
            et_op.fee = ASSET("0.100 GOLOS");
            et_op.json_meta = "";
            et_op.ratification_deadline = db->head_block_time() + 100;
            et_op.escrow_expiration = db->head_block_time() + 200;

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, et_op));

            BOOST_TEST_MESSAGE("---failure when to does not match escrow");
            escrow_approve_operation op;
            op.from = "alice";
            op.to = "dave";
            op.agent = "sam";
            op.who = "dave";
            op.approve = true;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, dave_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_bad_to)));

            BOOST_TEST_MESSAGE("--- failure when agent does not match escrow");
            op.to = "bob";
            op.agent = "dave";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, dave_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_bad_agent)));

            BOOST_TEST_MESSAGE("--- success approving to");
            op.agent = "sam";
            op.who = "bob";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

            {
                auto& escrow = db->get_escrow(op.from, op.escrow_id);
                BOOST_CHECK_EQUAL(escrow.to, "bob");
                BOOST_CHECK_EQUAL(escrow.agent, "sam");
                BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
                BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
                BOOST_CHECK_EQUAL(escrow.sbd_balance, ASSET("0.000 GBG"));
                BOOST_CHECK_EQUAL(escrow.steem_balance, ASSET("1.000 GOLOS"));
                BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.100 GOLOS"));
                BOOST_CHECK(escrow.to_approved);
                BOOST_CHECK(!escrow.agent_approved);
                BOOST_CHECK(!escrow.disputed);
            }

            BOOST_TEST_MESSAGE("--- failure on repeat approval");
            generate_block();       // avoid tx_duplicate_transaction
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::account_already_approved_escrow)));

            BOOST_TEST_MESSAGE("--- failure trying to repeal after approval");
            op.approve = false;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::account_already_approved_escrow)));

            {
                auto& escrow = db->get_escrow(op.from, op.escrow_id);
                BOOST_CHECK_EQUAL(escrow.to, "bob");
                BOOST_CHECK_EQUAL(escrow.agent, "sam");
                BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
                BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
                BOOST_CHECK_EQUAL(escrow.sbd_balance, ASSET("0.000 GBG"));
                BOOST_CHECK_EQUAL(escrow.steem_balance, ASSET("1.000 GOLOS"));
                BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.100 GOLOS"));
                BOOST_CHECK(escrow.to_approved);
                BOOST_CHECK(!escrow.agent_approved);
                BOOST_CHECK(!escrow.disputed);
            }

            BOOST_TEST_MESSAGE("--- success refunding from because of repeal");
            op.who = op.agent;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            GOLOS_CHECK_ERROR_PROPS(db->get_escrow(op.from, op.escrow_id),
                CHECK_ERROR(missing_object, "escrow", make_escrow_id(op.from, op.escrow_id)));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("10.000 GOLOS"));
            validate_database();

            BOOST_TEST_MESSAGE("--- test automatic refund when escrow is not ratified before deadline");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, et_op));

            generate_blocks(et_op.ratification_deadline + STEEMIT_BLOCK_INTERVAL, true);

            GOLOS_CHECK_ERROR_PROPS(db->get_escrow(op.from, op.escrow_id),
                CHECK_ERROR(missing_object, "escrow", make_escrow_id(op.from, op.escrow_id)));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("10.000 GOLOS"));
            validate_database();

            BOOST_TEST_MESSAGE("--- test ratification expiration when escrow is only approved by to");
            et_op.ratification_deadline = db->head_block_time() + 100;
            et_op.escrow_expiration = db->head_block_time() + 200;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, et_op));

            op.who = op.to;
            op.approve = true;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

            generate_blocks(et_op.ratification_deadline + STEEMIT_BLOCK_INTERVAL, true);

            GOLOS_CHECK_ERROR_PROPS(db->get_escrow(op.from, op.escrow_id),
                CHECK_ERROR(missing_object, "escrow", make_escrow_id(op.from, op.escrow_id)));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("10.000 GOLOS"));
            validate_database();

            BOOST_TEST_MESSAGE("--- test ratification expiration when escrow is only approved by agent");
            et_op.ratification_deadline = db->head_block_time() + 100;
            et_op.escrow_expiration = db->head_block_time() + 200;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, et_op));

            op.who = op.agent;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            generate_blocks(et_op.ratification_deadline + STEEMIT_BLOCK_INTERVAL, true);

            GOLOS_CHECK_ERROR_PROPS(db->get_escrow(op.from, op.escrow_id),
                CHECK_ERROR(missing_object, "escrow", make_escrow_id(op.from, op.escrow_id)));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("10.000 GOLOS"));
            validate_database();

            BOOST_TEST_MESSAGE("--- success approving escrow");
            et_op.ratification_deadline = db->head_block_time() + 100;
            et_op.escrow_expiration = db->head_block_time() + 200;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, et_op));

            op.who = op.to;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

            op.who = op.agent;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            {
                const auto& escrow = db->get_escrow(op.from, op.escrow_id);
                BOOST_CHECK_EQUAL(escrow.to, "bob");
                BOOST_CHECK_EQUAL(escrow.agent, "sam");
                BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
                BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
                BOOST_CHECK_EQUAL(escrow.sbd_balance, ASSET("0.000 GBG"));
                BOOST_CHECK_EQUAL(escrow.steem_balance, ASSET("1.000 GOLOS"));
                BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.000 GOLOS"));
                BOOST_CHECK(escrow.to_approved);
                BOOST_CHECK(escrow.agent_approved);
                BOOST_CHECK(!escrow.disputed);
            }

            BOOST_CHECK_EQUAL(db->get_account("sam").balance, et_op.fee);
            validate_database();

            BOOST_TEST_MESSAGE("--- ratification expiration does not remove an approved escrow");

            generate_blocks(et_op.ratification_deadline + STEEMIT_BLOCK_INTERVAL, true);
            {
                const auto& escrow = db->get_escrow(op.from, op.escrow_id);
                BOOST_CHECK_EQUAL(escrow.to, "bob");
                BOOST_CHECK_EQUAL(escrow.agent, "sam");
                BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
                BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
                BOOST_CHECK_EQUAL(escrow.sbd_balance, ASSET("0.000 GBG"));
                BOOST_CHECK_EQUAL(escrow.steem_balance, ASSET("1.000 GOLOS"));
                BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.000 GOLOS"));
                BOOST_CHECK(escrow.to_approved);
                BOOST_CHECK(escrow.agent_approved);
                BOOST_CHECK(!escrow.disputed);
            }

            BOOST_CHECK_EQUAL(db->get_account("sam").balance, et_op.fee);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }
    BOOST_AUTO_TEST_SUITE_END() // escrow_approve


    BOOST_AUTO_TEST_SUITE(escrow_dispute)

    BOOST_AUTO_TEST_CASE(escrow_dispute_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_dispute_validate");
            escrow_dispute_operation op;
            op.from = "alice";
            op.to = "bob";
            op.agent = "dave";
            op.who = "alice";

            BOOST_TEST_MESSAGE("--- success on valid params");
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, who, "bob");

            BOOST_TEST_MESSAGE("--- failure when who is not from or to");
            CHECK_PARAM_INVALID(op, who, "dave");
            CHECK_PARAM_INVALID(op, who, "sam");

            BOOST_TEST_MESSAGE("--- failure when account invalid");
            CHECK_PARAM_INVALID(op, from, "");
            CHECK_PARAM_INVALID(op, to, "");
            CHECK_PARAM_INVALID(op, agent, "");
            CHECK_PARAM_INVALID(op, who, "");
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_dispute_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_dispute_authorities");
            escrow_dispute_operation op;
            op.from = "alice";
            op.to = "bob";
            op.who = "alice";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({op.who}), account_name_set());
            op.who = "bob";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({op.who}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_dispute_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_dispute_apply");
            ACTORS((alice)(bob)(sam)(dave))
            fund("alice", 10000);

            escrow_transfer_operation et_op;
            et_op.from = "alice";
            et_op.to = "bob";
            et_op.agent = "sam";
            et_op.steem_amount = ASSET("1.000 GOLOS");
            et_op.fee = ASSET("0.100 GOLOS");
            et_op.ratification_deadline = db->head_block_time() + STEEMIT_BLOCK_INTERVAL;
            et_op.escrow_expiration = db->head_block_time() + 2 * STEEMIT_BLOCK_INTERVAL;

            escrow_approve_operation ea_b_op;
            ea_b_op.from = "alice";
            ea_b_op.to = "bob";
            ea_b_op.agent = "sam";
            ea_b_op.who = "bob";
            ea_b_op.approve = true;

            signed_transaction tx;
            sign_tx_with_ops(tx, alice_private_key, et_op, ea_b_op);
            tx.sign(bob_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- failure when escrow has not been approved");
            escrow_dispute_operation op;
            op.from = "alice";
            op.to = "bob";
            op.agent = "sam";
            op.who = "bob";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_must_be_approved_first)));

            const auto& escrow = db->get_escrow(et_op.from, et_op.escrow_id);
            BOOST_CHECK_EQUAL(escrow.to, "bob");
            BOOST_CHECK_EQUAL(escrow.agent, "sam");
            BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
            BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
            BOOST_CHECK_EQUAL(escrow.sbd_balance, et_op.sbd_amount);
            BOOST_CHECK_EQUAL(escrow.steem_balance, et_op.steem_amount);
            BOOST_CHECK_EQUAL(escrow.pending_fee, et_op.fee);
            BOOST_CHECK(escrow.to_approved);
            BOOST_CHECK(!escrow.agent_approved);
            BOOST_CHECK(!escrow.disputed);

            BOOST_TEST_MESSAGE("--- failure when to does not match escrow");
            escrow_approve_operation ea_s_op;
            ea_s_op.from = "alice";
            ea_s_op.to = "bob";
            ea_s_op.agent = "sam";
            ea_s_op.who = "sam";
            ea_s_op.approve = true;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, ea_s_op));

            op.to = "dave";
            op.who = "alice";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_bad_to)));

            BOOST_CHECK_EQUAL(escrow.to, "bob");
            BOOST_CHECK_EQUAL(escrow.agent, "sam");
            BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
            BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
            BOOST_CHECK_EQUAL(escrow.sbd_balance, et_op.sbd_amount);
            BOOST_CHECK_EQUAL(escrow.steem_balance, et_op.steem_amount);
            BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.000 GOLOS"));
            BOOST_CHECK(escrow.to_approved);
            BOOST_CHECK(escrow.agent_approved);
            BOOST_CHECK(!escrow.disputed);

            BOOST_TEST_MESSAGE("--- failure when agent does not match escrow");
            op.to = "bob";
            op.who = "alice";
            op.agent = "dave";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_bad_agent)));

            BOOST_CHECK_EQUAL(escrow.to, "bob");
            BOOST_CHECK_EQUAL(escrow.agent, "sam");
            BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
            BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
            BOOST_CHECK_EQUAL(escrow.sbd_balance, et_op.sbd_amount);
            BOOST_CHECK_EQUAL(escrow.steem_balance, et_op.steem_amount);
            BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.000 GOLOS"));
            BOOST_CHECK(escrow.to_approved);
            BOOST_CHECK(escrow.agent_approved);
            BOOST_CHECK(!escrow.disputed);

            BOOST_TEST_MESSAGE("--- failure when escrow is expired");
            generate_blocks(2);

            op.agent = "sam";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::cannot_dispute_expired_escrow)));

            {
                const auto& escrow = db->get_escrow(et_op.from, et_op.escrow_id);
                BOOST_CHECK_EQUAL(escrow.to, "bob");
                BOOST_CHECK_EQUAL(escrow.agent, "sam");
                BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
                BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
                BOOST_CHECK_EQUAL(escrow.sbd_balance, et_op.sbd_amount);
                BOOST_CHECK_EQUAL(escrow.steem_balance, et_op.steem_amount);
                BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.000 GOLOS"));
                BOOST_CHECK(escrow.to_approved);
                BOOST_CHECK(escrow.agent_approved);
                BOOST_CHECK(!escrow.disputed);
            }

            BOOST_TEST_MESSAGE("--- success disputing escrow");
            et_op.escrow_id = 1;
            et_op.ratification_deadline = db->head_block_time() + STEEMIT_BLOCK_INTERVAL;
            et_op.escrow_expiration = db->head_block_time() + 2 * STEEMIT_BLOCK_INTERVAL;
            ea_b_op.escrow_id = et_op.escrow_id;
            ea_s_op.escrow_id = et_op.escrow_id;

            sign_tx_with_ops(tx, alice_private_key, et_op, ea_b_op, ea_s_op);
            tx.sign(bob_private_key, db->get_chain_id());
            tx.sign(sam_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            op.escrow_id = et_op.escrow_id;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            {
                const auto& escrow = db->get_escrow(et_op.from, et_op.escrow_id);
                BOOST_CHECK_EQUAL(escrow.to, "bob");
                BOOST_CHECK_EQUAL(escrow.agent, "sam");
                BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
                BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
                BOOST_CHECK_EQUAL(escrow.sbd_balance, et_op.sbd_amount);
                BOOST_CHECK_EQUAL(escrow.steem_balance, et_op.steem_amount);
                BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.000 GOLOS"));
                BOOST_CHECK(escrow.to_approved);
                BOOST_CHECK(escrow.agent_approved);
                BOOST_CHECK(escrow.disputed);
            }

            BOOST_TEST_MESSAGE("--- failure when escrow is already under dispute");
            op.who = "bob";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_already_disputed)));

            {
                const auto& escrow = db->get_escrow(et_op.from, et_op.escrow_id);
                BOOST_CHECK_EQUAL(escrow.to, "bob");
                BOOST_CHECK_EQUAL(escrow.agent, "sam");
                BOOST_CHECK_EQUAL(escrow.ratification_deadline, et_op.ratification_deadline);
                BOOST_CHECK_EQUAL(escrow.escrow_expiration, et_op.escrow_expiration);
                BOOST_CHECK_EQUAL(escrow.sbd_balance, et_op.sbd_amount);
                BOOST_CHECK_EQUAL(escrow.steem_balance, et_op.steem_amount);
                BOOST_CHECK_EQUAL(escrow.pending_fee, ASSET("0.000 GOLOS"));
                BOOST_CHECK(escrow.to_approved);
                BOOST_CHECK(escrow.agent_approved);
                BOOST_CHECK(escrow.disputed);
            }
        }
        FC_LOG_AND_RETHROW()
    }
    BOOST_AUTO_TEST_SUITE_END() // escrow_dispute


    BOOST_AUTO_TEST_SUITE(escrow_release)

    BOOST_AUTO_TEST_CASE(escrow_release_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow release validate");
            escrow_release_operation op;
            op.from = "alice";
            op.to = "bob";
            op.agent = "sam";
            op.who = "alice";
            op.receiver = "bob";
            op.steem_amount = ASSET_GOLOS(1);
            op.sbd_amount = ASSET_GBG(0);
            BOOST_TEST_MESSAGE("--- success");
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failure when invalid account");
            CHECK_PARAM_INVALID(op, from, "");
            CHECK_PARAM_INVALID(op, to, "");
            CHECK_PARAM_INVALID(op, agent, "");
            CHECK_PARAM_INVALID(op, who, "");
            CHECK_PARAM_INVALID(op, receiver, "");

            BOOST_TEST_MESSAGE("--- failure when who not from or to or agent");
            CHECK_PARAM_INVALID(op, who, "dave");

            BOOST_TEST_MESSAGE("--- failure when receiver not from or to");
            CHECK_PARAM_INVALID(op, receiver, "sam");
            CHECK_PARAM_INVALID(op, receiver, "dave");

            BOOST_TEST_MESSAGE("--- failure when steem < 0");
            CHECK_PARAM_INVALID(op, steem_amount, ASSET_GOLOS(-1));

            BOOST_TEST_MESSAGE("--- failure when sbd < 0");
            CHECK_PARAM_INVALID(op, sbd_amount, ASSET_GBG(-1));

            BOOST_TEST_MESSAGE("--- failure when steem == 0 and sbd == 0");
            CHECK_PARAM_INVALID_LOGIC(op, steem_amount, ASSET_GOLOS(0), escrow_no_amount_set);

            BOOST_TEST_MESSAGE("--- failure when sbd is not sbd symbol");
            CHECK_PARAM_INVALID(op, sbd_amount, ASSET_GOLOS(1));
            CHECK_PARAM_INVALID(op, sbd_amount, ASSET_GESTS(1));

            BOOST_TEST_MESSAGE("--- failure when steem is not steem symbol");
            CHECK_PARAM_INVALID(op, steem_amount, ASSET_GBG(0));
            CHECK_PARAM_INVALID(op, steem_amount, ASSET_GESTS(0));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_release_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_release_authorities");
            escrow_release_operation op;
            op.from = "alice";
            op.to = "bob";
            op.who = "alice";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({op.who}), account_name_set());
            op.who = "bob";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({op.who}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(escrow_release_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: escrow_release_apply");
            ACTORS((alice)(bob)(sam)(dave))
            fund("alice", 10000);

            escrow_transfer_operation et_op;
            et_op.from = "alice";
            et_op.to = "bob";
            et_op.agent = "sam";
            et_op.steem_amount = ASSET("1.000 GOLOS");
            et_op.fee = ASSET("0.100 GOLOS");
            et_op.ratification_deadline = db->head_block_time() + STEEMIT_BLOCK_INTERVAL;
            et_op.escrow_expiration = db->head_block_time() + 2 * STEEMIT_BLOCK_INTERVAL;

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, et_op));

            BOOST_TEST_MESSAGE("--- failure releasing funds prior to approval");
            escrow_release_operation op;
            op.from = et_op.from;
            op.to = et_op.to;
            op.agent = et_op.agent;
            op.who = et_op.from;
            op.receiver = et_op.to;
            op.steem_amount = ASSET("0.100 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::escrow_must_be_approved_first)));

            escrow_approve_operation ea_b_op;
            ea_b_op.from = "alice";
            ea_b_op.to = "bob";
            ea_b_op.agent = "sam";
            ea_b_op.who = "bob";
            escrow_approve_operation ea_s_op;
            ea_s_op.from = "alice";
            ea_s_op.to = "bob";
            ea_s_op.agent = "sam";
            ea_s_op.who = "sam";
            sign_tx_with_ops(tx, bob_private_key, ea_b_op, ea_s_op);
            tx.sign(sam_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("--- failure when 'agent' attempts to release non-disputed escrow to 'to'");
            op.who = et_op.agent;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_from_to_can_release_non_disputed)));

            BOOST_TEST_MESSAGE("--- failure when 'agent' attempts to release non-disputed escrow to 'from' ");
            op.receiver = et_op.from;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_from_to_can_release_non_disputed)));

            BOOST_TEST_MESSAGE("--- failure when 'agent' attempt to release non-disputed escrow to not 'to' or 'from'");
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- failure when other attempts to release non-disputed escrow to 'to'");
            op.receiver = et_op.to;
            op.who = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "who"));

            BOOST_TEST_MESSAGE("--- failure when other attempts to release non-disputed escrow to 'from' ");
            op.receiver = et_op.from;
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "who"));

            BOOST_TEST_MESSAGE("--- failure when other attempt to release non-disputed escrow to not 'to' or 'from'");
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "who"));

            BOOST_TEST_MESSAGE("--- failure when 'to' attemtps to release non-disputed escrow to 'to'");
            op.receiver = et_op.to;
            op.who = et_op.to;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::to_can_release_only_to_from)));

            BOOST_TEST_MESSAGE("--- failure when 'to' attempts to release non-dispured escrow to 'agent' ");
            op.receiver = et_op.agent;
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- failure when 'to' attempts to release non-disputed escrow to not 'from'");
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- success release non-disputed escrow to 'to' from 'from'");
            op.receiver = et_op.from;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
            BOOST_CHECK_EQUAL(db->get_escrow(op.from, op.escrow_id).steem_balance, ASSET("0.900 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("9.000 GOLOS"));

            BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release non-disputed escrow to 'from'");
            op.receiver = et_op.from;
            op.who = et_op.from;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::from_can_release_only_to_to)));

            BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release non-disputed escrow to 'agent'");
            op.receiver = et_op.agent;
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release non-disputed escrow to not 'from'");
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- success release non-disputed escrow to 'from' from 'to'");
            op.receiver = et_op.to;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_escrow(op.from, op.escrow_id).steem_balance, ASSET("0.800 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("bob").balance, ASSET("0.100 GOLOS"));

            BOOST_TEST_MESSAGE("--- failure when releasing more sbd than available");
            op.steem_amount = ASSET("1.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::release_amount_exceeds_escrow_balance)));

            BOOST_TEST_MESSAGE("--- failure when releasing less steem than available");
            op.steem_amount = ASSET("0.000 GOLOS");
            op.sbd_amount = ASSET("1.000 GBG");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::release_amount_exceeds_escrow_balance)));

            generate_block();
            BOOST_TEST_MESSAGE("--- failure when 'to' attempts to release disputed escrow");
            escrow_dispute_operation ed_op;
            ed_op.from = "alice";
            ed_op.to = "bob";
            ed_op.agent = "sam";
            ed_op.who = "alice";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ed_op));

            op.from = et_op.from;
            op.receiver = et_op.from;
            op.who = et_op.to;
            op.steem_amount = ASSET("0.100 GOLOS");
            op.sbd_amount = ASSET("0.000 GBG");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_agent_can_release_disputed)));

            BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release disputed escrow");
            op.receiver = et_op.to;
            op.who = et_op.from;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_agent_can_release_disputed)));

            BOOST_TEST_MESSAGE("--- failure when releasing disputed escrow to an account not 'to' or 'from'");
            op.who = et_op.agent;
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- failure when agent does not match escrow");
            op.who = "dave";
            op.receiver = et_op.from;
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "who"));

            BOOST_TEST_MESSAGE("--- success releasing disputed escrow with agent to 'to'");
            op.receiver = et_op.to;
            op.who = et_op.agent;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("bob").balance, ASSET("0.200 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_escrow(et_op.from, et_op.escrow_id).steem_balance, ASSET("0.700 GOLOS"));

            BOOST_TEST_MESSAGE("--- success releasing disputed escrow with agent to 'from'");
            op.receiver = et_op.from;
            op.who = et_op.agent;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("9.100 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_escrow(et_op.from, et_op.escrow_id).steem_balance, ASSET("0.600 GOLOS"));

            BOOST_TEST_MESSAGE("--- failure when 'to' attempts to release disputed expired escrow");
            generate_blocks(2);
            op.receiver = et_op.from;
            op.who = et_op.to;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_agent_can_release_disputed)));

            BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release disputed expired escrow");
            op.receiver = et_op.to;
            op.who = et_op.from;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_agent_can_release_disputed)));

            BOOST_TEST_MESSAGE("--- success releasing disputed expired escrow with agent");
            op.receiver = et_op.from;
            op.who = et_op.agent;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("9.200 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_escrow(et_op.from, et_op.escrow_id).steem_balance, ASSET("0.500 GOLOS"));

            BOOST_TEST_MESSAGE("--- success deleting escrow when balances are both zero");
            op.steem_amount = ASSET("0.500 GOLOS");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("9.700 GOLOS"));
            GOLOS_CHECK_ERROR_PROPS(db->get_escrow(et_op.from, et_op.escrow_id),
                CHECK_ERROR(missing_object, "escrow", make_escrow_id(et_op.from, et_op.escrow_id)));

            et_op.ratification_deadline = db->head_block_time() + STEEMIT_BLOCK_INTERVAL;
            et_op.escrow_expiration = db->head_block_time() + 2 * STEEMIT_BLOCK_INTERVAL;
            sign_tx_with_ops(tx, alice_private_key, et_op, ea_b_op, ea_s_op);
            tx.sign(bob_private_key, db->get_chain_id());
            tx.sign(sam_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
            generate_blocks(2);

            BOOST_TEST_MESSAGE("--- failure when 'agent' attempts to release non-disputed expired escrow to 'to'");
            op.receiver = et_op.to;
            op.who = et_op.agent;
            op.steem_amount = ASSET("0.100 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_from_to_can_release_non_disputed)));

            BOOST_TEST_MESSAGE("--- failure when 'agent' attempts to release non-disputed expired escrow to 'from'");
            op.receiver = et_op.from;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::only_from_to_can_release_non_disputed)));

            BOOST_TEST_MESSAGE("--- failure when 'agent' attempt to release non-disputed expired escrow to not 'to' or 'from'");
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- failure when 'to' attempts to release non-dispured expired escrow to 'agent'");
            op.who = et_op.to;
            op.receiver = et_op.agent;
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- failure when 'to' attempts to release non-disputed expired escrow to not 'from' or 'to'");
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- success release non-disputed expired escrow to 'to' from 'to'");
            op.receiver = et_op.to;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("bob").balance, ASSET("0.300 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_escrow(et_op.from, et_op.escrow_id).steem_balance, ASSET("0.900 GOLOS"));

            BOOST_TEST_MESSAGE("--- success release non-disputed expired escrow to 'from' from 'to'");
            op.receiver = et_op.from;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("8.700 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_escrow(et_op.from, et_op.escrow_id).steem_balance, ASSET("0.800 GOLOS"));

            BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release non-disputed expired escrow to 'agent'");
            op.who = et_op.from;
            op.receiver = et_op.agent;
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release non-disputed expired escrow to not 'from' or 'to'");
            op.receiver = "dave";
            GOLOS_CHECK_ERROR_PROPS(op.validate(), CHECK_ERROR(invalid_parameter, "receiver"));

            BOOST_TEST_MESSAGE("--- success release non-disputed expired escrow to 'to' from 'from'");
            op.receiver = et_op.to;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("bob").balance, ASSET("0.400 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_escrow(et_op.from, et_op.escrow_id).steem_balance, ASSET("0.700 GOLOS"));

            BOOST_TEST_MESSAGE("--- success release non-disputed expired escrow to 'from' from 'from'");
            op.receiver = et_op.from;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("8.800 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_escrow(et_op.from, et_op.escrow_id).steem_balance, ASSET("0.600 GOLOS"));

            BOOST_TEST_MESSAGE("--- success deleting escrow when balances are zero on non-disputed escrow");
            op.steem_amount = ASSET("0.600 GOLOS");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("9.400 GOLOS"));
            GOLOS_CHECK_ERROR_PROPS(db->get_escrow(et_op.from, et_op.escrow_id),
                CHECK_ERROR(missing_object, "escrow", make_escrow_id(et_op.from, et_op.escrow_id)));
        }
        FC_LOG_AND_RETHROW()
    }
    BOOST_AUTO_TEST_SUITE_END() // escrow_release
    BOOST_AUTO_TEST_SUITE_END() // escrow

//-------------------------------------------------------------

    BOOST_AUTO_TEST_SUITE(transfer_to_savings)

    BOOST_AUTO_TEST_CASE(transfer_to_savings_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_to_savings_validate");

            transfer_to_savings_operation op;
            op.from = "alice";
            op.to = "alice";
            op.amount = ASSET("1.000 GOLOS");

            BOOST_TEST_MESSAGE("--- success with valid parameters");
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, amount, ASSET_GBG(1));
            CHECK_PARAM_VALID(op, memo, string(STEEMIT_MAX_MEMO_SIZE-1, ' '));  // valid is < MAX_SIZE
            CHECK_PARAM_VALID(op, memo, u8"тест");

            BOOST_TEST_MESSAGE("--- failure when 'from' or `to` is empty");
            CHECK_PARAM_INVALID(op, from, "");
            CHECK_PARAM_INVALID(op, to, "");

            BOOST_TEST_MESSAGE("--- failure when amount is GESTS");
            CHECK_PARAM_INVALID(op, amount, ASSET("1.000 GESTS"));  // unsupported asset
            CHECK_PARAM_INVALID(op, amount, ASSET_GESTS(1));        // gests

            BOOST_TEST_MESSAGE("--- failure when amount is negative");
            CHECK_PARAM_INVALID(op, amount, ASSET_GOLOS(-1));
            CHECK_PARAM_INVALID(op, amount, ASSET_GBG(-1));

            BOOST_TEST_MESSAGE("--- failure when memo invalid");
            CHECK_PARAM_INVALID(op, memo, string(STEEMIT_MAX_MEMO_SIZE, ' '));
            CHECK_PARAM_INVALID(op, memo, BAD_UTF8_STRING);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_to_savings_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_to_savings_authorities");
            transfer_to_savings_operation op;
            op.from = "alice";
            op.to = "alice";
            op.amount = ASSET("1.000 GOLOS");
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"alice"}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_to_savings_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_to_savings_apply");

            ACTORS((alice)(bob));
            generate_block();
            fund("alice", ASSET("10.000 GOLOS"));
            fund("alice", ASSET("10.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("10.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, ASSET("10.000 GBG"));

            transfer_to_savings_operation op;
            signed_transaction tx;

            BOOST_TEST_MESSAGE("--- failure with insufficient funds");
            op.from = "alice";
            op.to = "alice";
            op.amount = ASSET("20.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "alice", "fund", "20.000 GOLOS")));
            validate_database();

            BOOST_TEST_MESSAGE("--- failure when transferring to non-existent account");
            op.to = "sam";
            op.amount = ASSET("1.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "account", "sam")));
            validate_database();

            BOOST_TEST_MESSAGE("--- success transferring STEEM to self");
            op.to = "alice";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("9.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_balance, ASSET("1.000 GOLOS"));
            validate_database();

            BOOST_TEST_MESSAGE("--- success transferring SBD to self");
            op.amount = ASSET("1.000 GBG");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, ASSET("9.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_sbd_balance, ASSET("1.000 GBG"));
            validate_database();

            BOOST_TEST_MESSAGE("--- success transferring STEEM to other");
            op.to = "bob";
            op.amount = ASSET("1.000 GOLOS");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("8.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("bob").savings_balance, ASSET("1.000 GOLOS"));
            validate_database();

            BOOST_TEST_MESSAGE("--- success transferring SBD to other");
            op.amount = ASSET("1.000 GBG");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, ASSET("8.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("bob").savings_sbd_balance, ASSET("1.000 GBG"));
            validate_database();


            BOOST_TEST_MESSAGE("--- failure when transferring without authorities");
            op.from = "bob";
            op.to = "alice";
            op.amount = ASSET("1.000 GOLOS");
            GOLOS_CHECK_THROW_PROPS(push_tx_with_ops(tx, alice_private_key, op), tx_missing_active_auth, {});
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }
    BOOST_AUTO_TEST_SUITE_END() // transfer_to_savings


    BOOST_AUTO_TEST_SUITE(transfer_from_savings)

    BOOST_AUTO_TEST_CASE(transfer_from_savings_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_from_savings_validate");

            transfer_from_savings_operation op;
            op.from = "alice";
            op.request_id = 0;
            op.to = "alice";
            op.amount = ASSET("1.000 GOLOS");
            BOOST_TEST_MESSAGE("--- success with valid parameters");
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, amount, ASSET_GBG(1));
            CHECK_PARAM_VALID(op, memo, string(STEEMIT_MAX_MEMO_SIZE-1, ' '));  // valid is < MAX_SIZE
            CHECK_PARAM_VALID(op, memo, u8"тест");

            BOOST_TEST_MESSAGE("--- failure when 'from' or 'to' is empty");
            CHECK_PARAM_INVALID(op, from, "");
            CHECK_PARAM_INVALID(op, to, "");

            BOOST_TEST_MESSAGE("--- failure when amount is GESTS");
            CHECK_PARAM_INVALID(op, amount, ASSET("1.000 GESTS"));  // unsupported asset
            CHECK_PARAM_INVALID(op, amount, ASSET_GESTS(1));        // gests

            BOOST_TEST_MESSAGE("--- failure when amount is negative");
            CHECK_PARAM_INVALID(op, amount, ASSET_GOLOS(-1));
            CHECK_PARAM_INVALID(op, amount, ASSET_GBG(-1));

            BOOST_TEST_MESSAGE("--- failure when memo invalid");
            CHECK_PARAM_INVALID(op, memo, string(STEEMIT_MAX_MEMO_SIZE, ' '));
            CHECK_PARAM_INVALID(op, memo, BAD_UTF8_STRING);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_from_savings_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_from_savings_authorities");
            transfer_from_savings_operation op;
            op.from = "alice";
            op.to = "alice";
            op.amount = ASSET("1.000 GOLOS");
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"alice"}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_from_savings_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_from_savings_apply");

            ACTORS((alice)(bob));
            generate_block();
            fund("alice", ASSET("10.000 GOLOS"));
            fund("alice", ASSET("10.000 GBG"));

            transfer_to_savings_operation save;
            save.from = "alice";
            save.to = "alice";
            save.amount = ASSET("10.000 GOLOS");

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, save));
            save.amount = ASSET("10.000 GBG");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, save));

            BOOST_TEST_MESSAGE("--- failure when account has insufficient funds");
            transfer_from_savings_operation op;
            op.from = "alice";
            op.to = "bob";
            op.amount = ASSET("20.000 GOLOS");
            op.request_id = 0;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "alice", "savings", "20.000 GOLOS")));

            BOOST_TEST_MESSAGE("--- failure withdrawing to non-existant account");
            op.to = "sam";
            op.amount = ASSET("1.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "account", "sam")));

            BOOST_TEST_MESSAGE("--- success withdrawing GOLOS to self");
            op.to = "alice";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("0.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_balance, ASSET("9.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 1);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).from, op.from);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).to, op.to);
            BOOST_CHECK_EQUAL(to_string(db->get_savings_withdraw("alice", op.request_id).memo), op.memo);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).request_id, op.request_id);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).amount, op.amount);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).complete, db->head_block_time() + STEEMIT_SAVINGS_WITHDRAW_TIME);
            validate_database();

            BOOST_TEST_MESSAGE("--- success withdrawing GBG to self");
            op.amount = ASSET("1.000 GBG");
            op.request_id = 1;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, ASSET("0.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_sbd_balance, ASSET("9.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 2);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).from, op.from);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).to, op.to);
            BOOST_CHECK_EQUAL(to_string(db->get_savings_withdraw("alice", op.request_id).memo), op.memo);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).request_id, op.request_id);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).amount, op.amount);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).complete, db->head_block_time() + STEEMIT_SAVINGS_WITHDRAW_TIME);
            validate_database();

            BOOST_TEST_MESSAGE("--- failure withdrawing with repeat request id");
            op.amount = ASSET("2.000 GOLOS");
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(object_already_exist, "savings_withdraw",
                        fc::mutable_variant_object()("owner","alice")("request_id",op.request_id))));

            BOOST_TEST_MESSAGE("--- success withdrawing GOLOS to other");
            op.to = "bob";
            op.amount = ASSET("1.000 GOLOS");
            op.request_id = 3;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("0.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_balance, ASSET("8.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 3);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).from, op.from);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).to, op.to);
            BOOST_CHECK_EQUAL(to_string(db->get_savings_withdraw("alice", op.request_id).memo), op.memo);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).request_id, op.request_id);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).amount, op.amount);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).complete, db->head_block_time() + STEEMIT_SAVINGS_WITHDRAW_TIME);
            validate_database();

            BOOST_TEST_MESSAGE("--- success withdrawing GBG to other");
            op.amount = ASSET("1.000 GBG");
            op.request_id = 4;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, ASSET("0.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_sbd_balance, ASSET("8.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 4);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).from, op.from);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).to, op.to);
            BOOST_CHECK_EQUAL(to_string(db->get_savings_withdraw("alice", op.request_id).memo), op.memo);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).request_id, op.request_id);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).amount, op.amount);
            BOOST_CHECK_EQUAL(db->get_savings_withdraw("alice", op.request_id).complete, db->head_block_time() + STEEMIT_SAVINGS_WITHDRAW_TIME);
            validate_database();

            BOOST_TEST_MESSAGE("--- withdraw on timeout");
            generate_blocks(db->head_block_time() + STEEMIT_SAVINGS_WITHDRAW_TIME - fc::seconds(STEEMIT_BLOCK_INTERVAL), true);

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("0.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, ASSET("0.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("bob").balance, ASSET("0.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("bob").sbd_balance, ASSET("0.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 4);
            validate_database();

            generate_block();

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("1.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, ASSET("1.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("bob").balance, ASSET("1.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("bob").sbd_balance, ASSET("1.000 GBG"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 0);
            validate_database();

            BOOST_TEST_MESSAGE("--- savings withdraw request limit");
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            op.to = "alice";
            op.amount = ASSET("0.001 GOLOS");

            for (int i = 0; i < STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT; i++) {
                op.request_id = i;
                BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
                BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, i + 1);
            }

            op.request_id = STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::reached_limit_for_pending_withdraw_requests)));

            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(transfer_from_savings_memo_storing_flag) {
        try {
            BOOST_TEST_MESSAGE("Testing: transfer_from_savings_memo_storing_flag");

            ACTORS((alice)(bob));
            generate_block();
            fund("alice", ASSET("10.000 GOLOS"));

            transfer_to_savings_operation save;
            save.from = "alice";
            save.to = "alice";
            save.amount = ASSET("10.000 GOLOS");
            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, save));

            // Default is true
            transfer_from_savings_operation op;
            op.from = "alice";
            op.to = "alice";
            op.amount = ASSET("1.000 GOLOS");
            op.request_id = 1;
            op.memo = "{\"test\":123}";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            BOOST_CHECK_EQUAL(to_string(db->get_savings_withdraw("alice", op.request_id).memo), op.memo);

            db->set_store_memo_in_savings_withdraws(true);
            op.request_id = 2;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            BOOST_CHECK_EQUAL(to_string(db->get_savings_withdraw("alice", op.request_id).memo), op.memo);

            db->set_store_memo_in_savings_withdraws(false);
            op.request_id = 3;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            BOOST_CHECK_EQUAL(to_string(db->get_savings_withdraw("alice", op.request_id).memo), "");

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }
    BOOST_AUTO_TEST_SUITE_END() // transfer_from_savings


    BOOST_AUTO_TEST_CASE(cancel_transfer_from_savings_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: cancel_transfer_from_savings_validate");
            cancel_transfer_from_savings_operation op;
            op.from = "alice";
            op.request_id = 0;
            BOOST_TEST_MESSAGE("--- sucess on valid params");
            CHECK_OP_VALID(op);
            BOOST_TEST_MESSAGE("--- failure when 'from' is invalid");
            CHECK_PARAM_INVALID(op, from, "");

        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(cancel_transfer_from_savings_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: cancel_transfer_from_savings_authorities");
            cancel_transfer_from_savings_operation op;
            op.from = "alice";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"alice"}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(cancel_transfer_from_savings_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: cancel_transfer_from_savings_apply");

            ACTORS((alice)(bob))
            generate_block();

            fund("alice", ASSET("10.000 GOLOS"));

            transfer_to_savings_operation save;
            save.from = "alice";
            save.to = "alice";
            save.amount = ASSET("10.000 GOLOS");

            transfer_from_savings_operation withdraw;
            withdraw.from = "alice";
            withdraw.to = "bob";
            withdraw.request_id = 1;
            withdraw.amount = ASSET("3.000 GOLOS");

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, save, withdraw));
            validate_database();
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 1);
            BOOST_CHECK_EQUAL(db->get_account("bob").savings_withdraw_requests, 0);

            BOOST_TEST_MESSAGE("--- Failure when there is no pending request");
            cancel_transfer_from_savings_operation op;
            op.from = "alice";
            op.request_id = 0;

            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "savings_withdraw",
                        fc::mutable_variant_object()("account","alice")("request_id",op.request_id))));
            validate_database();
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 1);
            BOOST_CHECK_EQUAL(db->get_account("bob").savings_withdraw_requests, 0);

            BOOST_TEST_MESSAGE("--- Success");
            op.request_id = 1;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_CHECK_EQUAL(db->get_account("alice").balance, ASSET("0.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_balance, ASSET("10.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("alice").savings_withdraw_requests, 0);
            BOOST_CHECK_EQUAL(db->get_account("bob").balance, ASSET("0.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("bob").savings_balance, ASSET("0.000 GOLOS"));
            BOOST_CHECK_EQUAL(db->get_account("bob").savings_withdraw_requests, 0);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(decline_voting_rights_validate) { try {
        BOOST_TEST_MESSAGE("Testing: decline_voting_rights_validate");
        decline_voting_rights_operation op;
        op.account = "alice";
        CHECK_OP_VALID(op);
        CHECK_PARAM_INVALID(op, account, "");
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(decline_voting_rights_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: decline_voting_rights_authorities");
            decline_voting_rights_operation op;
            op.account = "alice";
            CHECK_OP_AUTHS(op, account_name_set({"alice"}), account_name_set(), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(decline_voting_rights_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: decline_voting_rights_apply");

            ACTORS((alice)(bob));
            generate_block();
            vest("alice", ASSET("10.000 GOLOS"));
            vest("bob", ASSET("10.000 GOLOS"));
            generate_block();

            account_witness_proxy_operation proxy;
            proxy.account = "bob";
            proxy.proxy = "alice";

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, proxy));

            decline_voting_rights_operation op;
            op.account = "alice";

            BOOST_TEST_MESSAGE("--- success");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            const auto& request_idx = db->get_index<decline_voting_rights_request_index>().indices().get<by_account>();
            auto itr = request_idx.find(db->get_account("alice").id);
            BOOST_CHECK(itr != request_idx.end());
            BOOST_CHECK_EQUAL(itr->effective_date, db->head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD);

            BOOST_TEST_MESSAGE("--- failure revoking voting rights with existing request");
            generate_block();
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(object_already_exist, "decline_voting_rights_request", "alice")));

            BOOST_TEST_MESSAGE("--- successs cancelling a request");
            op.decline = false;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            itr = request_idx.find(db->get_account("alice").id);
            BOOST_CHECK(itr == request_idx.end());

            BOOST_TEST_MESSAGE("--- failure cancelling a request that doesn't exist");
            generate_block();
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "decline_voting_rights_request", "alice")));

            BOOST_TEST_MESSAGE("--- check account can vote during waiting period");
            op.decline = true;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            generate_blocks(
                db->head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD - fc::seconds(STEEMIT_BLOCK_INTERVAL), true);
            BOOST_CHECK(db->get_account("alice").can_vote);
            witness_create("alice", alice_private_key, "foo.bar", alice_private_key.get_public_key(), 0);

            account_witness_vote_operation witness_vote;
            witness_vote.account = "alice";
            witness_vote.witness = "alice";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, witness_vote));

            comment_operation comment;
            comment.author = "alice";
            comment.permlink = "test";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "test";
            vote_operation vote;
            vote.voter = "alice";
            vote.author = "alice";
            vote.permlink = "test";
            vote.weight = STEEMIT_100_PERCENT;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment, vote));
            validate_database();

            BOOST_TEST_MESSAGE("--- check account cannot vote after request is processed");
            generate_block();
            BOOST_CHECK(!db->get_account("alice").can_vote);
            validate_database();

            itr = request_idx.find(db->get_account("alice").id);
            BOOST_CHECK(itr == request_idx.end());

            const auto& witness_idx = db->get_index<witness_vote_index>().indices().get<by_account_witness>();
            auto witness_itr = witness_idx.find(
                boost::make_tuple(db->get_account("alice").id, db->get_witness("alice").id));
            BOOST_CHECK(witness_itr == witness_idx.end());

            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, witness_vote),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::voter_declined_voting_rights)));

            db->get<comment_vote_object, by_comment_voter>(
                boost::make_tuple(db->get_comment("alice", string("test")).id, db->get_account("alice").id)
            );

            vote.weight = 0;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, vote),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::voter_declined_voting_rights)));

            vote.weight = STEEMIT_1_PERCENT * 50;
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, vote),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::voter_declined_voting_rights)));

            proxy.account = "alice";
            proxy.proxy = "bob";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, proxy),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::voter_declined_voting_rights)));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_bandwidth) {
        try {
            ACTORS((alice)(bob))
            generate_block();
            vest("alice", ASSET("10.000 GOLOS"));
            fund("alice", ASSET("10.000 GOLOS"));
            vest("bob", ASSET("10.000 GOLOS"));

            generate_block();
            db->skip_transaction_delta_check = false;

            signed_transaction tx;
            transfer_operation op;

            op.from = "alice";
            op.to = "bob";
            op.amount = ASSET("1.000 GOLOS");

            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());

            db->push_transaction(tx, 0);

            auto last_bandwidth_update = db->get<account_bandwidth_object, by_account_bandwidth_type>(
                    boost::make_tuple("alice", bandwidth_type::market)).last_bandwidth_update;
            auto average_bandwidth = db->get<account_bandwidth_object, by_account_bandwidth_type>(
                    boost::make_tuple("alice", bandwidth_type::market)).average_bandwidth;
            BOOST_REQUIRE(last_bandwidth_update == db->head_block_time());
            BOOST_REQUIRE(average_bandwidth == fc::raw::pack_size(tx) * 10 * STEEMIT_BANDWIDTH_PRECISION);
            auto total_bandwidth = average_bandwidth;

            op.amount = ASSET("0.100 GOLOS");
            tx.clear();
            tx.operations.push_back(op);
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db->get_chain_id());

            db->push_transaction(tx, 0);

            last_bandwidth_update = db->get<account_bandwidth_object, by_account_bandwidth_type>(
                    boost::make_tuple("alice", bandwidth_type::market)).last_bandwidth_update;
            average_bandwidth = db->get<account_bandwidth_object, by_account_bandwidth_type>(
                    boost::make_tuple("alice", bandwidth_type::market)).average_bandwidth;
            BOOST_REQUIRE(last_bandwidth_update == db->head_block_time());
            BOOST_REQUIRE(average_bandwidth == total_bandwidth + fc::raw::pack_size(tx) * 10 * STEEMIT_BANDWIDTH_PRECISION);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(comment_beneficiaries_validate) {
        try {
            BOOST_TEST_MESSAGE("Test Comment Beneficiaries Validate");
            comment_options_operation op;

            op.author = "alice";
            op.permlink = "test";

            BOOST_TEST_MESSAGE("--- Testing more than 100% weight on a single route");
            comment_payout_beneficiaries b;
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("bob"), STEEMIT_100_PERCENT + 1));
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "beneficiaries"));

            BOOST_TEST_MESSAGE("--- Testing more than 100% total weight");
            b.beneficiaries.clear();
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("bob"), STEEMIT_1_PERCENT * 75));
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("sam"), STEEMIT_1_PERCENT * 75));
            op.extensions.clear();
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "beneficiaries"));

            BOOST_TEST_MESSAGE("--- Testing maximum number of routes");
            b.beneficiaries.clear();
            for (size_t i = 0; i < 127; i++) {
                b.beneficiaries.push_back(beneficiary_route_type(account_name_type("foo" + fc::to_string(i)), 1));
            }

            op.extensions.clear();
            std::sort(b.beneficiaries.begin(), b.beneficiaries.end());
            op.extensions.insert(b);
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- Testing one too many routes");
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("bar"), 1));
            std::sort(b.beneficiaries.begin(), b.beneficiaries.end());
            op.extensions.clear();
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "beneficiaries"));

            BOOST_TEST_MESSAGE("--- Testing duplicate accounts");
            b.beneficiaries.clear();
            b.beneficiaries.push_back(beneficiary_route_type("bob", STEEMIT_1_PERCENT * 2));
            b.beneficiaries.push_back(beneficiary_route_type("bob", STEEMIT_1_PERCENT));
            op.extensions.clear();
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "beneficiaries"));

            BOOST_TEST_MESSAGE("--- Testing incorrect account sort order");
            b.beneficiaries.clear();
            b.beneficiaries.push_back(beneficiary_route_type("bob", STEEMIT_1_PERCENT));
            b.beneficiaries.push_back(beneficiary_route_type("alice", STEEMIT_1_PERCENT));
            op.extensions.clear();
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "beneficiaries"));

            BOOST_TEST_MESSAGE("--- Testing correct account sort order");
            b.beneficiaries.clear();
            b.beneficiaries.push_back(beneficiary_route_type("alice", STEEMIT_1_PERCENT));
            b.beneficiaries.push_back(beneficiary_route_type("bob", STEEMIT_1_PERCENT));
            op.extensions.clear();
            op.extensions.insert(b);
            BOOST_CHECK_NO_THROW(op.validate());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(comment_beneficiaries_apply) {
        try {
            BOOST_TEST_MESSAGE("Test Comment Beneficiaries");
            ACTORS((alice)(bob)(sam))
            generate_block();

            fund("alice", 10000);
            vest("alice", 10000);
            fund("bob", 10000);
            vest("bob", 10000);
            fund("sam", 10000);
            vest("sam", 10000);

            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            comment_operation comment;
            vote_operation vote;
            comment_options_operation op;
            comment_payout_beneficiaries b;
            signed_transaction tx;

            comment.author = "alice";
            comment.permlink = "test";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "foobar";

            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));

            BOOST_TEST_MESSAGE("--- Test failure on max of benefactors");
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("bob"), STEEMIT_1_PERCENT));

            for (size_t i = 0; i < STEEMIT_MAX_COMMENT_BENEFICIARIES; i++) {
                b.beneficiaries.push_back(
                    beneficiary_route_type(
                        account_name_type(STEEMIT_INIT_MINER_NAME + fc::to_string(i)),
                        STEEMIT_1_PERCENT));
            }

            op.author = "alice";
            op.permlink = "test";
            op.allow_curation_rewards = false;
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::cannot_specify_more_beneficiaries)));


            BOOST_TEST_MESSAGE("--- Test specifying a non-existent benefactor");
            b.beneficiaries.clear();
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("dave"), STEEMIT_1_PERCENT));
            op.extensions.clear();
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "account", "dave")));


            BOOST_TEST_MESSAGE("--- Test setting when comment has been voted on");
            vote.author = "alice";
            vote.permlink = "test";
            vote.voter = "bob";
            vote.weight = STEEMIT_100_PERCENT;

            b.beneficiaries.clear();
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("bob"), 25 * STEEMIT_1_PERCENT));
            op.extensions.clear();
            op.extensions.insert(b);

            tx.clear();
            tx.operations.push_back(vote);
            tx.operations.push_back(op);
            tx.sign(alice_private_key, db->get_chain_id());
            tx.sign(bob_private_key, db->get_chain_id());
            GOLOS_CHECK_ERROR_PROPS(db->push_transaction(tx),
                CHECK_ERROR(tx_invalid_operation, 1,
                    CHECK_ERROR(logic_exception, logic_exception::comment_options_requires_no_rshares)));


            BOOST_TEST_MESSAGE("--- Test success");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));


            BOOST_TEST_MESSAGE("--- Test setting when there are already beneficiaries");
            b.beneficiaries.clear();
            b.beneficiaries.push_back(beneficiary_route_type(account_name_type("sam"), 25 * STEEMIT_1_PERCENT));
            op.extensions.clear();
            op.extensions.insert(b);
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::comment_already_has_beneficiaries)));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(delete_comment_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: delete_comment_validate");

            delete_comment_operation op;

            BOOST_TEST_MESSAGE("--- success on valid parameters");
            op.author = "alice";
            op.permlink = "foo";
            CHECK_OP_VALID(op);

            BOOST_TEST_MESSAGE("--- failed when 'author' is invalid");
            CHECK_PARAM_INVALID(op, author, "");
            CHECK_PARAM_INVALID(op, author, "a");

            BOOST_TEST_MESSAGE("--- failed when 'permlink' is invalid");
            CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH, ' '));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(delete_comment_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: delete_comment_authorities");
            delete_comment_operation op;
            op.author = "alice";
            op.permlink = "foo";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"alice"}));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(delete_comment_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: delete_comment_apply");
            ACTORS((alice)(bob))

            signed_transaction tx;
            delete_comment_operation op;

            BOOST_TEST_MESSAGE("--- failed when comment missing");
            op.author = "alice";
            op.permlink = "foo";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "comment", make_comment_id("alice", "foo"))));
            validate_database();

            BOOST_TEST_MESSAGE("--- prepare testing comments");
            {
                comment_operation op;
                op.author = "alice";
                op.permlink = "lorem";
                op.parent_author = "";
                op.parent_permlink = "ipsum";
                op.title = "Lorem Ipsum";
                op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
                op.json_metadata = "{\"foo\":\"bar\"}";
                BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

                op.author = "bob";
                op.permlink = "bar";
                op.parent_author = "alice";
                op.parent_permlink = "lorem";
                BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
                validate_database();
            }

            BOOST_TEST_MESSAGE("--- failed when comment has replies");
            op.author = "alice";
            op.permlink = "lorem";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::cannot_delete_comment_with_replies)));
            validate_database();

            BOOST_TEST_MESSAGE("--- success delete comment");
            op.author = "bob";
            op.permlink = "bar";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
            validate_database();

            BOOST_TEST_MESSAGE("--- success delete comment after delete replies");
            op.author = "alice";
            op.permlink = "lorem";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_SUITE(delegation)

    BOOST_AUTO_TEST_CASE(account_create_with_delegation_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_create_with_delegation_validate");

            BOOST_TEST_MESSAGE("--- Test valid operation");
            account_create_with_delegation_operation op;
            private_key_type priv_key = generate_private_key("temp_key");
            op.fee = ASSET_GOLOS(10);
            op.delegation = ASSET_GESTS(100);
            op.creator = "alice";
            op.new_account_name = "bob";
            op.owner = authority(1, priv_key.get_public_key(), 1);
            op.active = authority(1, priv_key.get_public_key(), 1);
            op.memo_key = priv_key.get_public_key();
            op.json_metadata = "{\"foo\":\"bar\"}";
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, json_metadata, "");
            CHECK_PARAM_VALID(op, json_metadata, "{\"a\":\"тест\"}");

            BOOST_TEST_MESSAGE("--- Test failing on negative fee");
            CHECK_PARAM_INVALID(op, fee, ASSET_GOLOS(-1));

            BOOST_TEST_MESSAGE("--- Test failing when fee is not GOLOS");
            CHECK_PARAM_INVALID(op, fee, ASSET_GBG(10));

            BOOST_TEST_MESSAGE("--- Test failing on negative delegation");
            CHECK_PARAM_INVALID(op, delegation, ASSET_GESTS(-1));

            BOOST_TEST_MESSAGE("--- Test failing when delegation is not VESTS");
            CHECK_PARAM_INVALID(op, delegation, ASSET_GOLOS(100));

            BOOST_TEST_MESSAGE("--- Test failing when account empty");
            CHECK_PARAM_INVALID(op, creator, "");
            CHECK_PARAM_INVALID(op, new_account_name, "");

            BOOST_TEST_MESSAGE("--- Test failing when json_metadata invalid");
            CHECK_PARAM_INVALID(op, json_metadata, "{a:b}");

            // TODO: owner/active/posting
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_create_with_delegation_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_create_with_delegation_authorities");
            account_create_with_delegation_operation op;
            op.creator = "bob";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"bob"}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_create_with_delegation_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_create_with_delegation_apply");
            signed_transaction tx;
            ACTOR(alice);

            generate_blocks(1);
            fund("alice", ASSET_GOLOS(10));
            vest("alice", ASSET_GOLOS(10000));
            private_key_type priv_key = generate_private_key("temp_key");

            generate_block();
            db_plugin->debug_update([=](database& db) {
                db.modify(db.get_witness_schedule_object(), [&](witness_schedule_object& w) {
                    w.median_props.account_creation_fee = ASSET_GOLOS(1);
                });
            });
            generate_block();

            BOOST_TEST_MESSAGE("--- Test failure when GESTS are powering down");
            withdraw_vesting_operation withdraw;
            withdraw.account = "alice";
            withdraw.vesting_shares = db->get_account("alice").vesting_shares;

            account_create_with_delegation_operation op;
            op.fee = ASSET_GOLOS(10);
            op.delegation = ASSET_GESTS(1e7);
            op.creator = "alice";
            op.new_account_name = "bob";
            op.owner = authority(1, priv_key.get_public_key(), 1);
            op.active = authority(1, priv_key.get_public_key(), 1);
            op.memo_key = priv_key.get_public_key();
            op.json_metadata = "{\"foo\":\"bar\"}";
            sign_tx_with_ops(tx, alice_private_key, withdraw, op);
            GOLOS_CHECK_THROW_PROPS(db->push_transaction(tx, 0), tx_invalid_operation, {});

            BOOST_TEST_MESSAGE("--- Test success under normal conditions");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            const account_object& bob_acc = db->get_account("bob");
            const account_object& alice_acc = db->get_account("alice");
            BOOST_CHECK_EQUAL(alice_acc.delegated_vesting_shares, ASSET_GESTS(1e7));
            BOOST_CHECK_EQUAL(bob_acc.received_vesting_shares, ASSET_GESTS(1e7));
            BOOST_CHECK_EQUAL(bob_acc.available_vesting_shares(true),
                bob_acc.vesting_shares - bob_acc.delegated_vesting_shares);
            BOOST_CHECK_EQUAL(bob_acc.available_vesting_shares(),
                bob_acc.vesting_shares - bob_acc.delegated_vesting_shares);
            BOOST_CHECK_EQUAL(bob_acc.effective_vesting_shares(),
                bob_acc.vesting_shares - bob_acc.delegated_vesting_shares + bob_acc.received_vesting_shares);

            BOOST_TEST_MESSAGE("--- Test delegation object integrity");
            auto delegation = db->find<vesting_delegation_object, by_delegation>(std::make_tuple(op.creator, op.new_account_name));
            BOOST_CHECK(delegation != nullptr);
            BOOST_CHECK_EQUAL(delegation->delegator, op.creator);
            BOOST_CHECK_EQUAL(delegation->delegatee, op.new_account_name);
            BOOST_CHECK_EQUAL(delegation->vesting_shares, ASSET_GESTS(1e7));
            BOOST_CHECK_EQUAL(delegation->min_delegation_time, db->head_block_time() + GOLOS_CREATE_ACCOUNT_DELEGATION_TIME);

            auto delegated = delegation->vesting_shares;
            auto exp_time = delegation->min_delegation_time;
            generate_block();

            BOOST_TEST_MESSAGE("--- Test success using only GOLOS to reach target delegation");
            const auto& gp = db->get_dynamic_global_properties();
            const auto& mp = db->get_witness_schedule_object().median_props;
            auto min_fee = mp.create_account_min_golos_fee;
            auto required_fee = min_fee + mp.create_account_min_delegation;
            auto required_gests = required_fee * gp.get_vesting_share_price();
            op.fee = required_fee;
            op.delegation = ASSET_GESTS(0);
            op.new_account_name = "sam";
            fund("alice", op.fee);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_TEST_MESSAGE("--- Test success using minimum GOLOS fee");
            op.fee = min_fee;
            op.delegation = (required_fee - min_fee) * gp.get_vesting_share_price();
            op.new_account_name = "pam";
            fund("alice", op.fee);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_TEST_MESSAGE("--- Test success using both GESTS and GOLOS to reach target delegation");
            op.fee = asset(required_fee.amount / 2 + 1, STEEM_SYMBOL);
            op.delegation = asset(required_gests.amount / 2 + 1, VESTS_SYMBOL);
            op.new_account_name = "ram";
            fund("alice", op.fee);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));

            BOOST_TEST_MESSAGE("--- Test failure when insufficient funds to process transaction");
            op.fee = ASSET_GOLOS(10);
            op.delegation = ASSET_GESTS(0);
            op.new_account_name = "dave";
            sign_tx_with_ops(tx, alice_private_key, op);
            GOLOS_CHECK_THROW_PROPS(db->push_transaction(tx, 0), fc::exception, {});

            BOOST_TEST_MESSAGE("--- Test failure when insufficient fee to reach target delegation");
            fund("alice", required_fee);
            op.fee = ASSET_GOLOS(0);
            op.delegation = required_gests - ASSET_GESTS(1);
            sign_tx_with_ops(tx, alice_private_key, op);
            GOLOS_CHECK_THROW_PROPS(db->push_transaction(tx, 0), fc::exception, {});
            validate_database();

            BOOST_TEST_MESSAGE("--- Test removing delegation from new account");
            delegate_vesting_shares_operation delegate;
            delegate.delegator = "alice";
            delegate.delegatee = "bob";
            delegate.vesting_shares = ASSET_GESTS(0);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, delegate));

            auto itr = db->get_index<vesting_delegation_expiration_index, by_id>().begin();
            auto end = db->get_index<vesting_delegation_expiration_index, by_id>().end();
            BOOST_CHECK(itr != end);
            BOOST_CHECK_EQUAL(itr->delegator, "alice");
            BOOST_CHECK_EQUAL(itr->vesting_shares, delegated);
            BOOST_CHECK_EQUAL(itr->expiration, exp_time);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(delegate_vesting_shares_validate) {
        try {
            delegate_vesting_shares_operation op;
            op.delegator = "alice";
            op.delegatee = "bob";
            op.vesting_shares = ASSET_GESTS(1e6);
            BOOST_TEST_MESSAGE("--- Test success under normal conditions");
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, vesting_shares, ASSET_GESTS(0));

            BOOST_TEST_MESSAGE("--- Test failure when delegate negative amount");
            CHECK_PARAM_INVALID(op, vesting_shares, ASSET_GESTS(-1));

            BOOST_TEST_MESSAGE("--- Test failure when delegate to same acc");
            CHECK_PARAM_INVALID_LOGIC(op, delegator, "bob", logic_exception::cannot_delegate_to_yourself);

            BOOST_TEST_MESSAGE("--- Test failure when account not set");
            CHECK_PARAM_INVALID(op, delegator, "");
            CHECK_PARAM_INVALID(op, delegatee, "");
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(delegate_vesting_shares_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: delegate_vesting_shares_authorities");
            delegate_vesting_shares_operation op;
            op.delegator = "bob";
            op.delegatee = "alice";
            op.vesting_shares = ASSET_GESTS(1e6);
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"bob"}), account_name_set());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(delegate_vesting_shares_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: delegate_vesting_shares_apply");
            signed_transaction tx;
            ACTORS((alice)(bob))

            generate_block();
            vest("alice", ASSET_GOLOS(10000));
            vest("bob", ASSET_GOLOS(1000));
            generate_block();
            db_plugin->debug_update([=](database& db) {
                db.modify(db.get_witness_schedule_object(), [&](witness_schedule_object& w) {
                    w.median_props.account_creation_fee = ASSET_GOLOS(1);
                });
            });
            generate_block();

            delegate_vesting_shares_operation op;
            op.vesting_shares = ASSET_GESTS(1e6);
            op.delegator = "alice";
            op.delegatee = "bob";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            generate_blocks(1);
            const auto& alice_acc = db->get_account("alice");
            const auto& bob_acc = db->get_account("bob");
            BOOST_CHECK_EQUAL(alice_acc.delegated_vesting_shares, ASSET_GESTS(1e6));
            BOOST_CHECK_EQUAL(bob_acc.received_vesting_shares, ASSET_GESTS(1e6));

            BOOST_TEST_MESSAGE("--- Test that the delegation object is correct");
            auto delegation = db->find<vesting_delegation_object, by_delegation>(std::make_tuple(op.delegator, op.delegatee));
            BOOST_CHECK(delegation != nullptr);
            BOOST_CHECK_EQUAL(delegation->delegator, op.delegator);
            BOOST_CHECK_EQUAL(delegation->delegatee, op.delegatee);
            BOOST_CHECK_EQUAL(delegation->vesting_shares, ASSET_GESTS(1e6));
            validate_database();

            BOOST_TEST_MESSAGE("--- Test delegation change");
            op.vesting_shares = ASSET_GESTS(2e7);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            generate_blocks(1);
            BOOST_CHECK(delegation != nullptr);
            BOOST_CHECK_EQUAL(delegation->vesting_shares, ASSET_GESTS(2e7));
            BOOST_CHECK_EQUAL(alice_acc.delegated_vesting_shares, ASSET_GESTS(2e7));
            BOOST_CHECK_EQUAL(bob_acc.received_vesting_shares, ASSET_GESTS(2e7));

            // TODO: test min delta evaluator logic

            BOOST_TEST_MESSAGE("--- Test that effective vesting shares is accurate and being applied");
            comment_operation comment_op;
            comment_op.author = "alice";
            comment_op.permlink = "foo";
            comment_op.parent_permlink = "test";
            comment_op.title = "bar";
            comment_op.body = "foo bar";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment_op));

            auto old_voting_power = bob_acc.voting_power;
            vote_operation vote_op;
            vote_op.voter = "bob";
            vote_op.author = "alice";
            vote_op.permlink = "foo";
            vote_op.weight = STEEMIT_100_PERCENT;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, vote_op));
            generate_blocks(1);

            auto& alice_comment = db->get_comment("alice", string("foo"));
            const auto& vote_idx = db->get_index<comment_vote_index>().indices().get<by_comment_voter>();
            auto itr = vote_idx.find(std::make_tuple(alice_comment.id, bob_acc.id));
            auto rshares = bob_acc.effective_vesting_shares().amount.value *
                (old_voting_power - bob_acc.voting_power) / STEEMIT_100_PERCENT;
            BOOST_CHECK_EQUAL(rshares, itr->rshares);
            BOOST_CHECK_EQUAL(rshares, alice_comment.net_rshares.value);

            BOOST_TEST_MESSAGE("--- Test that delegation limited by current voting power");
            auto max_allowed = bob_acc.vesting_shares * bob_acc.voting_power / STEEMIT_100_PERCENT;
            op.delegator = "bob";
            op.delegatee = "alice";
            op.vesting_shares = asset(max_allowed.amount + 1, VESTS_SYMBOL);
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::delegation_limited_by_voting_power)));
            op.vesting_shares = max_allowed;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

            generate_block();
            ACTORS((sam)(dave))
            generate_block();
            vest("sam", ASSET_GOLOS(1000));
            generate_block();
            const auto& sam_acc = db->get_account("sam");
            auto sam_vest = sam_acc.vesting_shares;

            BOOST_TEST_MESSAGE("--- Test failure when delegating 0 GESTS");
            op.vesting_shares = ASSET_GESTS(0);
            op.delegator = "sam";
            op.delegatee = "dave";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(logic_exception, logic_exception::delegation_difference_too_low)));

            BOOST_TEST_MESSAGE("--- Test failure delegating more vesting shares than account has");
            op.vesting_shares = asset(sam_vest.amount + 1, VESTS_SYMBOL);
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "sam", "available vesting shares", op.vesting_shares)));

            BOOST_TEST_MESSAGE("--- Test failure delegating vesting shares that are part of a power down");
            sam_vest = asset(sam_vest.amount / 2, VESTS_SYMBOL);
            withdraw_vesting_operation withdraw;
            withdraw.account = "sam";
            withdraw.vesting_shares = sam_vest;
            op.vesting_shares = asset(sam_vest.amount + 2, VESTS_SYMBOL);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, withdraw));
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "sam", "available vesting shares", op.vesting_shares)));

            BOOST_TEST_MESSAGE("--- Test available_vesting_shares calculation with active power down");
            BOOST_CHECK_EQUAL(sam_acc.available_vesting_shares(true),
                sam_acc.vesting_shares - sam_acc.delegated_vesting_shares - asset(sam_acc.to_withdraw, VESTS_SYMBOL));

            withdraw.vesting_shares = ASSET_GESTS(0);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, withdraw));

            BOOST_TEST_MESSAGE("--- Test failure powering down vesting shares that are delegated");
            sam_vest.amount += 1000;
            op.vesting_shares = sam_vest;
            withdraw.vesting_shares = asset(sam_vest.amount, VESTS_SYMBOL);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, sam_private_key, withdraw),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(insufficient_funds, "sam", "having vesting shares", sam_vest)));

            BOOST_TEST_MESSAGE("--- Remove a delegation and ensure it is returned after 1 week");
            op.vesting_shares = ASSET_GESTS(0);
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, sam_private_key, op));

            auto exp_obj = db->get_index<vesting_delegation_expiration_index, by_id>().begin();
            auto end = db->get_index<vesting_delegation_expiration_index, by_id>().end();
            BOOST_CHECK(exp_obj != end);
            BOOST_CHECK_EQUAL(exp_obj->delegator, "sam");
            BOOST_CHECK_EQUAL(exp_obj->vesting_shares, sam_vest);
            BOOST_CHECK_EQUAL(exp_obj->expiration, db->head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS);
            BOOST_CHECK_EQUAL(db->get_account("sam").delegated_vesting_shares, sam_vest);
            BOOST_CHECK_EQUAL(db->get_account("dave").received_vesting_shares, ASSET_GESTS(0));
            delegation = db->find<vesting_delegation_object, by_delegation>(std::make_tuple(op.delegator, op.delegatee));
            BOOST_CHECK(delegation == nullptr);

            generate_blocks(exp_obj->expiration + STEEMIT_BLOCK_INTERVAL);
            exp_obj = db->get_index<vesting_delegation_expiration_index, by_id>().begin();
            end = db->get_index<vesting_delegation_expiration_index, by_id>().end();
            BOOST_CHECK(exp_obj == end);
            BOOST_CHECK_EQUAL(db->get_account("sam").delegated_vesting_shares, ASSET_GESTS(0));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(reject_vesting_shares_delegation_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: reject_vesting_shares_delegation_apply");
            signed_transaction tx;
            ACTORS((alice)(bob))

            generate_block();
            vest("alice", ASSET_GOLOS(10000));
            vest("bob", ASSET_GOLOS(1000));
            generate_block();

            BOOST_TEST_MESSAGE("--- Test failure when vesting shares delegation does not exist");
            reject_vesting_shares_delegation_operation reject_op;
            reject_op.delegator = "alice";
            reject_op.delegatee = "bob";
            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, reject_op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(missing_object, "vesting_delegation_object", fc::mutable_variant_object()("delegator",reject_op.delegator)("delegatee",reject_op.delegatee))));

            delegate_vesting_shares_operation op;
            op.vesting_shares = ASSET_GESTS(5000);
            op.delegator = "alice";
            op.delegatee = "bob";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            generate_block();

            auto delegation = db->find<vesting_delegation_object, by_delegation>(std::make_tuple(op.delegator, op.delegatee));
            BOOST_CHECK(delegation != nullptr);
            BOOST_CHECK_EQUAL(delegation->delegator, op.delegator);
            BOOST_CHECK_EQUAL(delegation->delegatee, op.delegatee);
            BOOST_CHECK_EQUAL(delegation->vesting_shares, ASSET_GESTS(5000));

            BOOST_TEST_MESSAGE("--- Test success");
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, reject_op));
            generate_block();

            delegation = db->find<vesting_delegation_object, by_delegation>(std::make_tuple(op.delegator, op.delegatee));
            BOOST_CHECK(delegation == nullptr);

            BOOST_CHECK_EQUAL(db->get_account("bob").received_vesting_shares, ASSET_GESTS(0));

            BOOST_CHECK_NE(db->get_account("alice").delegated_vesting_shares, ASSET_GESTS(0));

            generate_blocks(db->head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS - 1);

            BOOST_CHECK_NE(db->get_account("alice").delegated_vesting_shares, ASSET_GESTS(0));

            generate_block();

            BOOST_CHECK_EQUAL(db->get_account("alice").delegated_vesting_shares, ASSET_GESTS(0));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(delegate_vesting_shares_with_interest_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: delegate_vesting_shares_with_interest_apply");

            ACTORS((alice)(bob)(carol)(dave))
            generate_block();

            fund("alice", 10000);
            vest("alice", ASSET_GOLOS(10000));
            vest("carol", ASSET_GOLOS(10000));
            vest("dave", ASSET_GOLOS(10000));
            generate_block();

            price exchange_rate(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
            set_price_feed(exchange_rate);
            generate_block();

            signed_transaction tx;

            delegate_vesting_shares_with_interest_operation op;
            op.vesting_shares = ASSET_GESTS(50000);
            op.delegator = "carol";
            op.delegatee = "bob";
            op.payout_strategy = to_delegated_vesting;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, op));
            generate_block();
            tx.operations.clear();
            tx.signatures.clear();

            op.payout_strategy = to_delegator;
            op.delegator = "dave";
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, op));
            generate_block();
            tx.operations.clear();
            tx.signatures.clear();

            comment_operation comment_op;
            comment_op.author = "alice";
            comment_op.permlink = "test";
            comment_op.parent_permlink = "test";
            comment_op.title = "foo";
            comment_op.body = "bar";
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(comment_op);
            tx.sign(alice_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);

            tx.operations.clear();
            tx.signatures.clear();

            generate_blocks(db->head_block_time() + STEEMIT_MIN_VOTE_INTERVAL_SEC + STEEMIT_BLOCK_INTERVAL);

            vote_operation vote_op;
            vote_op.voter = "bob";
            vote_op.author = "alice";
            vote_op.permlink = "test";
            vote_op.weight = STEEMIT_100_PERCENT;
            tx.operations.push_back(vote_op);
            tx.sign(bob_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);

            BOOST_TEST_MESSAGE("-- Getting old VDO vesting_shares before cashout");

            const auto& vdo_idx = db->get_index<vesting_delegation_index>().indices().get<by_delegation>();
            auto vdo_itr = vdo_idx.find(std::make_tuple(account_name_type("carol"), account_name_type("bob")));
            BOOST_CHECK(vdo_itr != vdo_idx.end());
            auto old_vdo_gests = vdo_itr->vesting_shares;

            tx.set_expiration(db->head_block_time() + STEEMIT_BLOCK_INTERVAL);
            generate_blocks(db->get_comment("alice", string("test")).cashout_time - STEEMIT_BLOCK_INTERVAL, true);

            auto& alice_comment = db->get_comment("alice", string("test"));

            comment_fund total_comment_fund(*db);
            comment_reward alice_comment_reward(*db, total_comment_fund, alice_comment);

            generate_block();

            BOOST_TEST_MESSAGE("-- Checking total funds");

            auto& gpo = db->get_dynamic_global_properties();

            APPROX_CHECK_EQUAL(gpo.total_vesting_shares.to_real(), total_comment_fund.vesting_shares().to_real(), 10);
            APPROX_CHECK_EQUAL(gpo.total_vesting_fund_steem.amount.value, total_comment_fund.vesting_fund().amount.value, 1);

            BOOST_TEST_MESSAGE("-- Checking bob delegatee payout");

            auto& bob_account = db->get_account("bob");
            auto ops = get_last_operations<curation_reward_operation>(1);
            auto vop_curation = ops[0];
            BOOST_CHECK_EQUAL(vop_curation.reward, alice_comment_reward.vote_payout(bob_account));

            BOOST_TEST_MESSAGE("-- Checking new VDO vesting_shares == old VDO vesting_shares + delegator_payout");

            auto carol_payout = alice_comment_reward.delegator_payout("carol");
            vdo_itr = vdo_idx.find(std::make_tuple(account_name_type("carol"), account_name_type("bob")));
            BOOST_CHECK(vdo_itr != vdo_idx.end());
            BOOST_CHECK_EQUAL(vdo_itr->vesting_shares, old_vdo_gests + carol_payout);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_SUITE_END() // delegation


    BOOST_AUTO_TEST_SUITE(account_metadata)

    BOOST_AUTO_TEST_CASE(account_metadata_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_metadata_validate");

            BOOST_TEST_MESSAGE("--- Test success under normal conditions");
            account_metadata_operation op;
            op.account = "bob";
            op.json_metadata = "{}";
            CHECK_OP_VALID(op);
            CHECK_PARAM_VALID(op, json_metadata, "{\"a\":\"тест\"}")

            BOOST_TEST_MESSAGE("--- Test failure when bad account name passed");
            CHECK_PARAM_INVALID(op, account, "-bad-");

            BOOST_TEST_MESSAGE("--- Test failure when json_metadata is empty");
            CHECK_PARAM_INVALID(op, json_metadata, "");

            BOOST_TEST_MESSAGE("--- Test failure when json_metadata is invalid JSON");
            CHECK_PARAM_INVALID(op, json_metadata, "{test:fail}");
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_metadata_authorities) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_metadata_authorities");
            account_metadata_operation op;
            op.account = "bob";
            op.json_metadata = "{}";
            CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
        }
        FC_LOG_AND_RETHROW()
    }

    // WARNING: it should be before another metadata apply tests
    BOOST_AUTO_TEST_CASE(account_metadata_apply_store_for_all) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_metadata_apply_store_for_all");

            // Do not set any settings related to storing of account metadata
            // and it should store for all.

            signed_transaction tx;
            ACTOR(alice);

            BOOST_TEST_MESSAGE("--- Test success under normal conditions");
            generate_blocks(10);
            const auto json = "{\"test\":1}";
            auto now = db->head_block_time();
            account_metadata_operation op;
            op.account = "alice";
            op.json_metadata = json;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            generate_blocks(10);

            auto alice_acc = db->get_account("alice");
            auto meta = db->get<account_metadata_object, by_account>("alice");
            BOOST_CHECK_EQUAL(meta.account, "alice");
            BOOST_CHECK_EQUAL(meta.json_metadata, json);
            BOOST_CHECK_EQUAL(alice_acc.last_account_update, now);

            BOOST_TEST_MESSAGE("----- Test API");
            account_api_object alice_api(alice_acc, *db);
            BOOST_CHECK_EQUAL(alice_api.json_metadata, json);

            BOOST_TEST_MESSAGE("--- Test existance of account_metadata_object after account_create");
            // bob is created before all metadata storing settings
            // therefore it should have account_metadata_object
            ACTOR(bob);                                             // create_account with json_metadata = ""
            meta = db->get<account_metadata_object, by_account>("bob"); // just checks presence, throws on fail
            BOOST_CHECK_EQUAL(meta.account, "bob");
            BOOST_CHECK_EQUAL(meta.json_metadata, "");

            BOOST_TEST_MESSAGE("--- Test existance of account_metadata_object after account_create_with_delegation");
            generate_blocks(1);
            fund("bob", ASSET_GOLOS(1000));
            private_key_type priv_key = generate_private_key("temp_key");
            account_create_with_delegation_operation cr;
            cr.fee = ASSET_GOLOS(1000);
            cr.delegation = ASSET_GESTS(0);
            cr.creator = "bob";
            cr.new_account_name = "sam";
            cr.owner = authority(1, priv_key.get_public_key(), 1);
            cr.active = authority(1, priv_key.get_public_key(), 1);
            cr.memo_key = priv_key.get_public_key();
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cr));

            meta = db->get<account_metadata_object, by_account>("sam");
            BOOST_CHECK_EQUAL(meta.account, "sam");
            BOOST_CHECK_EQUAL(meta.json_metadata, "");
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_metadata_apply_store_for_nobody) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_metadata_apply_store_for_nobody");

            db->set_store_account_metadata(database::store_metadata_for_nobody);
            // Add account to list to check restriction works
            std::vector<std::string> accs_v;
            accs_v.push_back("alice");
            accs_v.push_back("sam");
            db->set_accounts_to_store_metadata(accs_v);

            signed_transaction tx;
            ACTOR(alice);

            BOOST_TEST_MESSAGE("--- Test success under normal conditions");
            generate_blocks(10);
            const auto json = "{\"test\":1}";
            auto now = db->head_block_time();
            account_metadata_operation op;
            op.account = "alice";
            op.json_metadata = json;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            generate_blocks(10);

            auto alice_acc = db->get_account("alice");
            auto meta = db->find<account_metadata_object, by_account>("alice");
            BOOST_CHECK(meta == nullptr);
            BOOST_CHECK_EQUAL(alice_acc.last_account_update, now);

            BOOST_TEST_MESSAGE("----- Test API");
            account_api_object alice_api(alice_acc, *db);
            BOOST_CHECK_EQUAL(alice_api.json_metadata, "");

            ACTOR(bob);                                             // create_account with json_metadata = ""

            BOOST_TEST_MESSAGE("--- Test existance of account_metadata_object after account_create_with_delegation");
            generate_blocks(1);
            fund("bob", ASSET_GOLOS(1000));
            private_key_type priv_key = generate_private_key("temp_key");
            account_create_with_delegation_operation cr;
            cr.fee = ASSET_GOLOS(1000);
            cr.delegation = ASSET_GESTS(0);
            cr.creator = "bob";
            cr.new_account_name = "sam";
            cr.owner = authority(1, priv_key.get_public_key(), 1);
            cr.active = authority(1, priv_key.get_public_key(), 1);
            cr.memo_key = priv_key.get_public_key();
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cr));

            meta = db->find<account_metadata_object, by_account>("sam");
            BOOST_CHECK(meta == nullptr);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(account_metadata_apply_store_for_listed) {
        try {
            BOOST_TEST_MESSAGE("Testing: account_metadata_apply_store_for_listed");

            db->set_store_account_metadata(database::store_metadata_for_listed);
            // Add account to list to check restriction works
            std::vector<std::string> accs_v;
            accs_v.push_back("sam");
            db->set_accounts_to_store_metadata(accs_v);

            signed_transaction tx;
            ACTOR(alice);

            BOOST_TEST_MESSAGE("--- Test success under normal conditions");
            generate_blocks(10);
            const auto json = "{\"test\":1}";
            auto now = db->head_block_time();
            account_metadata_operation op;
            op.account = "alice";
            op.json_metadata = json;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
            generate_blocks(10);

            auto alice_acc = db->get_account("alice");
            auto meta = db->find<account_metadata_object, by_account>("alice");
            BOOST_CHECK(meta == nullptr);
            BOOST_CHECK_EQUAL(alice_acc.last_account_update, now);

            BOOST_TEST_MESSAGE("----- Test API");
            account_api_object alice_api(alice_acc, *db);
            BOOST_CHECK_EQUAL(alice_api.json_metadata, "");

            ACTOR(bob);                                             // create_account with json_metadata = ""

            BOOST_TEST_MESSAGE("--- Test existance of account_metadata_object after account_create_with_delegation");
            generate_blocks(1);
            fund("bob", ASSET_GOLOS(1000));
            private_key_type priv_key = generate_private_key("temp_key");
            account_create_with_delegation_operation cr;
            cr.fee = ASSET_GOLOS(1000);
            cr.delegation = ASSET_GESTS(0);
            cr.creator = "bob";
            cr.new_account_name = "sam";
            cr.owner = authority(1, priv_key.get_public_key(), 1);
            cr.active = authority(1, priv_key.get_public_key(), 1);
            cr.memo_key = priv_key.get_public_key();
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cr));

            meta = db->find<account_metadata_object, by_account>("sam");
            BOOST_CHECK(meta != nullptr);
            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_SUITE_END() // account_metadata


    BOOST_AUTO_TEST_SUITE(referral)

    BOOST_AUTO_TEST_CASE(referral_validate_create_account) {
        try {
            BOOST_TEST_MESSAGE("Testing: referral_validate_create_account");

            BOOST_TEST_MESSAGE("--- Test failure with non-GOLOS break fee");

            auto testref_private_key = generate_private_key("temp_key");

            account_create_with_delegation_operation cr;
            cr.fee = ASSET_GOLOS(1000);
            cr.delegation = ASSET_GESTS(0);
            cr.creator = "bob";
            cr.new_account_name = "testref";
            cr.owner = authority(1, testref_private_key.get_public_key(), 1);
            cr.active = authority(1, testref_private_key.get_public_key(), 1);
            cr.posting = authority(1, testref_private_key.get_public_key(), 1);
            cr.memo_key = testref_private_key.get_public_key();

            account_referral_options aro;
            aro.referrer = "bob";
            aro.interest_rate = 900;
            auto end_date = db->head_block_time() + GOLOS_DEFAULT_REFERRAL_TERM_SEC;
            aro.end_date = end_date;
            aro.break_fee = asset(100, VESTS_SYMBOL);
            cr.extensions.insert(aro);

            GOLOS_CHECK_ERROR_PROPS(cr.validate(),
                CHECK_ERROR(invalid_parameter, "break_fee"));

            BOOST_TEST_MESSAGE("--- Test failure with negative break fee");

            aro.break_fee = asset(-100, STEEM_SYMBOL);

            cr.extensions.clear();
            cr.extensions.insert(aro);

            GOLOS_CHECK_ERROR_PROPS(cr.validate(),
                CHECK_ERROR(invalid_parameter, "break_fee"));

            BOOST_TEST_MESSAGE("--- Test success with correct break fee");

            aro.break_fee = asset(100, STEEM_SYMBOL);

            cr.extensions.clear();
            cr.extensions.insert(aro);

            CHECK_OP_VALID(cr);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(referral_create_account_comment) {
        try {
            BOOST_TEST_MESSAGE("Testing: referral_create_account_comment");

            ACTOR(bob);
            generate_blocks(1);

            fund("bob", ASSET_GOLOS(1000));

            BOOST_TEST_MESSAGE("--- Test creation of referral account");

            auto testref_private_key = generate_private_key("temp_key");

            account_create_with_delegation_operation cr;
            cr.fee = ASSET_GOLOS(1000);
            cr.delegation = ASSET_GESTS(0);
            cr.creator = "bob";
            cr.new_account_name = "testref";
            cr.owner = authority(1, testref_private_key.get_public_key(), 1);
            cr.active = authority(1, testref_private_key.get_public_key(), 1);
            cr.posting = authority(1, testref_private_key.get_public_key(), 1);
            cr.memo_key = testref_private_key.get_public_key();

            account_referral_options aro;
            aro.referrer = "bob";
            aro.interest_rate = 900;
            auto end_date = db->head_block_time() + GOLOS_DEFAULT_REFERRAL_TERM_SEC;
            aro.end_date = end_date;
            aro.break_fee = asset(100, STEEM_SYMBOL);
            cr.extensions.insert(aro);

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cr));
            generate_blocks(1);

            const auto& testref_acc = db->get_account("testref");
            BOOST_CHECK_EQUAL(testref_acc.referrer_account, "bob");
            BOOST_CHECK_EQUAL(testref_acc.referrer_interest_rate, 900);
            BOOST_CHECK_EQUAL(testref_acc.referral_end_date, end_date);
            BOOST_CHECK_EQUAL(testref_acc.referral_break_fee.amount, 100);

            fund("testref", ASSET_GOLOS(100));

            BOOST_TEST_MESSAGE("--- Test posting of comment by referral");

            tx.operations.clear();
            tx.signatures.clear();

            comment_operation co;
            co.author = "testref";
            co.permlink = "foo";
            co.title = "bar";
            co.body = "foo bar";
            co.parent_author = "";
            co.parent_permlink = "ipsum";

            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(co);

            tx.sign(testref_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);
            generate_blocks(1);

            const auto& testref_com = db->get_comment("testref", string("foo"));
            const auto& referrer_find = std::find_if(testref_com.beneficiaries.begin(),
                    testref_com.beneficiaries.end(), [&testref_acc](const beneficiary_route_type& benef) {
                return benef.account == testref_acc.referrer_account;
            });
            BOOST_CHECK(referrer_find != testref_com.beneficiaries.end());

            BOOST_TEST_MESSAGE("--- Test breaking of referral account");

            tx.operations.clear();
            tx.signatures.clear();

            break_free_referral_operation op;
            op.referral = "testref";

            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            tx.sign(testref_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);

            const auto& testref_free_acc = db->get_account("testref");
            BOOST_CHECK_EQUAL(testref_free_acc.referrer_account, account_name_type());
            BOOST_CHECK_EQUAL(testref_free_acc.referrer_interest_rate, 0);
            BOOST_CHECK_EQUAL(testref_free_acc.referral_end_date, time_point_sec::min());
            BOOST_CHECK_EQUAL(testref_free_acc.referral_break_fee.amount, 0);

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(referral_create_break_account_comment) {
        try {
            BOOST_TEST_MESSAGE("Testing: referral_create_break_account_comment");

            ACTOR(bob);
            generate_blocks(1);

            fund("bob", ASSET_GOLOS(1000));

            BOOST_TEST_MESSAGE("--- Test creation of referral account");

            auto testref_private_key = generate_private_key("temp_key");

            account_create_with_delegation_operation cr;
            cr.fee = ASSET_GOLOS(1000);
            cr.delegation = ASSET_GESTS(0);
            cr.creator = "bob";
            cr.new_account_name = "testref";
            cr.owner = authority(1, testref_private_key.get_public_key(), 1);
            cr.active = authority(1, testref_private_key.get_public_key(), 1);
            cr.posting = authority(1, testref_private_key.get_public_key(), 1);
            cr.memo_key = testref_private_key.get_public_key();

            account_referral_options aro;
            aro.referrer = "bob";
            aro.interest_rate = 900;
            auto end_date = db->head_block_time() + GOLOS_DEFAULT_REFERRAL_TERM_SEC;
            aro.end_date = end_date;
            aro.break_fee = asset(100, STEEM_SYMBOL);
            cr.extensions.insert(aro);

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cr));
            generate_blocks(1);

            const auto& testref_acc = db->get_account("testref");
            BOOST_CHECK_EQUAL(testref_acc.referrer_account, "bob");
            BOOST_CHECK_EQUAL(testref_acc.referrer_interest_rate, 900);
            BOOST_CHECK_EQUAL(testref_acc.referral_end_date, end_date);
            BOOST_CHECK_EQUAL(testref_acc.referral_break_fee.amount, 100);

            fund("testref", ASSET_GOLOS(100));

            BOOST_TEST_MESSAGE("--- Test breaking of referral account");

            tx.operations.clear();
            tx.signatures.clear();

            break_free_referral_operation op;
            op.referral = "testref";

            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(op);

            tx.sign(testref_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);

            const auto& testref_free_acc = db->get_account("testref");
            BOOST_CHECK_EQUAL(testref_free_acc.referrer_account, account_name_type());
            BOOST_CHECK_EQUAL(testref_free_acc.referrer_interest_rate, 0);
            BOOST_CHECK_EQUAL(testref_free_acc.referral_end_date, time_point_sec::min());
            BOOST_CHECK_EQUAL(testref_free_acc.referral_break_fee.amount, 0);

            BOOST_TEST_MESSAGE("--- Test posting of comment by referral account after break");

            tx.operations.clear();
            tx.signatures.clear();

            comment_operation co;
            co.author = "testref";
            co.permlink = "foo2";
            co.title = "bar";
            co.body = "foo bar";
            co.parent_author = "";
            co.parent_permlink = "ipsum";

            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(co);

            tx.sign(testref_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);
            generate_blocks(1);

            const auto& testref_com2 = db->get_comment("testref", string("foo2"));
            const auto& referrer_find2 = std::find_if(testref_com2.beneficiaries.begin(),
                    testref_com2.beneficiaries.end(), [&testref_acc](const beneficiary_route_type& benef) {
                return benef.account == testref_acc.referrer_account; // Using account before break
            });
            BOOST_CHECK(referrer_find2 == testref_com2.beneficiaries.end());

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(referral_create_expire_account_comment) {
        try {
            BOOST_TEST_MESSAGE("Testing: referral_create_expire_account_comment");

            ACTOR(bob);
            generate_blocks(1);

            fund("bob", ASSET_GOLOS(1000));

            BOOST_TEST_MESSAGE("--- Test creation of referral account");

            auto testref_private_key = generate_private_key("temp_key");

            account_create_with_delegation_operation cr;
            cr.fee = ASSET_GOLOS(1000);
            cr.delegation = ASSET_GESTS(0);
            cr.creator = "bob";
            cr.new_account_name = "testref";
            cr.owner = authority(1, testref_private_key.get_public_key(), 1);
            cr.active = authority(1, testref_private_key.get_public_key(), 1);
            cr.posting = authority(1, testref_private_key.get_public_key(), 1);
            cr.memo_key = testref_private_key.get_public_key();

            account_referral_options aro;
            aro.referrer = "bob";
            aro.interest_rate = 900;
            auto end_date = db->head_block_time(); // Will expire after generating at least 1 block
            aro.end_date = end_date;
            aro.break_fee = asset(100, STEEM_SYMBOL);
            cr.extensions.insert(aro);

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cr));
            generate_blocks(1);

            const auto& testref_acc = db->get_account("testref");
            BOOST_CHECK_EQUAL(testref_acc.referrer_account, "bob");
            BOOST_CHECK_EQUAL(testref_acc.referrer_interest_rate, 900);
            BOOST_CHECK_EQUAL(testref_acc.referral_end_date, end_date);
            BOOST_CHECK_EQUAL(testref_acc.referral_break_fee.amount, 100);

            fund("testref", ASSET_GOLOS(100));

            BOOST_TEST_MESSAGE("--- Test posting of comment by referral account after expiration");

            tx.operations.clear();
            tx.signatures.clear();

            comment_operation co;
            co.author = "testref";
            co.permlink = "foo3";
            co.title = "bar";
            co.body = "foo bar";
            co.parent_author = "";
            co.parent_permlink = "ipsum";

            co.title = "bar";
            co.body = "foo bar";

            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.operations.push_back(co);

            tx.sign(testref_private_key, db->get_chain_id());
            db->push_transaction(tx, 0);
            generate_blocks(1);

            const auto& testref_com = db->get_comment("testref", string("foo3"));
            const auto& referrer_find = std::find_if(testref_com.beneficiaries.begin(),
                    testref_com.beneficiaries.end(), [&testref_acc](const beneficiary_route_type& benef) {
                return benef.account == testref_acc.referrer_account; // Using account before break
            });
            BOOST_CHECK(referrer_find == testref_com.beneficiaries.end());

            validate_database();
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_SUITE_END() // referral

    BOOST_AUTO_TEST_SUITE(comment_curation_rewards_percent) // comment_curation_rewards_percent

    BOOST_AUTO_TEST_CASE(comment_curation_rewards_percent_validate) {
        try {
            BOOST_TEST_MESSAGE("Testing: comment_curation_rewards_percent_validate");
            comment_options_operation op;

            op.author = "alice";
            op.permlink = "test";

            BOOST_TEST_MESSAGE("--- Test more than allowed maximum");

            golos::protocol::comment_curation_rewards_percent perc;
            perc.percent = STEEMIT_MAX_CURATION_PERCENT + STEEMIT_1_PERCENT;
            op.extensions.clear();
            op.extensions.insert(perc);
            GOLOS_CHECK_ERROR_PROPS(op.validate(),
                CHECK_ERROR(invalid_parameter, "percent"));

            BOOST_TEST_MESSAGE("--- Test allowed minimum");

            perc.percent = STEEMIT_MIN_CURATION_PERCENT;
            op.extensions.clear();
            op.extensions.insert(perc);
            BOOST_CHECK_NO_THROW(op.validate());

            BOOST_TEST_MESSAGE("--- Test allowed maximum");

            perc.percent = STEEMIT_MAX_CURATION_PERCENT;
            op.extensions.clear();
            op.extensions.insert(perc);
            BOOST_CHECK_NO_THROW(op.validate());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(comment_curation_rewards_percent_apply) {
        try {
            BOOST_TEST_MESSAGE("Testing: comment_curation_rewards_percent_apply");
            ACTORS((alice))
            generate_block();

            auto& wso = db->get_witness_schedule_object();
            BOOST_CHECK_EQUAL(wso.median_props.min_curation_percent, STEEMIT_DEF_CURATION_PERCENT);
            BOOST_CHECK_EQUAL(wso.median_props.max_curation_percent, STEEMIT_DEF_CURATION_PERCENT);

            fund("alice", 10000);
            vest("alice", 10000);

            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            comment_operation comment;
            comment.author = "alice";
            comment.permlink = "test";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "foobar";

            signed_transaction tx;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            comment_options_operation op;
            op.author = "alice";
            op.permlink = "test";

            golos::protocol::comment_curation_rewards_percent perc;

            BOOST_TEST_MESSAGE("--- Test less than allowed minimum");

            perc.percent = wso.median_props.min_curation_percent - STEEMIT_1_PERCENT;
            op.extensions.clear();
            op.extensions.insert(perc);

            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "percent")));

            BOOST_TEST_MESSAGE("--- Test more than allowed maximum");

            perc.percent = wso.median_props.max_curation_percent + STEEMIT_1_PERCENT;
            op.extensions.clear();
            op.extensions.insert(perc);

            GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
                CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "percent")));

            BOOST_TEST_MESSAGE("--- Test allowed value");

            perc.percent = wso.median_props.max_curation_percent;
            op.extensions.clear();
            op.extensions.insert(perc);

            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_SUITE_END() // comment_curation_rewards_percent

BOOST_AUTO_TEST_SUITE_END()


struct votes_extended_fixture : public golos::chain::clean_database_fixture {

    void initialize(const plugin_options& opts = {}) {
        database_fixture::initialize(opts);
        open_database();
        startup();
    }

    fc::ecc::private_key vote_key;
    uint32_t current_voter = 0;
    static const uint32_t cashout_blocks = STEEMIT_CASHOUT_WINDOW_SECONDS / STEEMIT_BLOCK_INTERVAL;

    void generate_voters(uint32_t n) {
        fc::ecc::private_key private_key = generate_private_key("test");
        fc::ecc::private_key post_key = generate_private_key("test_post");
        vote_key = post_key;
        for (uint32_t i = 0; i < n; i++) {
            const auto name = "voter" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, private_key.get_public_key(), post_key.get_public_key()));
        }
        generate_block();
        validate_database();
    }

    void vote_sequence(const std::string& author, const std::string& permlink, uint32_t n_votes, uint32_t interval = 0) {
        uint32_t end = current_voter + n_votes;
        for (; current_voter < end; current_voter++) {
            const auto name = "voter" + std::to_string(current_voter);
            vote_operation op;
            op.voter = name;
            op.author = author;
            op.permlink = permlink;
            op.weight = STEEMIT_100_PERCENT;
            signed_transaction tx;
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, vote_key, op));
            if (interval > 0) {
                generate_blocks(interval);
            }
        }
        validate_database();
    }

    void post(const std::string& permlink = "post", const std::string& parent_permlink = "test") {
        ACTOR(alice);
        comment_operation op;
        op.author = "alice";
        op.permlink = permlink;
        op.parent_author = parent_permlink == "test" ? "" : "alice";
        op.parent_permlink = parent_permlink;
        op.title = "foo";
        op.body = "bar";
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
        validate_database();
    }

    uint32_t count_stored_votes() {
        const auto n = db->get_index<golos::chain::comment_vote_index>().indices().size();
        return n;
    }
};

BOOST_FIXTURE_TEST_SUITE(auction_window_tests, votes_extended_fixture)

    BOOST_AUTO_TEST_SUITE(auction_window)

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_reward_fund) {
        try {
            BOOST_TEST_MESSAGE("Test changing auction window size");

            db_plugin->debug_update([=](database &db) {
                auto &wso = db.get_witness_schedule_object();
                BOOST_CHECK_EQUAL(wso.median_props.auction_window_size, 1800);

                db.modify(wso, [&](witness_schedule_object &w) {
                    w.median_props.auction_window_size = 10 * 60;
                });

                BOOST_CHECK_EQUAL(wso.median_props.auction_window_size, 600);
            }, database::skip_witness_signature);

            BOOST_TEST_MESSAGE("Auction window reward goes to reward fund.");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_reward_fund;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment(comment.author, comment.permlink);
            generate_voters(20);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);
            generate_blocks((alice_post.created + alice_post.auction_window_size), true);
            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_reward_fund_empty_case) {
        try {
            BOOST_TEST_MESSAGE("Auction window is 0.");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw2";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_reward_fund;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            generate_blocks((alice_post.created + alice_post.auction_window_size), true);
            vote_sequence(comment.author, comment.permlink, voters_count, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_reward_fund_all_in_auw) {
        try {
            BOOST_TEST_MESSAGE("Auction window reward goes to reward fund. All votes made in auw");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw3";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_reward_fund;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count, 5);
            generate_blocks((alice_post.created + alice_post.auction_window_size), true);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }


    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_curators_case_empty_auw) {
        try {
            BOOST_TEST_MESSAGE("Auction window is 0. The to_curators case.");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw4";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_curators;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            generate_blocks((alice_post.created + alice_post.auction_window_size), true);
            vote_sequence(comment.author, comment.permlink, voters_count, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }

            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_curators_case_all_in_auw) {
        try {
            BOOST_TEST_MESSAGE("Auction window reward goes to reward fund. The to_curators case");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 10;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw5";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_curators;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count, 5);
            generate_blocks((alice_post.created + alice_post.auction_window_size), true);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);
            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(auction_window_tokens_to_curators_case) {
        try {
            BOOST_TEST_MESSAGE("Auction window reward goes to curators.");

            // Needed to be sured, that auction window's been already enabled.
            generate_blocks(fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME), true);

            auto &wso = db->get_witness_schedule_object();
            db->modify(wso, [&](witness_schedule_object &w) {
                w.median_props.auction_window_size = 10 * 60;
            });

            generate_block();

            ACTOR(alice);
            int voters_count = 6;

            comment_operation comment;
            comment_options_operation cop;
            signed_transaction tx;
            set_price_feed(price(ASSET("1.000 GOLOS"), ASSET("1.000 GBG")));

            BOOST_TEST_MESSAGE("Create a post.");
            comment.author = "alice";
            comment.permlink = "testauw6";
            comment.parent_permlink = "test";
            comment.title = "test";
            comment.body = "Let's test auction window improvements!";

            tx.operations.clear();
            tx.signatures.clear();
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, comment));
            generate_block();

            protocol::comment_auction_window_reward_destination dest;
            dest.destination = protocol::to_curators;

            cop.author = comment.author;
            cop.permlink = comment.permlink;

            cop.extensions.insert(dest);

            tx.operations.clear();
            tx.signatures.clear();

            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
            generate_block();
            auto& alice_post = db->get_comment(comment.author, comment.permlink);
            generate_voters(voters_count);

            BOOST_TEST_MESSAGE("Create votes.");
            BOOST_CHECK_EQUAL(alice_post.cashout_time, alice_post.created + STEEMIT_CASHOUT_WINDOW_SECONDS);

            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);
            generate_blocks((alice_post.created + alice_post.auction_window_size), true);
            vote_sequence(comment.author, comment.permlink, voters_count / 2, 5);

            generate_blocks((alice_post.cashout_time - STEEMIT_BLOCK_INTERVAL), true);

            comment_fund total_comment_fund(*db);
            comment_reward alice_post_reward(*db, total_comment_fund, alice_post);

            generate_block();

            share_type rwd = 0;
            for (int i = 0; i < voters_count; i++) {
                std::string acc_name = "voter" + std::to_string(i);
                auto& account = db->get_account(acc_name);
                rwd += account.curation_rewards;
            }
            auto& alice_acc = db->get_account(alice_post.author);

            BOOST_CHECK_EQUAL(rwd.value, alice_post_reward.total_curators_reward());
            BOOST_CHECK_EQUAL(alice_acc.posting_rewards.value, alice_post_reward.total_author_reward());
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_SUITE_END() // auction_window
BOOST_AUTO_TEST_SUITE_END()
#endif
