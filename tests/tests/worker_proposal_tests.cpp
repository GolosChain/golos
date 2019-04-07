#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_proposal_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    worker_proposal_operation proposal_op;
    proposal_op.author = "alice";
    proposal_op.permlink = "test";
    CHECK_OP_AUTHS(proposal_op, account_name_set(), account_name_set(), account_name_set({"alice"}));

    worker_proposal_delete_operation proposal_del_op;
    proposal_del_op.author = "alice";
    proposal_del_op.permlink = "test";
    CHECK_OP_AUTHS(proposal_del_op, account_name_set(), account_name_set(), account_name_set({"alice"}));
}

BOOST_AUTO_TEST_CASE(worker_proposal_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_proposal_operation op;
    op.author = "alice";
    op.permlink = "test";
    op.type = worker_proposal_type::premade_work;
    BOOST_CHECK_NO_THROW(op.validate());

    BOOST_TEST_MESSAGE("-- Invalid type case");

    CHECK_PARAM_INVALID(op, type, worker_proposal_type::_size);
}

BOOST_AUTO_TEST_CASE(worker_proposal_apply_create) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_apply_create");

    ACTORS((alice)(bob)(carol))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Create worker proposal with no post case");

    worker_proposal_operation op;
    op.author = "alice";
    op.permlink = "fake";
    op.type = worker_proposal_type::task;
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("alice", "fake"), alice_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Create worker proposal on comment instead of post case");

    comment_create("alice", alice_private_key, "i-am-post", "", "i-am-post");

    comment_create("bob", bob_private_key, "i-am-comment", "alice", "i-am-post");

    validate_database();

    op.author = "bob";
    op.permlink = "i-am-comment";
    GOLOS_CHECK_ERROR_LOGIC(worker_proposal_can_be_created_only_on_post, bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Normal create worker proposal case");

    op.author = "alice";
    op.permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto& wpo_post = db->get_comment("alice", string("i-am-post"));
    const auto* wpo = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(wpo);
    BOOST_CHECK(wpo->type == worker_proposal_type::task);
    BOOST_CHECK(wpo->state == worker_proposal_state::created);

    {
        BOOST_TEST_MESSAGE("-- Check cannot create worker proposal on post outside cashout window");

        comment_create("carol", carol_private_key, "carol-proposal", "", "carol-proposal");

        generate_blocks(db->head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS + STEEMIT_BLOCK_INTERVAL, true);

        op.author = "carol";
        op.permlink = "carol-proposal";
        GOLOS_CHECK_ERROR_LOGIC(post_should_be_in_cashout_window, carol_private_key, op);
    }

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_proposal_apply_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_apply_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal_operation op;
    op.author = "alice";
    op.permlink = "alice-proposal";
    op.type = worker_proposal_type::premade_work;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto& wpo_post = db->get_comment("alice", string("alice-proposal"));
    const auto* wpo = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(wpo);
    BOOST_CHECK(wpo->type == worker_proposal_type::premade_work);

    BOOST_TEST_MESSAGE("-- Modifying worker proposal");

    op.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto* wpo_mod = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(wpo_mod);
    BOOST_CHECK(wpo_mod->type == worker_proposal_type::task);

    BOOST_TEST_MESSAGE("-- Check cannot modify worker proposal with approved techspec");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_techspec_approve_operation wtaop;
        wtaop.approver = name;
        wtaop.author = "bob";
        wtaop.permlink = "bob-techspec";
        wtaop.state = worker_techspec_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, wtaop));
        generate_block();
    }

    op.type = worker_proposal_type::task;
    GOLOS_CHECK_ERROR_LOGIC(cannot_edit_worker_proposal_with_techspecs, alice_private_key, op);
}

BOOST_AUTO_TEST_CASE(worker_proposal_delete_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_delete_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_proposal_delete_operation op;
    op.author = "alice";
    op.permlink = "alice-techspec";
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
}

BOOST_AUTO_TEST_CASE(worker_proposal_delete_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_delete_apply");

    ACTORS((alice)(bob)(carol))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Checking cannot delete worker proposal with not-exist post");

    worker_proposal_delete_operation op;
    op.author = "alice";
    op.permlink = "i-am-post";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("alice", "i-am-post"), alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Checking cannot delete not-exist proposal");

    comment_create("alice", alice_private_key, "i-am-post", "", "i-am-post");

    GOLOS_CHECK_ERROR_MISSING(worker_proposal_object, make_comment_id("alice", "i-am-post"), alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Creating worker proposal");

    worker_proposal("alice", alice_private_key, "i-am-post", worker_proposal_type::task);
    generate_block();

    {
        const auto* wpo = db->find_worker_proposal(db->get_comment("alice", string("i-am-post")).id);
        BOOST_CHECK(wpo);
    }

    BOOST_TEST_MESSAGE("-- Checking cannot delete post with worker proposal");

    delete_comment_operation dcop;
    dcop.author = "alice";
    dcop.permlink = "i-am-post";
    GOLOS_CHECK_ERROR_LOGIC(cannot_delete_post_with_worker_proposal, alice_private_key, dcop);

    BOOST_TEST_MESSAGE("-- Deleting worker proposal");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    {
        const auto* wpo = db->find_worker_proposal(db->get_comment("alice", string("i-am-post")).id);
        BOOST_CHECK(!wpo);
    }

    BOOST_TEST_MESSAGE("-- Checking cannot delete deleted worker proposal");

    GOLOS_CHECK_ERROR_MISSING(worker_proposal_object, make_comment_id("alice", "i-am-post"), alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Checking can delete post now");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, dcop));
    generate_block();

    {
        BOOST_TEST_MESSAGE("-- Checking cannot delete worker proposal with techspec");

        comment_create("bob", bob_private_key, "i-am-post", "", "i-am-post");

        worker_proposal("bob", bob_private_key, "i-am-post", worker_proposal_type::task);
        generate_block();

        comment_create("carol", carol_private_key, "carol-techspec", "", "carol-techspec");

        worker_techspec_operation wtop;
        wtop.author = "carol";
        wtop.permlink = "carol-techspec";
        wtop.worker_proposal_author = "bob";
        wtop.worker_proposal_permlink = "i-am-post";
        wtop.specification_cost = ASSET_GOLOS(6);
        wtop.development_cost = ASSET_GOLOS(60);
        wtop.payments_interval = 60*60*24*2;
        wtop.payments_count = 2;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
        generate_block();

        op.author = "bob";
        op.permlink = "i-am-post";
        GOLOS_CHECK_ERROR_LOGIC(cannot_delete_worker_proposal_with_techspecs, bob_private_key, op);
    }

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
