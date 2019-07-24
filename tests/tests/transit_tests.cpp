#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>

#include <golos/protocol/exceptions.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>

#include <golos/plugins/account_history/history_object.hpp>
#include <golos/plugins/account_history/plugin.hpp>

#include <graphene/utilities/tempdir.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;
using namespace golos::plugins;

BOOST_AUTO_TEST_SUITE(transit_tests)

    BOOST_FIXTURE_TEST_CASE(validate_transit_operation, clean_database_fixture) {
        try {
            BOOST_TEST_MESSAGE("Testing: transit_to_cyberway voting");

            ACTORS((alice))
            fund("alice", 10000);
            private_key_type signing_key = generate_private_key("new_key");

            BOOST_TEST_MESSAGE("-- register witness");
            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            witness_update_operation uop;
            uop.owner = "alice";
            uop.url = "foo.bar";
            uop.fee = ASSET("1.000 GOLOS");
            uop.block_signing_key = signing_key.get_public_key();
            uop.props.account_creation_fee = asset(STEEMIT_MIN_ACCOUNT_CREATION_FEE + 10, STEEM_SYMBOL);
            uop.props.maximum_block_size = STEEMIT_MIN_BLOCK_SIZE_LIMIT + 100;
            tx.operations.push_back(uop);

            BOOST_TEST_MESSAGE("-- vote to transit");
            transit_to_cyberway_operation top;
            top.owner = uop.owner;
            top.vote_to_transit = true;
            tx.operations.push_back(top);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("-- same vote to transit");
            tx.clear();
            tx.operations.push_back(top);
            tx.sign(alice_private_key, db->get_chain_id());
            GOLOS_CHECK_THROW_PROPS(db->push_transaction(tx, 0), tx_invalid_operation, {});

            BOOST_TEST_MESSAGE("-- remove vote to transit");
            tx.clear();
            top.vote_to_transit = false;
            tx.operations.push_back(top);
            tx.sign(alice_private_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_FIXTURE_TEST_CASE(transit_to_cyberway_event, clean_database_fixture) {
        try {
            resize_shared_mem(1024 * 1024 * 32);

            BOOST_TEST_MESSAGE("Testing: transit_to_cyberway event");

            auto witness = db->get_scheduled_witness(1);
            auto witness_priv_key = STEEMIT_INIT_PRIVATE_KEY;
            generate_block();

            BOOST_TEST_MESSAGE("-- wait STEEMIT_START_MINER_VOTING_BLOCK blocks");
            generate_blocks(STEEMIT_START_MINER_VOTING_BLOCK);

            BOOST_TEST_MESSAGE("-- vote to transit");
            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            transit_to_cyberway_operation op;
            op.owner = witness;
            op.vote_to_transit = true;
            tx.operations.push_back(op);
            tx.sign(witness_priv_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("-- save block to transit");
            generate_block();
            auto transit_block_num = db->head_block_num();

            BOOST_TEST_MESSAGE("-- wait event to transit");

            uint32_t event_block_num = 0;
            db->transit_to_cyberway.connect([&](const uint32_t n, const uint32_t skip) {
                BOOST_CHECK_GT(n, event_block_num);
                event_block_num = n;
            });

            std::size_t i;
            for (i = 0; i < 100 /* limiting waiting */ && event_block_num == 0; ++i) {
                generate_block();
            }
            BOOST_CHECK_NE(event_block_num, 0);

            BOOST_TEST_MESSAGE("-- validate transit properties");

            {
                auto& gpo = db->get_dynamic_global_properties();
                BOOST_CHECK_EQUAL(gpo.transit_witnesses[0], witness);
                for (i = 1; i < gpo.transit_witnesses.size(); ++i) {
                    BOOST_CHECK_EQUAL(gpo.transit_witnesses[i], account_name_type());
                }

                BOOST_CHECK_EQUAL(transit_block_num, gpo.transit_block_num);
                BOOST_CHECK_EQUAL(gpo.transit_block_num, gpo.last_irreversible_block_num);
                BOOST_CHECK_GT(event_block_num, gpo.last_irreversible_block_num);
            }
            db->transit_to_cyberway.disconnect_all_slots();

            BOOST_TEST_MESSAGE("-- fix block number to transit");
            db->_fixed_irreversible_block_num = transit_block_num;
            int events_count = 0;
            db->transit_to_cyberway.connect([&](const uint32_t n, const uint32_t skip) {
                BOOST_CHECK_GT(n, event_block_num);
                event_block_num = n;
                ++events_count;

                auto& gpo = db->get_dynamic_global_properties();
                BOOST_CHECK_EQUAL(transit_block_num, gpo.transit_block_num);
                BOOST_CHECK_EQUAL(gpo.transit_block_num, gpo.last_irreversible_block_num);
                BOOST_CHECK_GT(n, gpo.last_irreversible_block_num);
            });

            for (i = 0; i < 1000; ++i) {
                generate_block();
            }
            BOOST_CHECK_EQUAL(events_count, 1000);

            db->transit_to_cyberway.disconnect_all_slots();

            BOOST_TEST_MESSAGE("-- unfix block number to transit");

            db->_fixed_irreversible_block_num = UINT32_MAX;
            events_count = 0;
            auto unfixed_event = db->transit_to_cyberway.connect([&](const uint32_t n, const uint32_t skip) {
                BOOST_CHECK_GT(n, event_block_num);
                event_block_num = n;
                ++events_count;

                auto& gpo = db->get_dynamic_global_properties();
                BOOST_CHECK_EQUAL(transit_block_num, gpo.transit_block_num);
                BOOST_CHECK_EQUAL(gpo.transit_block_num, gpo.last_irreversible_block_num);
                BOOST_CHECK_GT(n, gpo.last_irreversible_block_num);
            });

            for (i = 0; i < 1000; ++i) {
                generate_block();
            }
            BOOST_CHECK_EQUAL(events_count, 0);

            {
                auto& gpo = db->get_dynamic_global_properties();
                BOOST_CHECK_EQUAL(transit_block_num, gpo.transit_block_num);
                BOOST_CHECK_GT(gpo.last_irreversible_block_num, gpo.transit_block_num);
            }
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_FIXTURE_TEST_CASE(skip_transit_to_cyberway_event, clean_database_fixture) {
        try {
            resize_shared_mem(1024 * 1024 * 32);

            BOOST_TEST_MESSAGE("Testing: skipping of transit_to_cyberway event");

            auto witness = db->get_scheduled_witness(1);
            auto witness_priv_key = STEEMIT_INIT_PRIVATE_KEY;
            generate_block();

            BOOST_TEST_MESSAGE("-- wait STEEMIT_START_MINER_VOTING_BLOCK blocks");
            generate_blocks(STEEMIT_START_MINER_VOTING_BLOCK);

            BOOST_TEST_MESSAGE("-- vote to transit");
            signed_transaction tx;
            tx.set_expiration(db->head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

            transit_to_cyberway_operation op;
            op.owner = witness;
            op.vote_to_transit = true;
            tx.operations.push_back(op);
            tx.sign(witness_priv_key, db->get_chain_id());
            BOOST_CHECK_NO_THROW(db->push_transaction(tx, 0));

            BOOST_TEST_MESSAGE("-- save block to transit");
            generate_block();
            auto transit_block_num = db->head_block_num();

            BOOST_TEST_MESSAGE("-- wait event to transit");

            int events_count = 0;
            uint32_t event_block_num = 0;
            db->transit_to_cyberway.connect([&](const uint32_t n, const uint32_t skip) {
                BOOST_CHECK_GT(n, event_block_num);
                event_block_num = n;
                ++events_count;

                BOOST_TEST_MESSAGE("--- validate transit properties");
                auto& gpo = db->get_dynamic_global_properties();
                BOOST_CHECK_EQUAL(gpo.transit_witnesses[0], witness);
                for (std::size_t i = 1; i < gpo.transit_witnesses.size(); ++i) {
                    BOOST_CHECK_EQUAL(gpo.transit_witnesses[i], account_name_type());
                }

                BOOST_CHECK_EQUAL(transit_block_num, gpo.transit_block_num);
                BOOST_CHECK_EQUAL(gpo.transit_block_num, gpo.last_irreversible_block_num);
                BOOST_CHECK_GT(event_block_num, gpo.last_irreversible_block_num);
            });

            std::size_t i;
            for (i = 0; i < 1000; ++i) {
                generate_block();
            }
            BOOST_CHECK_EQUAL(events_count, 1);

            {
                auto& gpo = db->get_dynamic_global_properties();
                BOOST_CHECK_EQUAL(transit_block_num, gpo.transit_block_num);
                BOOST_CHECK_GT(gpo.last_irreversible_block_num, gpo.transit_block_num);
            }
        }
        FC_LOG_AND_RETHROW()
    }

BOOST_AUTO_TEST_SUITE_END()
#endif