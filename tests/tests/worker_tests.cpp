#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_tests, clean_database_fixture)

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

    op.type = worker_proposal_type::_size;
    GOLOS_CHECK_ERROR_PROPS(op.validate(),
        CHECK_ERROR(invalid_parameter, "type"));
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
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(missing_object, "comment", make_comment_id("alice", "fake"))));
    generate_block();

    BOOST_TEST_MESSAGE("-- Create worker proposal on comment instead of post case");

    comment_operation cop;
    cop.title = "test";
    cop.body = "test";
    cop.author = "alice";
    cop.permlink = "i-am-post";
    cop.parent_author = "";
    cop.parent_permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    cop.author = "bob";
    cop.permlink = "i-am-comment";
    cop.parent_author = "alice";
    cop.parent_permlink = "i-am-post";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop));
    validate_database();

    op.author = "bob";
    op.permlink = "i-am-comment";
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, op),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_exception::worker_proposal_can_be_created_only_on_post)));
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
    const auto* wpo = db->find_worker_proposal(wpo_post.id);
    BOOST_CHECK(wpo);

    BOOST_TEST_MESSAGE("-- Checking cannot delete post with worker proposal");

    delete_comment_operation dcop;
    dcop.author = "alice";
    dcop.permlink = "i-am-post";
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, dcop),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_exception::cannot_delete_post_with_worker_proposal)));
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting worker proposal");

    worker_proposal_delete_operation op;
    op.author = "alice";
    op.permlink = "i-am-post";
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
