#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/plugins/worker_api/worker_api_plugin.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::plugins::worker_api;

struct worker_api_fixture : public golos::chain::worker_fixture {
    worker_api_fixture() : golos::chain::worker_fixture() {
        database_fixture::initialize<worker_api_plugin>();
        open_database();
        startup();
    }
};

BOOST_FIXTURE_TEST_SUITE(worker_api_plugin_techspec, worker_api_fixture)

BOOST_AUTO_TEST_CASE(worker_techspec_create) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_create");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    vote_operation vop;
    vop.voter = "alice";
    vop.author = "bob";
    vop.permlink = "bob-techspec";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, vop));
    generate_block();

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.worker_proposal_author = "alice";
    op.worker_proposal_permlink = "alice-proposal";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking metadata creating");

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Checking metadata has NOT filled field modified");

    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Checking metadata has field net_rshares filled from post");

    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, wto_post.net_rshares);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.worker_proposal_author = "alice";
    op.worker_proposal_permlink = "alice-proposal";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Modifying worker techspec");

    auto now = db->head_block_time();

    op.payments_count = 3;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    wtmo_itr = wtmo_idx.find(wto_post.id);

    BOOST_TEST_MESSAGE("-- Checking metadata has updated field modified");

    BOOST_CHECK_EQUAL(wtmo_itr->modified, now);

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
