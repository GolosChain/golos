#include <boost/test/unit_test.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/database_exceptions.hpp>

#include <golos/version/hardfork.hpp>

#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/asset_object.hpp>
#include <golos/chain/objects/market_object.hpp>

#include <golos/application/database_api.hpp>

#include <fc/crypto/digest.hpp>
#include <golos/market_history/market_history_api.hpp>

#include "../common/database_fixture.hpp"

using namespace golos::chain;
using namespace golos::chain::test;

namespace golos {
    namespace chain {

        struct swan_fixture : database_fixture {
            limit_order_object init_standard_swan(share_type amount = 1000) {
                standard_users();
                standard_asset();
                return trigger_swan(amount, amount);
            }

            void standard_users() {
                ACTORS((borrower)(borrower2)(feedproducer));
                _borrower = borrower_id;
                _borrower2 = borrower2_id;
                _feedproducer = feedproducer_id;

                transfer(committee_account, borrower_id, asset<0, 17, 0>(init_balance));
                transfer(committee_account, borrower2_id, asset<0, 17, 0>(init_balance));
            }

            void standard_asset() {
                const auto &bitusd = create_bitasset("USDBIT", _feedproducer);
                _swan = bitusd.asset_name;
                _back = STEEM_SYMBOL_NAME;
                update_feed_producers(swan(), {_feedproducer});
            }

            vector<collateral_bid_object> get_collateral_bids(const asset_name_type asset, uint32_t limit,
                                                              uint32_t start) const {
                try {
                    FC_ASSERT(limit <= 100);
                    const asset_object &swan = db.get_asset(asset);
                    FC_ASSERT(swan.is_market_issued());
                    const asset_bitasset_data_object &bad = db.get_asset_bitasset_data(asset);
                    const asset_object &back = db.get_asset(bad.options.short_backing_asset);
                    const auto &idx = db.get_index<collateral_bid_index>();
                    const auto &aidx = idx.indices().get<by_price>();
                    auto start = aidx.lower_bound(boost::make_tuple(asset, price<0, 17, 0>::max(back.asset_name, asset),
                                                                    collateral_bid_object::id_type()));
                    auto end = aidx.lower_bound(boost::make_tuple(asset, price<0, 17, 0>::min(back.asset_name, asset),
                                                                  collateral_bid_object::id_type(STEEMIT_MAX_INSTANCE_ID)));
                    vector<collateral_bid_object> result;
                    while (start != end && limit-- > 0) {
                        result.emplace_back(*start);
                        ++start;
                    }
                    return result;
                } FC_CAPTURE_AND_RETHROW((asset)(limit))
            }

            limit_order_object trigger_swan(share_type amount1, share_type amount2) {
                // starting out with price 1:1
                set_feed(1, 1);
                // start out with 2:1 collateral
                borrow(borrower(), swan().amount(amount1), back().amount(2 * amount1));
                borrow(borrower2(), swan().amount(amount2), back().amount(4 * amount2));

                FC_ASSERT(get_balance(borrower(), swan()) == amount1);
                FC_ASSERT(get_balance(borrower2(), swan()) == amount2);
                FC_ASSERT(get_balance(borrower(), back()) == init_balance - 2 * amount1);
                FC_ASSERT(get_balance(borrower2(), back()) == init_balance - 4 * amount2);

                set_feed(1, 2);
                // this sell order is designed to trigger a black swan
                const limit_order_object *oid = create_sell_order(borrower2(), swan().amount(1), back().amount(3));

                FC_ASSERT(get_balance(borrower(), swan()) == amount1);
                FC_ASSERT(get_balance(borrower2(), swan()) == amount2 - 1);
                FC_ASSERT(get_balance(borrower(), back()) == init_balance - 2 * amount1);
                FC_ASSERT(get_balance(borrower2(), back()) == init_balance - 2 * amount2);

                BOOST_CHECK(db.get_asset_bitasset_data(swan().asset_name).has_settlement());

                return *oid;
            }

            void set_feed(share_type usd, share_type core) {
                price_feed<0, 17, 0> feed;
                feed.settlement_price = swan().amount(usd) / back().amount(core);
                publish_feed(swan(), feedproducer(), feed);
            }

            void expire_feed() {
                generate_blocks(db.head_block_time() + STEEMIT_DEFAULT_PRICE_FEED_LIFETIME);
                generate_blocks(2);
                FC_ASSERT(db.get_asset_bitasset_data(_swan).current_feed.settlement_price.is_null());
            }

            const account_object &borrower() {
                return db.get_account(_borrower);
            }

            const account_object &borrower2() {
                return db.get_account(_borrower2);
            }

            const account_object &feedproducer() {
                return db.get_account(_feedproducer);
            }

            const asset_object &swan() {
                return db.get_asset(_swan);
            }

            const asset_object &back() {
                return db.get_asset(_back);
            }

            int64_t init_balance = 1000000;
            account_name_type _borrower, _borrower2, _feedproducer;
            asset_name_type _swan, _back;
        };
    }
}

BOOST_FIXTURE_TEST_SUITE(swan_tests, swan_fixture)

    /**
     *  This test sets up the minimum condition for a black swan to occur but does
     *  not test the full range of cases that may be possible during a black swan.
     */
    BOOST_AUTO_TEST_CASE(black_swan) {
        try {
            init_standard_swan();

            force_settle(borrower(), swan().amount(100));

            expire_feed();
            generate_blocks(2);

            force_settle(borrower(), swan().amount(100));

            set_feed(100, 150);

            BOOST_TEST_MESSAGE("Verify that we cannot borrow after black swan");
            STEEMIT_REQUIRE_THROW(borrow(borrower(), swan().amount(1000), back().amount(2000)), fc::exception)
            trx.operations.clear();
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    /**
     * Black swan occurs when price feed falls, triggered by settlement
     * order.
     */
    BOOST_AUTO_TEST_CASE(black_swan_issue_346) {
        try {
            ACTORS((buyer)(seller)(borrower)(borrower2)(settler)(feeder));

            const asset_object &core = db.get_asset(STEEM_SYMBOL_NAME);

            int trial = 0;

            std::vector<const account_object *> actors{&buyer, &seller, &borrower, &borrower2, &settler, &feeder};

            auto top_up = [&]() {
                for (const account_object *actor : actors) {
                    int64_t bal = get_balance(*actor, core);
                    if (bal < init_balance) {
                        transfer(committee_account, actor->name, asset<0, 17, 0>(init_balance - bal));
                    } else if (bal > init_balance) {
                        transfer(actor->name, committee_account, asset<0, 17, 0>(bal - init_balance));
                    }
                }
            };

            auto setup_asset = [&]() -> const asset_object & {
                const asset_object &bitusd = create_bitasset("USDBIT" + fc::to_string(trial) + "X", feeder_id);
                update_feed_producers(bitusd, {feeder.name});
                BOOST_CHECK(!db.get_asset_bitasset_data(bitusd.asset_name).has_settlement());
                trial++;
                return bitusd;
            };

            /*
             * STEEMIT_COLLATERAL_RATIO_DENOM
            uint16_t maintenance_collateral_ratio = STEEMIT_DEFAULT_MAINTENANCE_COLLATERAL_RATIO;
            uint16_t maximum_short_squeeze_ratio = STEEMIT_DEFAULT_MAX_SHORT_SQUEEZE_RATIO;
            */

            // situations to test:
            // 1. minus short squeeze protection would be black swan, otherwise no
            // 2. issue 346 (price feed drops followed by force settle, drop should trigger BS)
            // 3. feed price < D/C of least collateralized short < call price < highest bid

            auto set_price = [&](const asset_object &bitusd, const price<0, 17, 0> &settlement_price) {
                price_feed<0, 17, 0> feed;
                feed.settlement_price = settlement_price;
                feed.core_exchange_rate = settlement_price;
                wdump((feed.max_short_squeeze_price()));
                publish_feed(bitusd, feeder, feed);
            };

            auto wait_for_settlement = [&]() {
                const auto &idx = db.get_index<force_settlement_index>().indices().get<by_expiration>();
                const auto &itr = idx.rbegin();
                if (itr == idx.rend()) {
                    return;
                }
                generate_blocks(itr->settlement_date);
                BOOST_CHECK(!idx.empty());
                generate_block();
                BOOST_CHECK(idx.empty());
            };

            {
                const asset_object &bitusd = setup_asset();
                top_up();
                set_price(bitusd, bitusd.amount(1) / core.amount(5));  // $0.20
                borrow(borrower, bitusd.amount(100), asset<0, 17, 0>(1000));       // 2x collat
                transfer(borrower.name, settler.name, bitusd.amount(100));

                // drop to $0.02 and settle
                BOOST_CHECK(!db.get_asset_bitasset_data(bitusd.asset_name).has_settlement());
                set_price(bitusd, bitusd.amount(1) / core.amount(50)); // $0.02
                BOOST_CHECK(db.get_asset_bitasset_data(bitusd.asset_name).has_settlement());
                STEEMIT_REQUIRE_THROW(borrow(borrower2, bitusd.amount(100), asset<0, 17, 0>(10000)), fc::exception);
                force_settle(settler, bitusd.amount(100));

                // wait for forced settlement to execute
                // this would throw on Sep.18 testnet, see #346
                wait_for_settlement();
            }

            // issue 350
            {
                // ok, new asset
                const asset_object &bitusd = setup_asset();
                top_up();
                set_price(bitusd, bitusd.amount(40) / core.amount(1000)); // $0.04
                borrow(borrower, bitusd.amount(100), asset<0, 17, 0>(5000));    // 2x collat
                transfer(borrower.name, seller.name, bitusd.amount(100));
                const limit_order_object *oid_019 = create_sell_order(seller, bitusd.amount(39), core.amount(
                        2000));   // this order is at $0.019, we should not be able to match against it
                const limit_order_object *oid_020 = create_sell_order(seller, bitusd.amount(40), core.amount(
                        2000));   // this order is at $0.020, we should be able to match against it
                set_price(bitusd, bitusd.amount(21) / core.amount(1000)); // $0.021
                //
                // We attempt to match against $0.019 order and black swan,
                // and this is intended behavior.  See discussion in ticket.
                //
                BOOST_CHECK(db.get_asset_bitasset_data(bitusd.asset_name).has_settlement());
                BOOST_CHECK(db.find_limit_order(seller.name, oid_019->order_id) != nullptr);
                BOOST_CHECK(db.find_limit_order(seller.name, oid_020->order_id) == nullptr);
            }

        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    /** Creates a black swan, recover price feed - asset should be revived
     */
    BOOST_AUTO_TEST_CASE(revive_recovered) {
        try {
            init_standard_swan(700);

            generate_blocks(2);

            // revive after price recovers
            set_feed(700, 800);
            BOOST_CHECK(db.get_asset_bitasset_data(swan().asset_name).has_settlement());
            set_feed(701, 800);
            BOOST_CHECK(!db.get_asset_bitasset_data(swan().asset_name).has_settlement());
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    /** Creates a black swan, recover price feed - asset should be revived
     */
    BOOST_AUTO_TEST_CASE(recollateralize) {
        try {
            init_standard_swan(700);

            // no hardfork yet
            STEEMIT_REQUIRE_THROW(bid_collateral(borrower2(), back().amount(1000), swan().amount(100)), fc::exception);

            generate_blocks(2);

            int64_t b2_balance = get_balance(borrower2(), back());
            bid_collateral(borrower2(), back().amount(1000), swan().amount(100));
            BOOST_CHECK_EQUAL(get_balance(borrower2(), back()), b2_balance - 1000);
            bid_collateral(borrower2(), back().amount(2000), swan().amount(200));
            BOOST_CHECK_EQUAL(get_balance(borrower2(), back()), b2_balance - 2000);
            bid_collateral(borrower2(), back().amount(1000), swan().amount(0));
            BOOST_CHECK_EQUAL(get_balance(borrower2(), back()), b2_balance);

            // can't bid for non-bitassets
            STEEMIT_REQUIRE_THROW(bid_collateral(borrower2(), swan().amount(100), asset<0, 17, 0>(100)), fc::exception);
            // can't cancel a non-existant bid
            STEEMIT_REQUIRE_THROW(bid_collateral(borrower2(), back().amount(0), swan().amount(0)), fc::exception);
            // can't bid zero collateral
            STEEMIT_REQUIRE_THROW(bid_collateral(borrower2(), back().amount(0), swan().amount(100)), fc::exception);
            // can't bid more than we have
            STEEMIT_REQUIRE_THROW(bid_collateral(borrower2(), back().amount(b2_balance + 100), swan().amount(100)),
                                  fc::exception);
            trx.operations.clear();

            // can't bid on a live bitasset
            const asset_object &bitcny = create_bitasset("CNYBIT", _feedproducer);
            STEEMIT_REQUIRE_THROW(bid_collateral(borrower2(), asset<0, 17, 0>(100), bitcny.amount(100)), fc::exception);
            update_feed_producers(bitcny, {_feedproducer});
            price_feed<0, 17, 0> feed;
            feed.settlement_price = bitcny.amount(1) / asset<0, 17, 0>(1);
            publish_feed(bitcny.asset_name, _feedproducer, feed);
            borrow(borrower2(), bitcny.amount(100), asset<0, 17, 0>(1000));

            // can't bid wrong collateral type
            STEEMIT_REQUIRE_THROW(bid_collateral(borrower2(), bitcny.amount(100), swan().amount(100)), fc::exception);

            BOOST_CHECK(db.get_asset_dynamic_data(_swan).current_supply == 1400);
            BOOST_CHECK(db.get_asset_bitasset_data(_swan).settlement_fund == 2800);
            BOOST_CHECK(db.get_asset_bitasset_data(_swan).has_settlement());
            BOOST_CHECK(db.get_asset_bitasset_data(_swan).current_feed.settlement_price.is_null());

            // doesn't happen without price feed
            bid_collateral(borrower(), back().amount(1400), swan().amount(700));
            bid_collateral(borrower2(), back().amount(1400), swan().amount(700));

            generate_blocks(1);

            BOOST_CHECK(db.get_asset_bitasset_data(_swan).has_settlement());

            set_feed(1, 2);
            // doesn't happen if cover is insufficient
            bid_collateral(borrower2(), back().amount(1400), swan().amount(600));

            generate_blocks(1);

            BOOST_CHECK(db.get_asset_bitasset_data(_swan).has_settlement());

            set_feed(1, 2);
            // doesn't happen if some bids have a bad swan price
            bid_collateral(borrower2(), back().amount(1050), swan().amount(700));

            generate_blocks(1);

            BOOST_CHECK(db.get_asset_bitasset_data(_swan).has_settlement());

            set_feed(1, 2);
            // works
            bid_collateral(borrower(), back().amount(1051), swan().amount(700));
            bid_collateral(borrower2(), back().amount(2100), swan().amount(1399));

            // check get_collateral_bids
            STEEMIT_REQUIRE_THROW(get_collateral_bids(back().asset_name, 100, 0), fc::assert_exception);
            std::vector<collateral_bid_object> bids = get_collateral_bids(_swan, 100, 1);

            BOOST_CHECK_EQUAL(1, bids.size());
            FC_ASSERT(_borrower2 == bids[0].bidder);
            bids = get_collateral_bids(_swan, 1, 0);
            BOOST_CHECK_EQUAL(1, bids.size());
            FC_ASSERT(_borrower == bids[0].bidder);
            bids = get_collateral_bids(_swan, 100, 0);
            BOOST_CHECK_EQUAL(2, bids.size());
            FC_ASSERT(_borrower == bids[0].bidder);
            FC_ASSERT(_borrower2 == bids[1].bidder);

            // revive

            generate_blocks(1);

            BOOST_CHECK(!db.get_asset_bitasset_data(_swan).has_settlement());
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    /** Creates a black swan, settles all debts, recovers price feed - asset should be revived
     */
    BOOST_AUTO_TEST_CASE(revive_empty_recovered) {
        try {
            limit_order_object oid = init_standard_swan(1000);

            cancel_limit_order(oid);
            force_settle(borrower(), swan().amount(1000));
            force_settle(borrower2(), swan().amount(1000));
            BOOST_CHECK_EQUAL(0, db.get_asset_dynamic_data(_swan).current_supply.value);

            generate_blocks(2);

            // revive after price recovers
            set_feed(1, 1);
            BOOST_CHECK(!db.get_asset_bitasset_data(_swan).has_settlement());
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    /** Creates a black swan, settles all debts - asset should be revived in next maintenance
     */
    BOOST_AUTO_TEST_CASE(revive_empty) {
        try {
            generate_blocks(2);

            trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            limit_order_object oid = init_standard_swan(1000);

            cancel_limit_order(oid);
            force_settle(borrower(), swan().amount(1000));
            force_settle(borrower2(), swan().amount(1000));
            BOOST_CHECK_EQUAL(0, db.get_asset_dynamic_data(_swan).current_supply.value);

            BOOST_CHECK(db.get_asset_bitasset_data(_swan).has_settlement());

            // revive

            generate_blocks(1);

            BOOST_CHECK(!db.get_asset_bitasset_data(_swan).has_settlement());
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    /** Creates a black swan, settles all debts - asset should be revived in next maintenance
     */
    BOOST_AUTO_TEST_CASE(revive_empty_with_bid) {
        try {
            trx.set_expiration(db.head_block_time() + STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
            standard_users();
            standard_asset();

            set_feed(1, 1);
            borrow(borrower(), swan().amount(1000), back().amount(2000));
            borrow(borrower2(), swan().amount(1000), back().amount(1967));

            set_feed(1, 2);
            // this sell order is designed to trigger a black swan
            limit_order_object oid = *create_sell_order(borrower2(), swan().amount(1), back().amount(3));
            BOOST_CHECK(db.get_asset_bitasset_data(_swan).has_settlement());

            cancel_limit_order(oid);
            force_settle(borrower(), swan().amount(500));
            force_settle(borrower(), swan().amount(500));
            force_settle(borrower2(), swan().amount(667));
            force_settle(borrower2(), swan().amount(333));
            BOOST_CHECK_EQUAL(0, db.get_asset_dynamic_data(_swan).current_supply.value);
            BOOST_CHECK_EQUAL(0, db.get_asset_bitasset_data(_swan).settlement_fund.value);

            bid_collateral(borrower(), back().amount(1051), swan().amount(700));

            BOOST_CHECK(db.get_asset_bitasset_data(_swan).has_settlement());

            // revive
            generate_blocks(1);

            BOOST_CHECK(!db.get_asset_bitasset_data(_swan).has_settlement());
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

BOOST_AUTO_TEST_SUITE_END()