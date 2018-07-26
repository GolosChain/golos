#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <golos/protocol/exceptions.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/hardfork.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/proposal_object.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>


// TODO: move somewhere globally accessible
#define GOLOS_PROPOSAL_MAX_TITLE_SIZE   256
#define GOLOS_PROPOSAL_MAX_MEMO_SIZE    4096


using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;
using std::string;

BOOST_FIXTURE_TEST_SUITE(proposal_tests, clean_database_fixture)


// validate + authority tests
BOOST_AUTO_TEST_CASE(proposal_create_validate) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_create_validate");

    BOOST_TEST_MESSAGE("--- success on valid parameters");
    transfer_operation top;
    top.from = "alice";
    top.to = "bob";
    top.amount = ASSET_GBG(1.0);

    proposal_create_operation op;
    op.author = "alice";
    op.title = "test";
    op.expiration_time = db->head_block_time() + fc::hours(6);
    op.review_period_time = db->head_block_time() + fc::hours(3);
    op.proposed_operations.push_back(operation_wrapper(top));

    CHECK_OP_VALID(op);
    CHECK_PARAM_VALID(op, title, "-");
    CHECK_PARAM_VALID(op, title, string(GOLOS_PROPOSAL_MAX_TITLE_SIZE, ' '));
    CHECK_PARAM_VALID(op, title, u8"тест");
    CHECK_PARAM_VALID(op, memo, string(GOLOS_PROPOSAL_MAX_MEMO_SIZE, ' '));
    CHECK_PARAM_VALID(op, memo, u8"тест");

    BOOST_TEST_MESSAGE("--- failure when author is empty");
    CHECK_PARAM_INVALID(op, author, "");

    BOOST_TEST_MESSAGE("--- failure when title is not valid");
    CHECK_PARAM_INVALID(op, title, "");
    CHECK_PARAM_INVALID(op, title, string(1+GOLOS_PROPOSAL_MAX_TITLE_SIZE, ' '));
    CHECK_PARAM_INVALID(op, title, "\xc3\x28");

    BOOST_TEST_MESSAGE("--- failure when memo is not valid");
    CHECK_PARAM_INVALID(op, memo, string(1+GOLOS_PROPOSAL_MAX_MEMO_SIZE, ' '));
    CHECK_PARAM_INVALID(op, memo, "\xc3\x28");

    BOOST_TEST_MESSAGE("--- failure when proposed_operations is empty");
    proposal_create_operation e;
    CHECK_PARAM_INVALID(op, proposed_operations, e.proposed_operations);

    BOOST_TEST_MESSAGE("--- failure when proposed_operations contains invalid op");
    auto ops = op.proposed_operations;
    transfer_operation t2;
    t2.from = "alice";
    t2.to = "bob";
    t2.amount = ASSET_GESTS(1.0);
    ops.push_back(operation_wrapper(t2));
    // validation fails inside internal transaction, so error contains "amount" field
    CHECK_PARAM_VALIDATION_FAIL(op, proposed_operations, ops,
        CHECK_ERROR(invalid_parameter, "amount"));

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(proposal_create_authorities) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_create_authorities");
    proposal_create_operation op;
    op.author = "bob";
    op.title = "test";
    CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"bob"}), account_name_set());
} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_CASE(proposal_update_validate) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_update_validate");

    using approvals = flat_set<account_name_type>;

    BOOST_TEST_MESSAGE("--- success on valid parameters");
    proposal_update_operation op;
    op.author = "alice";
    op.title = "test";
    op.owner_approvals_to_add = approvals({"bob"});
    CHECK_OP_VALID(op);
    CHECK_PARAM_VALID(op, title, "-");
    CHECK_PARAM_VALID(op, title, string(GOLOS_PROPOSAL_MAX_TITLE_SIZE, ' '));
    CHECK_PARAM_VALID(op, title, u8"тест");
    CHECK_PARAM_VALID(op, active_approvals_to_add, approvals({"cid", "dave"}));

    BOOST_TEST_MESSAGE("--- failure when author is empty");
    CHECK_PARAM_INVALID(op, author, "");

    BOOST_TEST_MESSAGE("--- failure when title is not valid");
    CHECK_PARAM_INVALID(op, title, "");
    CHECK_PARAM_INVALID(op, title, string(1+GOLOS_PROPOSAL_MAX_TITLE_SIZE, ' '));
    CHECK_PARAM_INVALID(op, title, "\xc3\x28");

    BOOST_TEST_MESSAGE("--- failure when no approvals");
    CHECK_PARAM_INVALID_LOGIC(op, owner_approvals_to_add, approvals(), empty_approvals);

    BOOST_TEST_MESSAGE("--- failure when adding and deleting same approval");
    CHECK_PARAM_INVALID_LOGIC(op, owner_approvals_to_remove, approvals({"bob"}), add_and_remove_same_approval);

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_CASE(proposal_update_authorities) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_update_authorities");
    account_name_set nobody;
    account_name_set bob_acc = account_name_set({"bob"});
    account_name_set cid_acc = account_name_set({"cid"});
    account_name_set bob_and_cid = account_name_set({"bob", "cid"});

    proposal_update_operation op;
    op.author = "alice";
    op.title = "test";

    BOOST_TEST_MESSAGE("--- owner_approvals_to_add and owner_approvals_to_remove");
    op.owner_approvals_to_add = bob_acc;
    CHECK_OP_AUTHS(op, bob_acc, nobody, nobody);
    op.owner_approvals_to_remove = cid_acc;
    CHECK_OP_AUTHS(op, bob_and_cid, nobody, nobody);
    op.owner_approvals_to_add = nobody;
    CHECK_OP_AUTHS(op, cid_acc, nobody, nobody);
    op.owner_approvals_to_remove = nobody;

    BOOST_TEST_MESSAGE("--- active_approvals_to_add and active_approvals_to_remove");
    op.active_approvals_to_add = bob_acc;
    CHECK_OP_AUTHS(op, nobody, bob_acc, nobody);
    op.active_approvals_to_remove = cid_acc;
    CHECK_OP_AUTHS(op, nobody, bob_and_cid, nobody);
    op.active_approvals_to_add = nobody;
    CHECK_OP_AUTHS(op, nobody, cid_acc, nobody);
    op.active_approvals_to_remove = nobody;

    BOOST_TEST_MESSAGE("--- poting_approvals_to_add and poting_approvals_to_remove");
    op.posting_approvals_to_add = bob_acc;
    CHECK_OP_AUTHS(op, nobody, nobody, bob_acc);
    op.posting_approvals_to_remove = cid_acc;
    CHECK_OP_AUTHS(op, nobody, nobody, bob_and_cid);
    op.posting_approvals_to_add = nobody;
    CHECK_OP_AUTHS(op, nobody, nobody, cid_acc);
    op.posting_approvals_to_remove = nobody;

    // TODO: keys?

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_CASE(proposal_delete_validate) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_delete_validate");

    BOOST_TEST_MESSAGE("--- success on valid parameters");
    proposal_delete_operation op;
    op.requester = "bob";
    op.author = "alice";
    op.title = "test";
    CHECK_OP_VALID(op);
    CHECK_PARAM_VALID(op, title, "-");
    CHECK_PARAM_VALID(op, title, string(GOLOS_PROPOSAL_MAX_TITLE_SIZE, ' '));
    CHECK_PARAM_VALID(op, title, u8"тест");

    BOOST_TEST_MESSAGE("--- failure when requester or author is empty");
    CHECK_PARAM_INVALID(op, requester, "");
    CHECK_PARAM_INVALID(op, author, "");

    BOOST_TEST_MESSAGE("--- failure when title is not valid");
    CHECK_PARAM_INVALID(op, title, "");
    CHECK_PARAM_INVALID(op, title, string(GOLOS_PROPOSAL_MAX_TITLE_SIZE+1, ' '));
    CHECK_PARAM_INVALID(op, title, "\xc3\x28");

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(proposal_delete_authorities) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_delete_authorities");
    proposal_delete_operation op;
    op.requester = "bob";
    op.author = "alice";
    op.title = "test";
    CHECK_OP_AUTHS(op, account_name_set(), account_name_set({"bob"}), account_name_set());
} FC_LOG_AND_RETHROW() }


// apply tests
BOOST_AUTO_TEST_CASE(create_proposal) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_create_operation");

    signed_transaction tx;

    ACTORS((alice)(bob))
    generate_blocks(1);
    fund("alice", 10000);
    generate_blocks(1);

    BOOST_TEST_MESSAGE("--- Combination of operations with active and posting authority");

    transfer_operation top;
    top.from = "alice";
    top.to = "bob";
    top.amount = ASSET("2.500 GOLOS");

    vote_operation vop;
    vop.author = "alice";
    vop.permlink = "test";
    vop.voter = "bob";
    vop.weight = STEEMIT_100_PERCENT;

    proposal_create_operation cop;
    cop.author = "bob";
    cop.title = "Transfer with vote";
    cop.expiration_time = db->head_block_time() + fc::hours(6);
    cop.review_period_time = db->head_block_time() + fc::hours(3);
    cop.proposed_operations.push_back(operation_wrapper(top));
    cop.proposed_operations.push_back(operation_wrapper(vop));

    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, cop),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_exception::tx_with_both_posting_active_ops)));

    BOOST_TEST_MESSAGE("--- Simple proposal with a transfer operation");

    transfer_operation top1;
    top1.from = "alice";
    top1.to = "bob";
    top1.amount = ASSET("2.500 GOLOS");

    proposal_create_operation cop1;
    cop1.author = "bob";
    cop1.title = "Transfer to bob";
    cop1.memo = "Some memo about transfer";
    cop1.expiration_time = db->head_block_time() + fc::hours(6);
    cop1.review_period_time = db->head_block_time() + fc::hours(3);
    cop1.proposed_operations.push_back(operation_wrapper(top1));

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop1));
    generate_blocks(1);
    const auto& p = db->get_proposal(cop1.author, cop1.title);

    BOOST_CHECK_EQUAL(p.required_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.required_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.required_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_active_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_key_approvals.size(), 0);

    BOOST_CHECK(p.required_active_approvals.count("alice"));
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(update_proposal) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_update_operation");

    signed_transaction tx;

    ACTORS((alice)(bob))
    generate_blocks(1);
    fund("alice", 10000);
    generate_blocks(1);

    transfer_operation top;
    top.from = "alice";
    top.to = "bob";
    top.amount = ASSET("2.500 GOLOS");

    proposal_create_operation cop;
    cop.author = "bob";
    cop.title = "Transfer to bob";
    cop.memo = "Some memo about transfer";
    cop.expiration_time = db->head_block_time() + fc::hours(6);
    cop.review_period_time = db->head_block_time() + fc::hours(3);
    cop.proposed_operations.push_back(operation_wrapper(top));

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop));
    generate_blocks(1);
    const auto& p = db->get_proposal(cop.author, cop.title);

    BOOST_TEST_MESSAGE("--- Signing of a transaction with a proposal");

    proposal_update_operation uop;
    uop.author = cop.author;
    uop.title = cop.title;
    uop.active_approvals_to_add.insert("alice");

    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, uop),
        CHECK_ERROR(tx_missing_active_auth, 0));

    proposal_update_operation uop1;
    uop1.author = cop.author;
    uop1.title = cop.title;
    uop1.active_approvals_to_add.insert("alice");
    uop1.active_approvals_to_remove.insert("alice");

    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, uop1),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_exception::add_and_remove_same_approval)));

    proposal_update_operation uop2;
    uop2.author = cop.author;
    uop2.title = cop.title;
    uop2.key_approvals_to_add.insert(bob_public_key);

    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, uop2),
        CHECK_ERROR(tx_missing_other_auth, 0));

    BOOST_TEST_MESSAGE("--- Add an approval to a proposal");

    proposal_update_operation uop3;
    uop3.author = cop.author;
    uop3.title = cop.title;
    uop3.active_approvals_to_add.insert("alice");
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop3));
    generate_blocks(1);

    proposal_update_operation uop4;
    uop4.author = cop.author;
    uop4.title = cop.title;
    uop4.key_approvals_to_add.insert(bob_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, uop4));
    generate_blocks(1);

    BOOST_CHECK_EQUAL(p.required_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.required_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.required_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.available_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_key_approvals.size(), 1);

    BOOST_CHECK(p.required_active_approvals.count("alice"));
    BOOST_CHECK(p.available_active_approvals.count("alice"));
    BOOST_CHECK(p.available_key_approvals.count(bob_public_key));

    BOOST_TEST_MESSAGE("--- Remove an approval from a proposal");

    proposal_update_operation uop5;
    uop5.author = cop.author;
    uop5.title = cop.title;
    uop5.active_approvals_to_remove.insert("alice");
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop5));
    generate_blocks(1);

    proposal_update_operation uop6;
    uop6.author = cop.author;
    uop6.title = cop.title;
    uop6.key_approvals_to_remove.insert(bob_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, uop6));
    generate_blocks(1);

    BOOST_CHECK_EQUAL(p.required_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.required_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.required_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_active_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_key_approvals.size(), 0);

    BOOST_CHECK(p.required_active_approvals.count("alice"));

    BOOST_TEST_MESSAGE("--- Review period of a proposal");

    proposal_update_operation uop7;
    uop7.author = cop.author;
    uop7.title = cop.title;
    uop7.active_approvals_to_add.insert("alice");
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop7));
    generate_blocks(1);

    proposal_update_operation uop8;
    uop8.author = cop.author;
    uop8.title = cop.title;
    uop8.key_approvals_to_add.insert(bob_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, uop8));
    generate_blocks(1);

    BOOST_CHECK_EQUAL(p.required_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.required_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.required_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.available_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_key_approvals.size(), 1);

    BOOST_CHECK(p.required_active_approvals.count("alice"));
    BOOST_CHECK(p.available_active_approvals.count("alice"));
    BOOST_CHECK(p.available_key_approvals.count(bob_public_key));

    generate_blocks(*p.review_period_time);

    proposal_update_operation uop9;
    uop9.author = cop.author;
    uop9.title = cop.title;
    uop9.key_approvals_to_remove.insert(bob_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, uop9));
    generate_blocks(1);

    BOOST_CHECK_EQUAL(p.required_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.required_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.required_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_active_approvals.size(), 1);
    BOOST_CHECK_EQUAL(p.available_owner_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_posting_approvals.size(), 0);
    BOOST_CHECK_EQUAL(p.available_key_approvals.size(), 0);

    BOOST_CHECK(p.required_active_approvals.count("alice"));
    BOOST_CHECK(p.available_active_approvals.count("alice"));

    proposal_update_operation uop10;
    uop10.author = cop.author;
    uop10.title = cop.title;
    uop10.active_approvals_to_add.insert("bob");
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_private_key, uop10),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_exception::cannot_add_approval_in_review_period)));

    BOOST_TEST_MESSAGE("--- Auto removing of a proposal if no approvals in a review period");
    proposal_update_operation uop11;
    uop11.author = cop.author;
    uop11.title = cop.title;
    uop11.active_approvals_to_remove.insert("alice");
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop11));
    generate_blocks(1);

    BOOST_CHECK(nullptr == db->find_proposal(cop.author, cop.title));

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(update_proposal1) { try {
    BOOST_TEST_MESSAGE("--- Auto execution of a proposal without a review period by signing of a transaction");
    signed_transaction tx;

    ACTORS((alice)(bob))
    generate_blocks(1);
    fund("alice", 10000);
    generate_blocks(1);

    transfer_operation top;
    top.from = "alice";
    top.to = "bob";
    top.amount = ASSET("2.500 GOLOS");

    proposal_create_operation cop;
    cop.author = "bob";
    cop.title = "Transfer to bob";
    cop.memo = "Some memo about transfer";
    cop.expiration_time = db->head_block_time() + fc::hours(6);
    cop.proposed_operations.push_back(operation_wrapper(top));

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop));
    generate_blocks(1);

    proposal_update_operation uop;
    uop.author = cop.author;
    uop.title = cop.title;
    uop.active_approvals_to_add.insert("alice");
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop));
    generate_blocks(1);

    BOOST_CHECK(nullptr == db->find_proposal(cop.author, cop.title));
    BOOST_CHECK_EQUAL(db->get_account("alice").balance.amount.value, 7500);
    BOOST_CHECK_EQUAL(db->get_account("bob").balance.amount.value, 2500);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(update_proposal2) { try {
    BOOST_TEST_MESSAGE("--- Auto execution of a proposal without a review period by approving with a public key");
    signed_transaction tx;

    ACTORS((alice)(bob))
    generate_blocks(1);
    fund("alice", 10000);
    generate_blocks(1);

    transfer_operation top;
    top.from = "alice";
    top.to = "bob";
    top.amount = ASSET("2.500 GOLOS");

    proposal_create_operation cop;
    cop.author = "bob";
    cop.title = "Transfer to bob";
    cop.memo = "Some memo about transfer";
    cop.expiration_time = db->head_block_time() + fc::hours(6);
    cop.proposed_operations.push_back(operation_wrapper(top));

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop));
    generate_blocks(1);

    proposal_update_operation uop;
    uop.author = cop.author;
    uop.title = cop.title;
    uop.key_approvals_to_add.insert(alice_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop));
    generate_blocks(1);

    BOOST_CHECK(nullptr == db->find_proposal(cop.author, cop.title));
    BOOST_CHECK_EQUAL(db->get_account("alice").balance.amount.value, 7500);
    BOOST_CHECK_EQUAL(db->get_account("bob").balance.amount.value, 2500);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(update_proposal3) { try {
    BOOST_TEST_MESSAGE("--- Execution of a proposal with a review period after an expiration");
    signed_transaction tx;

    ACTORS((alice)(bob))
    generate_blocks(1);
    fund("alice", 10000);
    generate_blocks(1);

    transfer_operation top;
    top.from = "alice";
    top.to = "bob";
    top.amount = ASSET("2.500 GOLOS");

    proposal_create_operation cop;
    cop.author = "bob";
    cop.title = "Transfer to bob";
    cop.memo = "Some memo about transfer";
    cop.review_period_time = db->head_block_time() + fc::hours(3);
    cop.expiration_time = db->head_block_time() + fc::hours(6);
    cop.proposed_operations.push_back(operation_wrapper(top));

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop));
    generate_blocks(1);

    proposal_update_operation uop;
    uop.author = cop.author;
    uop.title = cop.title;
    uop.key_approvals_to_add.insert(alice_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop));
    generate_blocks(1);

    BOOST_CHECK_NO_THROW(db->get_proposal(cop.author, cop.title));

    const auto& p = db->get_proposal(cop.author, cop.title);

    generate_blocks(*p.review_period_time);

    BOOST_CHECK_NO_THROW(db->get_proposal(cop.author, cop.title));

    generate_blocks(p.expiration_time);

    BOOST_CHECK(nullptr == db->find_proposal(cop.author, cop.title));
    BOOST_CHECK_EQUAL(db->get_account("alice").balance.amount.value, 7500);
    BOOST_CHECK_EQUAL(db->get_account("bob").balance.amount.value, 2500);

} FC_LOG_AND_RETHROW() }

/*
 * Simple corporate accounts:
 *
 * Well Corp.       Alice 50, Bob 50                    T=60
 * Xylo Company     Alice 30, Cindy 50                  T=40
 * Yaya Inc.        Bob 10, Dan 10, Edy 10              T=20
 * Zyzz Co.         Dan 50                              T=40
 *
 * Complex corporate accounts:
 *
 * Mega Corp.       Well 30, Yaya 30                    T=40
 * Nova Ltd.        Alice 10, Well 10                   T=20
 * Odle Intl.       Dan 10, Yaya 10, Zyzz 10, Dave 5    T=20
 * Poxx LLC         Well 10, Xylo 10, Yaya 20, Zyzz 20  T=40
 */
BOOST_AUTO_TEST_CASE(nested_signatures) { try {
    BOOST_TEST_MESSAGE("--- Multiple signatures");
    ACTORS((alice)(bob)(cindy)(dave)(dan)(edy)(mega)(nova)(odle)(poxx)(well)(xylo)(yaya)(zyzz));
    generate_blocks(1);

    auto set_auth = [&](
        account_name_type account,
        fc::ecc::private_key account_private_key,
        const authority& auth
    ) {
        signed_transaction tx;
        account_update_operation op;
        op.account = account;
        op.active = auth;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, account_private_key, op));
        generate_blocks(1);
    };

    auto get_active = [&](const string& name) {
        return authority(db->get<account_authority_object, by_account>(name).active);
    };
    auto get_owner = [&](const string& name) {
        return authority(db->get<account_authority_object, by_account>(name).owner);
    };
    auto get_posting = [&](const string& name) {
        return authority(db->get<account_authority_object, by_account>(name).posting);
    };

    flat_set< public_key_type > all_keys{
        alice_public_key, bob_public_key, cindy_public_key, dan_public_key, edy_public_key
    };

    auto check = [&](const transfer_operation& op, set<public_key_type> ref_set) {
        signed_transaction tx;
        tx.operations.push_back(op);
        static const chain_id_type chain_id = STEEMIT_CHAIN_ID;
        auto result_set = tx.get_required_signatures(chain_id, all_keys, get_active, get_owner, get_posting);
        return result_set == ref_set;
    };

    set_auth("well", well_private_key, authority(60, "alice", 50, "bob", 50));
    set_auth("xylo", xylo_private_key, authority(40, "alice", 30, "cindy", 50));
    set_auth("yaya", yaya_private_key, authority(20, "bob", 10, "dan", 10, "edy", 10));
    set_auth("zyzz", zyzz_private_key, authority(40, "dan", 50));

    set_auth("mega", mega_private_key, authority(40, "well", 30, "yaya", 30));
    set_auth("nova", nova_private_key, authority(20, "alice", 10, "well", 10));
    set_auth("odle", odle_private_key, authority(20, "dan", 10, "yaya", 10, "zyzz", 10, "dave", 5));
    set_auth("poxx", poxx_private_key, authority(40, "well", 10, "xylo", 10, "yaya", 20, "zyzz", 20));

    transfer_operation op;
    op.to = "edy";
    op.amount = ASSET("2.500 GOLOS");

    op.from = "alice";
    BOOST_CHECK(check(op, {alice_public_key}));
    op.from = "bob";
    BOOST_CHECK(check(op, {bob_public_key}));
    op.from = "well";
    BOOST_CHECK(check(op, {alice_public_key, bob_public_key}));
    op.from = "xylo";
    BOOST_CHECK(check(op, {alice_public_key, cindy_public_key}));
    op.from = "yaya";
    BOOST_CHECK(check(op, {bob_public_key, dan_public_key}));
    op.from = "zyzz";
    BOOST_CHECK(check(op, {dan_public_key}));

    op.from = "mega";
    BOOST_CHECK(check(op, {alice_public_key, bob_public_key, dan_public_key}));
    op.from = "nova";
    BOOST_CHECK(check(op, {alice_public_key, bob_public_key}));
    op.from = "odle";
    BOOST_CHECK(check(op, {bob_public_key, dan_public_key}));
    op.from = "poxx";
    BOOST_CHECK(check(op, {alice_public_key, bob_public_key, cindy_public_key, dan_public_key}));

    fund("poxx", 10000);
    generate_blocks(1);

    signed_transaction tx;
    proposal_create_operation cop;
    cop.author = "edy";
    cop.title = "Transfer to edy";
    cop.memo = "Some memo about transfer";
    cop.expiration_time = db->head_block_time() + fc::hours(6);
    cop.proposed_operations.push_back(operation_wrapper(op));
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, edy_private_key, cop));
    generate_blocks(1);

    BOOST_REQUIRE_NO_THROW(db->get_proposal(cop.author, cop.title));

    proposal_update_operation uop;
    uop.author = cop.author;
    uop.title = cop.title;
    uop.key_approvals_to_add.insert(alice_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, uop));
    generate_blocks(1);

    const auto& poxx_account = db->get_account("poxx");
    const auto& edy_account = db->get_account("edy");

    BOOST_REQUIRE_NO_THROW(db->get_proposal(cop.author, cop.title));
    BOOST_CHECK_EQUAL(poxx_account.balance.amount.value, 10000);
    BOOST_CHECK_EQUAL(edy_account.balance.amount.value, 0);

    proposal_update_operation uop1;
    uop1.author = cop.author;
    uop1.title = cop.title;
    uop1.key_approvals_to_add.insert(bob_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, uop1));
    generate_blocks(1);

    BOOST_REQUIRE_NO_THROW(db->get_proposal(cop.author, cop.title));
    BOOST_CHECK_EQUAL(poxx_account.balance.amount.value, 10000);
    BOOST_CHECK_EQUAL(edy_account.balance.amount.value, 0);

    proposal_update_operation uop2;
    uop2.author = cop.author;
    uop2.title = cop.title;
    uop2.key_approvals_to_add.insert(cindy_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, cindy_private_key, uop2));
    generate_blocks(1);

    BOOST_REQUIRE_NO_THROW(db->get_proposal(cop.author, cop.title));
    BOOST_CHECK_EQUAL(poxx_account.balance.amount.value, 10000);
    BOOST_CHECK_EQUAL(edy_account.balance.amount.value, 0);

    proposal_update_operation uop3;
    uop3.author = cop.author;
    uop3.title = cop.title;
    uop3.key_approvals_to_add.insert(dave_public_key);
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, dave_private_key, uop3),
        CHECK_ERROR(tx_invalid_operation, 0, CHECK_ERROR(tx_irrelevant_sig, 0)));

    proposal_update_operation uop4;
    uop4.author = cop.author;
    uop4.title = cop.title;
    uop4.key_approvals_to_add.insert(dan_public_key);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, dan_private_key, uop4));
    generate_blocks(1);

    BOOST_CHECK(nullptr == db->find_proposal(cop.author, cop.title));
    BOOST_CHECK_EQUAL(poxx_account.balance.amount.value, 7500);
    BOOST_CHECK_EQUAL(edy_account.balance.amount.value, 2500);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(delete_proposal) { try {
    BOOST_TEST_MESSAGE("Testing: proposal_delete_operation");
    signed_transaction tx;

    ACTORS((alice)(bob)(dave))
    generate_blocks(1);
    fund("alice", 10000);
    generate_blocks(1);

    transfer_operation top;
    top.from = "alice";
    top.to = "bob";
    top.amount = ASSET("2.500 GOLOS");

    proposal_create_operation cop;
    cop.author = "bob";
    cop.title = "Transfer to bob";
    cop.memo = "Some memo about transfer";
    cop.review_period_time = db->head_block_time() + fc::hours(3);
    cop.expiration_time = db->head_block_time() + fc::hours(6);
    cop.proposed_operations.push_back(operation_wrapper(top));

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop));
    generate_blocks(1);

    BOOST_TEST_MESSAGE("--- Unauthorized trying of delete of proposal");

    proposal_delete_operation dop;
    dop.author = cop.author;
    dop.title = cop.title;
    dop.requester = "dave";
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, dave_private_key, dop),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(logic_exception, logic_exception::proposal_delete_not_allowed)));

    BOOST_TEST_MESSAGE("--- Authorized delete of proposal");

    proposal_delete_operation dop1;
    dop1.author = cop.author;
    dop1.title = cop.title;
    dop1.requester = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, dop1));
    generate_blocks(1);

    BOOST_CHECK(nullptr == db->find_proposal(cop.author, cop.title));

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

#endif
