#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/plugins/worker_api/worker_api_plugin.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::plugins::worker_api;

struct worker_api_fixture : public golos::chain::database_fixture {
    worker_api_fixture() : golos::chain::database_fixture() {
        initialize<golos::plugins::worker_api::worker_api_plugin>();
        open_database();
        startup();
    }
};

BOOST_FIXTURE_TEST_SUITE(worker_api_plugin, worker_api_fixture)

BOOST_AUTO_TEST_CASE(worker_proposal_create) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_create");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wpmo_idx = db->get_index<worker_proposal_metadata_index, by_post>();

    comment_operation cop;
    cop.title = "test";
    cop.body = "test";
    cop.author = "alice";
    cop.permlink = "i-am-post";
    cop.parent_author = "";
    cop.parent_permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));

    vote_operation vop;
    vop.voter = "bob";
    vop.author = "alice";
    vop.permlink = "i-am-post";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, vop));
    generate_block();

    auto now = db->head_block_time();

    worker_proposal_operation op;
    op.author = "alice";
    op.permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto& wpo_post = db->get_comment("alice", string("i-am-post"));
    auto wpmo_itr = wpmo_idx.find(wpo_post.id);
    BOOST_CHECK(wpmo_itr != wpmo_idx.end());
    BOOST_CHECK_EQUAL(wpmo_itr->created, now);
    BOOST_CHECK_EQUAL(wpmo_itr->modified, fc::time_point_sec::min());
    BOOST_CHECK_EQUAL(wpmo_itr->net_rshares, wpo_post.net_rshares);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_proposal_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wpmo_idx = db->get_index<worker_proposal_metadata_index, by_post>();

    comment_operation cop;
    cop.title = "test";
    cop.body = "test";
    cop.author = "alice";
    cop.permlink = "i-am-post";
    cop.parent_author = "";
    cop.parent_permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));

    worker_proposal_operation op;
    op.author = "alice";
    op.permlink = "i-am-post";
    op.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto& wpo_post = db->get_comment("alice", string("i-am-post"));
    auto wpmo_itr = wpmo_idx.find(wpo_post.id);
    BOOST_CHECK(wpmo_itr != wpmo_idx.end());
    BOOST_CHECK_EQUAL(wpmo_itr->modified, fc::time_point_sec::min());
    BOOST_CHECK_EQUAL(wpmo_itr->net_rshares, 0);

    BOOST_TEST_MESSAGE("-- Voting worker proposal post");

    vote_operation vop;
    vop.voter = "bob";
    vop.author = "alice";
    vop.permlink = "i-am-post";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, vop));
    generate_block();

    const auto& wpo_post_voted = db->get_comment("alice", string("i-am-post"));
    wpmo_itr = wpmo_idx.find(wpo_post_voted.id);
    BOOST_CHECK(wpmo_itr != wpmo_idx.end());
    BOOST_CHECK_EQUAL(wpmo_itr->net_rshares, wpo_post_voted.net_rshares);

    BOOST_TEST_MESSAGE("-- Modifying worker proposal");

    auto now = db->head_block_time();

    op.type = worker_proposal_type::premade_work;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    wpmo_itr = wpmo_idx.find(wpo_post.id);
    BOOST_CHECK(wpmo_itr != wpmo_idx.end());
    BOOST_CHECK_EQUAL(wpmo_itr->modified, now);
}

BOOST_AUTO_TEST_CASE(worker_proposal_delete) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_delete");

    ACTORS((alice))
    generate_block();

    signed_transaction tx;

    const auto& wpmo_idx = db->get_index<worker_proposal_metadata_index, by_post>();

    comment_operation cop;
    cop.title = "test";
    cop.body = "test";
    cop.author = "alice";
    cop.permlink = "i-am-post";
    cop.parent_author = "";
    cop.parent_permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();

    worker_proposal_operation wpop;
    wpop.author = "alice";
    wpop.permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wpop));
    generate_block();

    const auto& wpo_post = db->get_comment("alice", string("i-am-post"));
    auto wpmo_itr = wpmo_idx.find(wpo_post.id);
    BOOST_CHECK(wpmo_itr != wpmo_idx.end());

    BOOST_TEST_MESSAGE("-- Deleting worker proposal");

    worker_proposal_delete_operation op;
    op.author = "alice";
    op.permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    wpmo_itr = wpmo_idx.find(wpo_post.id);
    BOOST_CHECK(wpmo_itr == wpmo_idx.end());

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
