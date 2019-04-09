#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

fc::variant_object make_worker_result_id(const std::string& author, const std::string& permlink) {
    auto res = fc::mutable_variant_object()("account",author)("worker_result_permlink",permlink);
    return fc::variant_object(res);
}

BOOST_FIXTURE_TEST_SUITE(worker_result_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    {
        worker_result_operation op;
        op.author = "bob";
        op.permlink = "bob-result";
        op.worker_techspec_permlink = "bob-techspec";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_result_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-result";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }
}

BOOST_AUTO_TEST_CASE(worker_result_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_result_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_result_operation op;
    op.author = "bob";
    op.permlink = "bob-result";
    op.worker_techspec_permlink = "bob-techspec";
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
    CHECK_PARAM_INVALID(op, worker_techspec_permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
}

BOOST_AUTO_TEST_CASE(worker_result_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_result_apply");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");
    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    BOOST_TEST_MESSAGE("-- Create result on not-exist post");

    worker_result_operation op;
    op.author = "bob";
    op.permlink = "bob-result";
    op.worker_techspec_permlink = "bob-techspec";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-result"), bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Create result on comment instead of post");

    comment_create("bob", bob_private_key, "i-am-comment", "alice", "alice-proposal");

    op.permlink = "i-am-comment";
    GOLOS_CHECK_ERROR_LOGIC(worker_result_can_be_created_only_on_post, bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Create result for techspec with not-exist post");

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL); // Waiting for posts window

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");

    op.permlink = "bob-result";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-techspec"), bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Create result for not-exist techspec");

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL); // Waiting for posts window

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    GOLOS_CHECK_ERROR_MISSING(worker_techspec_object, make_comment_id("bob", "bob-techspec"), bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Creating techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*30;
    wtop.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Create result on post used for techspec");

    op.permlink = "bob-techspec";
    GOLOS_CHECK_ERROR_LOGIC(this_post_already_used_as_worker_techspec, bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Create result for techspec not in work");

    op.permlink = "bob-result";
    GOLOS_CHECK_ERROR_LOGIC(worker_result_can_be_created_only_for_techspec_in_work, bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Normal create result");

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "alice");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking created result");

    {
        const auto& worker_result_post = db->get_comment("bob", string("bob-result"));
        const auto& wto = db->get_worker_result(worker_result_post.id);
        BOOST_CHECK(wto.state == worker_techspec_state::complete);
    }

    BOOST_TEST_MESSAGE("-- Create result on post already used for result");

    GOLOS_CHECK_ERROR_LOGIC(this_post_already_used_as_worker_result, bob_private_key, op);
}

BOOST_AUTO_TEST_CASE(worker_result_delete_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_result_delete_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_result_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-result";
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
}

BOOST_AUTO_TEST_CASE(worker_result_delete_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_result_delete_apply");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Checking cannot delete result with not-exist post");

    worker_result_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-result";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-result"), bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Checking cannot delete not-exist result");

    comment_create("bob", bob_private_key, "bob-result", "", "bob-result");

    GOLOS_CHECK_ERROR_MISSING(worker_techspec_object, make_worker_result_id("bob", "bob-result"), bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Creating result");

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL); // Waiting for posts window

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.worker_proposal_author = "alice";
    wtop.worker_proposal_permlink = "alice-proposal";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        worker_techspec_approve("approver" + std::to_string(i), private_key,
            "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();
    }

    worker_assign("bob", bob_private_key, "bob", "bob-techspec", "alice");

    worker_result("bob",  bob_private_key, "bob-result", "bob-techspec");

    BOOST_TEST_MESSAGE("-- Checking cannot delete post with result");

    delete_comment_operation dcop;
    dcop.author = "bob";
    dcop.permlink = "bob-result";
    GOLOS_CHECK_ERROR_LOGIC(cannot_delete_post_with_worker_result, bob_private_key, dcop);
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting worker result");

    op.permlink = "bob-result";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto* worker_result = db->find_worker_result(db->get_comment("bob", string("bob-result")).id);
        BOOST_CHECK(!worker_result);
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(wto.state == worker_techspec_state::wip);
    }

    BOOST_TEST_MESSAGE("-- Checking cannot delete deleted result");

    GOLOS_CHECK_ERROR_MISSING(worker_techspec_object, make_worker_result_id("bob", "bob-result"), bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Checking can delete post after result deleted");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, dcop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking cannot delete result for closed techspec");

    {
        BOOST_TEST_MESSAGE("---- Creating result for techspec");

        generate_blocks(60 / STEEMIT_BLOCK_INTERVAL); // Waiting for posts window

        comment_create("bob", bob_private_key, "bob-result", "", "bob-result");
        worker_result("bob",  bob_private_key, "bob-result", "bob-techspec");

        BOOST_TEST_MESSAGE("---- Closing techspec by author");

        worker_techspec_delete_operation wtdop;
        wtdop.author = "bob";
        wtdop.permlink = "bob-techspec";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtdop));
        generate_block();

        BOOST_TEST_MESSAGE("---- Checking cannot delete result");

        GOLOS_CHECK_ERROR_LOGIC(cannot_delete_worker_result_for_paying_techspec, bob_private_key, op);
    }
}

BOOST_AUTO_TEST_SUITE_END()
