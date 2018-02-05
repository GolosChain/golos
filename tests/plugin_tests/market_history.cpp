#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>

#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/comment_object.hpp>
#include <golos/protocol/operations/steem_operations.hpp>

#include <golos/market_history/market_history_plugin.hpp>

#include "../common/database_fixture.hpp"

using namespace golos::chain;
using namespace golos::protocol;

typedef asset<0, 17, 0> latest_asset;

BOOST_FIXTURE_TEST_SUITE(market_history, clean_database_fixture)

    BOOST_AUTO_TEST_CASE(mh_test) {
        using namespace golos::market_history;

        try {
            auto mh_plugin = app.register_plugin<market_history_plugin>();
            boost::program_options::variables_map options;
            mh_plugin->plugin_initialize(options);

            ACTORS((alice)(bob)(sam));
            fund("alice", 1000000);
            fund("bob", 1000000);
            fund("sam", 1000000);

            set_price_feed(price<0, 17, 0>(latest_asset::from_string("0.500 TESTS"), latest_asset::from_string("1.000 TBD")));

            signed_transaction tx;
            comment_operation<0, 17, 0> comment;
            comment.author = "alice";
            comment.permlink = "test";
            comment.parent_permlink = "test";
            comment.title = "foo";
            comment.body = "bar";
            tx.operations.push_back(comment);

            vote_operation<0, 17, 0> vote;
            vote.voter = "alice";
            vote.weight = STEEMIT_100_PERCENT;
            vote.author = "alice";
            vote.permlink = "test";
            tx.operations.push_back(vote);

            tx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db.get_chain_id());
            db.push_transaction(tx, 0);

            generate_blocks(db.get_comment("alice", std::string("test")).cashout_time);

            const auto &bucket_idx = db.get_index<bucket_index>().indices().get<by_key>();
            const auto &order_hist_idx = db.get_index<order_history_index>().indices().get<golos::chain::by_id>();

            BOOST_REQUIRE(bucket_idx.begin() == bucket_idx.end());
            BOOST_REQUIRE(order_hist_idx.begin() == order_hist_idx.end());
            validate_database();

            tx.operations.clear();
            tx.signatures.clear();

            auto fill_order_a_time = db.head_block_time();
            auto time_a = fc::time_point_sec((fill_order_a_time.sec_since_epoch() / 15) * 15);

            limit_order_create_operation<0, 17, 0> op;
            op.owner = "alice";
            op.amount_to_sell = latest_asset::from_string("1.000 TBD");
            op.min_to_receive = latest_asset::from_string("2.000 TESTS");
            tx.operations.push_back(op);
            tx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db.get_chain_id());
            db.push_transaction(tx, 0);

            tx.operations.clear();
            tx.signatures.clear();

            op.owner = "bob";
            op.amount_to_sell = latest_asset::from_string("1.500 TESTS");
            op.min_to_receive = latest_asset::from_string("0.750 TBD");
            tx.operations.push_back(op);
            tx.sign(bob_private_key, db.get_chain_id());
            db.push_transaction(tx, 0);

            generate_blocks(db.head_block_time() + (60 * 90));

            auto fill_order_b_time = db.head_block_time();

            tx.operations.clear();
            tx.signatures.clear();

            op.owner = "sam";
            op.amount_to_sell = latest_asset::from_string("1.000 TESTS");
            op.min_to_receive = latest_asset::from_string("0.500 TBD");
            tx.operations.push_back(op);
            tx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(sam_private_key, db.get_chain_id());
            db.push_transaction(tx, 0);

            generate_blocks(db.head_block_time() + 60);

            auto fill_order_c_time = db.head_block_time();

            tx.operations.clear();
            tx.signatures.clear();

            op.owner = "alice";
            op.amount_to_sell = latest_asset::from_string("0.500 TBD");
            op.min_to_receive = latest_asset::from_string("0.900 TESTS");
            tx.operations.push_back(op);
            tx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(alice_private_key, db.get_chain_id());
            db.push_transaction(tx, 0);

            tx.operations.clear();
            tx.signatures.clear();

            op.owner = "bob";
            op.amount_to_sell = latest_asset::from_string("0.450 TESTS");
            op.min_to_receive = latest_asset::from_string("0.250 TBD");
            tx.operations.push_back(op);
            tx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            tx.sign(bob_private_key, db.get_chain_id());
            db.push_transaction(tx, 0);
            validate_database();

            auto bucket = bucket_idx.begin();

            BOOST_REQUIRE(bucket->key.seconds == 15);
            BOOST_REQUIRE(bucket->key.open == time_a);

            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("1.500 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.750 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 15);
            BOOST_REQUIRE(bucket->key.open == time_a + (60 * 90));
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("0.500 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.250 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 15);
            BOOST_REQUIRE(bucket->key.open == time_a + (60 * 90) + 60);
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("0.450 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("0.450 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("0.950 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.500 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 60);
            BOOST_REQUIRE(bucket->key.open == time_a);
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("1.500 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.750 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 60);
            BOOST_REQUIRE(bucket->key.open == time_a + (60 * 90));
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("0.500 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.250 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 60);
            BOOST_REQUIRE(bucket->key.open == time_a + (60 * 90) + 60);
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("0.450 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("0.450 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("0.950 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.500 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 300);
            BOOST_REQUIRE(bucket->key.open == time_a);
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("1.500 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.750 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 300);
            BOOST_REQUIRE(bucket->key.open == time_a + (60 * 90));
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("0.450 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("0.450 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("1.450 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.750 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 3600);
            BOOST_REQUIRE(bucket->key.open == time_a);
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("1.500 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.750 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 3600);
            BOOST_REQUIRE(bucket->key.open == time_a + (60 * 60));
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("0.450 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("0.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("0.450 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("1.450 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("0.750 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket->key.seconds == 86400);
            BOOST_REQUIRE(bucket->key.open == STEEMIT_GENESIS_TIME);
            BOOST_REQUIRE(bucket->high_base == latest_asset::from_string("0.450 TESTS ").amount);
            BOOST_REQUIRE(bucket->high_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->low_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->low_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->open_base == latest_asset::from_string("1.500 TESTS").amount);
            BOOST_REQUIRE(bucket->open_quote == latest_asset::from_string("0.750 TBD").amount);
            BOOST_REQUIRE(bucket->close_base == latest_asset::from_string("0.450 TESTS").amount);
            BOOST_REQUIRE(bucket->close_quote == latest_asset::from_string("0.250 TBD").amount);
            BOOST_REQUIRE(bucket->base_volume == latest_asset::from_string("2.950 TESTS").amount);
            BOOST_REQUIRE(bucket->quote_volume == latest_asset::from_string("1.500 TBD").amount);
            bucket++;

            BOOST_REQUIRE(bucket == bucket_idx.end());

            auto order = order_hist_idx.begin();

            BOOST_REQUIRE(order->time == fill_order_a_time);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_owner == "bob");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_pays == latest_asset::from_string("1.500 TESTS"));
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_owner == "alice");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_pays == latest_asset::from_string("0.750 TBD"));
            order++;

            BOOST_REQUIRE(order->time == fill_order_b_time);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_owner == "sam");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_pays == latest_asset::from_string("0.500 TESTS"));
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_owner == "alice");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_pays == latest_asset::from_string("0.250 TBD"));
            order++;

            BOOST_REQUIRE(order->time == fill_order_c_time);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_owner == "alice");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_pays == latest_asset::from_string("0.250 TBD"));
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_owner == "sam");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_pays == latest_asset::from_string("0.500 TESTS"));
            order++;

            BOOST_REQUIRE(order->time == fill_order_c_time);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_owner == "bob");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().current_pays == latest_asset::from_string("0.450 TESTS"));
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_owner == "alice");
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_order_id == 0);
            BOOST_REQUIRE(order->op.get<typename BOOST_IDENTITY_TYPE((fill_order_operation<0, 17, 0>))>().open_pays == latest_asset::from_string("0.250 TBD"));
            order++;

            BOOST_REQUIRE(order == order_hist_idx.end());
        } FC_LOG_AND_RETHROW()
    }

BOOST_AUTO_TEST_SUITE_END()
#endif