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

    ACTORS((alice)(bob))
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

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_proposal_apply_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_apply_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "i-am-post", "", "i-am-post");

    worker_proposal_operation op;
    op.author = "alice";
    op.permlink = "i-am-post";
    op.type = worker_proposal_type::task;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto& wpo_post = db->get_comment("alice", string("i-am-post"));
    const auto* wpo = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(wpo);
    BOOST_CHECK(wpo->type == worker_proposal_type::task);

    BOOST_TEST_MESSAGE("-- Modifying worker proposal");

    op.type = worker_proposal_type::premade_work;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto* wpo_mod = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(wpo_mod);
    BOOST_CHECK(wpo_mod->type == worker_proposal_type::premade_work);
}

BOOST_AUTO_TEST_CASE(worker_proposal_delete_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_proposal_delete_apply");

    ACTORS((alice))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Deleting worker proposal with non-existant post case");

    worker_proposal_delete_operation op;
    op.author = "alice";
    op.permlink = "i-am-post";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("alice", "i-am-post"), alice_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting non-existant worker proposal case");

    comment_create("alice", alice_private_key, "i-am-post", "", "i-am-post");

    GOLOS_CHECK_ERROR_MISSING(worker_proposal_object, make_comment_id("alice", "i-am-post"), alice_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking cannot delete post with worker proposal");

    worker_proposal("alice", alice_private_key, "i-am-post", worker_proposal_type::task);
    generate_block();

    const auto& wpo_post = db->get_comment("alice", string("i-am-post"));
    const auto* wpo = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(wpo);

    delete_comment_operation dcop;
    dcop.author = "alice";
    dcop.permlink = "i-am-post";
    GOLOS_CHECK_ERROR_LOGIC(cannot_delete_post_with_worker_proposal, alice_private_key, dcop);
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting worker proposal");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    const auto* wpo_del = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(!wpo_del);

    BOOST_TEST_MESSAGE("-- Checking can delete post now");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, dcop));
    generate_block();

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
