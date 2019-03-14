#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/protocol/worker_operations.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_techspec_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    {
        worker_techspec_operation op;
        op.author = "bob";
        op.permlink = "bob-techspec";
        op.worker_proposal_author = "alice";
        op.worker_proposal_permlink = "alice-proposal";
        op.specification_cost = ASSET_GOLOS(6000);
        op.development_cost = ASSET_GOLOS(60000);
        op.payments_interval = 60;
        op.payments_count = 2;
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_techspec_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-techspec";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_techspec_approve_operation op;
        op.approver = "cyberfounder";
        op.author = "bob";
        op.permlink = "bob-techspec";
        op.state = worker_techspec_approve_state::approve;
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"cyberfounder"}));
    }

    {
        worker_assign_operation op;
        op.assigner = "bob";
        op.worker_techspec_author = "bob";
        op.worker_techspec_permlink = "bob-techspec";
        op.worker = "alice";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));

        op.worker = "";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }
}

BOOST_AUTO_TEST_CASE(worker_techspec_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "techspec-permlink";
    op.worker_proposal_author = "alice";
    op.worker_proposal_permlink = "proposal-permlink";
    op.specification_cost = ASSET_GOLOS(6000);
    op.development_cost = ASSET_GOLOS(60000);
    op.payments_interval = 60*60*24;
    op.payments_count = 2;
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect author or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
    CHECK_PARAM_INVALID(op, worker_proposal_author, "");
    CHECK_PARAM_INVALID(op, worker_proposal_permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));

    BOOST_TEST_MESSAGE("-- Non-GOLOS cost case");

    CHECK_PARAM_INVALID(op, specification_cost, ASSET_GBG(6000));
    CHECK_PARAM_INVALID(op, specification_cost, ASSET_GESTS(6000));
    CHECK_PARAM_INVALID(op, development_cost, ASSET_GBG(60000));
    CHECK_PARAM_INVALID(op, development_cost, ASSET_GESTS(60000));

    BOOST_TEST_MESSAGE("-- Negative cost case");

    CHECK_PARAM_INVALID(op, specification_cost, ASSET_GOLOS(-1));
    CHECK_PARAM_INVALID(op, development_cost, ASSET_GOLOS(-1));

    BOOST_TEST_MESSAGE("-- Zero payments count case");

    CHECK_PARAM_INVALID(op, payments_count, 0);

    BOOST_TEST_MESSAGE("-- Too low payments interval case");

    CHECK_PARAM_INVALID(op, payments_interval, 60*60*24 - 1);

    BOOST_TEST_MESSAGE("-- Single payment with too big interval case");

    op.payments_count = 1;
    CHECK_PARAM_INVALID(op, payments_interval, 60*60*24 + 1);

    BOOST_TEST_MESSAGE("-- Single payment with normal interval case");

    op.payments_count = 1;
    CHECK_PARAM_VALID(op, payments_interval, 60*60*24);
}

BOOST_AUTO_TEST_CASE(worker_techspec_delete_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_delete_apply");

    ACTORS((alice)(bob)(approver))
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "alice-proposal", "", "alice-proposal");

    worker_proposal("alice", alice_private_key, "alice-proposal", worker_proposal_type::task);
    generate_block();

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
    generate_block();

    witness_create("approver", approver_private_key, "foo.bar", approver_private_key.get_public_key(), 1000);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Approving worker techspec (after abstain)");

    worker_techspec_approve_operation wtaop;
    wtaop.approver = "approver";
    wtaop.author = "bob";
    wtaop.permlink = "bob-techspec";
    wtaop.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_private_key, wtaop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting worker techspec with approves");

    worker_techspec_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto* wto = db->find_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
    BOOST_CHECK(wto);
    BOOST_CHECK(wto->state == worker_techspec_state::closed_by_author);

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
