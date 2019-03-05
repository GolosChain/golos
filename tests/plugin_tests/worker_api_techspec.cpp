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

BOOST_FIXTURE_TEST_SUITE(worker_api_plugin_techspec, worker_api_fixture)

BOOST_AUTO_TEST_CASE(worker_techspec_create) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_create");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal_operation wpop;
    wpop.author = "alice";
    wpop.permlink = "alice-proposal";
    wpop.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wpop));
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
    op.payments_interval = 60;
    op.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());
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

    worker_proposal_operation wpop;
    wpop.author = "alice";
    wpop.permlink = "alice-proposal";
    wpop.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wpop));
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.worker_proposal_author = "alice";
    op.worker_proposal_permlink = "alice-proposal";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60;
    op.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->modified, fc::time_point_sec::min());
    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, 0);

    BOOST_TEST_MESSAGE("-- Voting worker techspec post");

    vote_operation vop;
    vop.voter = "alice";
    vop.author = "bob";
    vop.permlink = "bob-techspec";
    vop.weight = STEEMIT_100_PERCENT;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, vop));
    generate_block();

    const auto& wto_post_voted = db->get_comment("bob", string("bob-techspec"));
    wtmo_itr = wtmo_idx.find(wto_post_voted.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->net_rshares, wto_post_voted.net_rshares);

    BOOST_TEST_MESSAGE("-- Modifying worker techspec");

    auto now = db->head_block_time();

    op.payments_count = 3;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->modified, now);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_delete) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_delete");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal_operation wpop;
    wpop.author = "alice";
    wpop.permlink = "alice-proposal";
    wpop.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wpop));
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());

    BOOST_TEST_MESSAGE("-- Deleting worker techspec");

    worker_techspec_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr == wtmo_idx.end());

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_combinations) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_combinations");

    ACTORS((alice)(bob)(approver))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal_operation wpop;
    wpop.author = "alice";
    wpop.permlink = "alice-proposal";
    wpop.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wpop));
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    witness_create("approver", approver_private_key, "foo.bar", approver_private_key.get_public_key(), 1000);

    generate_block();

    BOOST_TEST_MESSAGE("-- Approving worker techspec (after abstain)");

    db->modify(db->get_witness("approver"), [&](witness_object& witness) {
        witness.schedule = witness_object::top19;
    });

    auto old_approves = wtmo_itr->approves;
    auto old_disapproves = wtmo_itr->disapproves;

    worker_techspec_approve_operation op;
    op.approver = "approver";
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, old_approves + 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, old_disapproves);
    old_approves = wtmo_itr->approves;
    old_disapproves = wtmo_itr->disapproves;

    BOOST_TEST_MESSAGE("-- Disapproving worker techspec (after approve)");

    op.state = worker_techspec_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, old_approves - 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, old_disapproves + 1);
    old_approves = wtmo_itr->approves;
    old_disapproves = wtmo_itr->disapproves;

    BOOST_TEST_MESSAGE("-- Abstaining worker techspec (after disapprove)");

    op.state = worker_techspec_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, old_approves);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, old_disapproves - 1);
    old_approves = wtmo_itr->approves;
    old_disapproves = wtmo_itr->disapproves;

    generate_block();

    BOOST_TEST_MESSAGE("-- Disapproving worker techspec (after abstain)");

    db->modify(db->get_witness("approver"), [&](witness_object& witness) {
        witness.schedule = witness_object::top19;
    });

    op.state = worker_techspec_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, old_approves);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, old_disapproves + 1);
    old_approves = wtmo_itr->approves;
    old_disapproves = wtmo_itr->disapproves;

    BOOST_TEST_MESSAGE("-- Approving worker techspec (after disapprove)");

    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, old_approves + 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, old_disapproves - 1);
    old_approves = wtmo_itr->approves;
    old_disapproves = wtmo_itr->disapproves;

    BOOST_TEST_MESSAGE("-- Abstaining worker techspec (after approve)");

    op.state = worker_techspec_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, old_approves - 1);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, old_disapproves);
    old_approves = wtmo_itr->approves;
    old_disapproves = wtmo_itr->disapproves;

    generate_block();

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_top19_updating) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_top19_updating");

    ACTORS((alice)(bob)(approver)(approver2)(approver3))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal_operation wpop;
    wpop.author = "alice";
    wpop.permlink = "alice-proposal";
    wpop.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wpop));
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK(wtmo_itr != wtmo_idx.end());
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 0);
    BOOST_CHECK_EQUAL(wtmo_itr->disapproves, 0);

    BOOST_TEST_MESSAGE("-- Approving worker techspec by approver1");

    witness_create("approver", approver_private_key, "foo.bar", approver_private_key.get_public_key(), 1000);

    db->modify(db->get_witness("approver"), [&](witness_object& witness) {
        witness.schedule = witness_object::top19;
    });

    worker_techspec_approve_operation op;
    op.approver = "approver";
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 1);

    BOOST_TEST_MESSAGE("-- Removing approver1 from top19 and approving worker techspec by approver2, approver3");

    generate_block();

    db->modify(db->get_witness("approver"), [&](witness_object& witness) {
        witness.schedule = witness_object::miner;
    });

    witness_create("approver2", approver2_private_key, "foo.bar", approver2_private_key.get_public_key(), 1000);

    witness_create("approver3", approver3_private_key, "foo.bar", approver3_private_key.get_public_key(), 1000);

    db->modify(db->get_witness("approver2"), [&](witness_object& witness) {
        witness.schedule = witness_object::top19;
    });

    db->modify(db->get_witness("approver3"), [&](witness_object& witness) {
        witness.schedule = witness_object::top19;
    });

    op.approver = "approver2";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver2_private_key, op));

    op.approver = "approver3";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver3_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->approves, 2);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_assign) {
    BOOST_TEST_MESSAGE("Testing: worker_assign");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    const auto& wtmo_idx = db->get_index<worker_techspec_metadata_index, by_post>();

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal_operation wpop;
    wpop.author = "alice";
    wpop.permlink = "alice-proposal";
    wpop.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wpop));
    generate_block();

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    generate_block();

    BOOST_TEST_MESSAGE("-- Approving worker techspec by witnesses");

    auto private_key = generate_private_key("test");
    auto post_key = generate_private_key("test_post");
    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        GOLOS_CHECK_NO_THROW(account_create(name, private_key.get_public_key(), post_key.get_public_key()));
        GOLOS_CHECK_NO_THROW(witness_create(name, private_key, "foo.bar", private_key.get_public_key(), 1000));

        db->modify(db->get_witness(name), [&](witness_object& witness) {
            witness.schedule = witness_object::top19;
        });

        worker_techspec_approve_operation wtaop;
        wtaop.approver = name;
        wtaop.author = "bob";
        wtaop.permlink = "bob-techspec";
        wtaop.state = worker_techspec_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, wtaop));
    }

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    auto wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, time_point_sec::min());

    BOOST_TEST_MESSAGE("-- Assigning worker");

    auto now = db->head_block_time();

    worker_assign_operation op;
    op.assigner = "bob";
    op.worker_techspec_author = "bob";
    op.worker_techspec_permlink = "bob-techspec";
    op.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, now);

    BOOST_TEST_MESSAGE("-- Unassigning worker");

    op.worker = "";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));

    wtmo_itr = wtmo_idx.find(wto_post.id);
    BOOST_CHECK_EQUAL(wtmo_itr->work_beginning_time, time_point_sec::min());

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
