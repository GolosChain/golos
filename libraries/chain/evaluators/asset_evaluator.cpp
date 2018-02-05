#include <golos/chain/evaluators/asset_evaluator.hpp>
#include <golos/chain/objects/asset_object.hpp>
#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/market_object.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/database_exceptions.hpp>

#include <golos/version/hardfork.hpp>

#include <functional>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_create_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &op) {
            try {
                FC_ASSERT(op.common_options.whitelist_authorities.size() <=
                          STEEMIT_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES);
                FC_ASSERT(op.common_options.blacklist_authorities.size() <=
                          STEEMIT_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES);

                // Check that all authorities do exist
                for (const auto &id : op.common_options.whitelist_authorities) {
                    this->db.get_account(id);
                }
                for (const auto &id : op.common_options.blacklist_authorities) {
                    this->db.get_account(id);
                }

                auto &asset_indx = this->db.template get_index<asset_index>().indices().template get<by_asset_name>();
                auto asset_symbol_itr = asset_indx.find(op.asset_name);
                FC_ASSERT(asset_symbol_itr == asset_indx.end());


                std::string asset_name_string = op.asset_name;

                auto dotpos = asset_name_string.rfind('.');
                if (dotpos != std::string::npos) {
                    auto prefix = asset_name_string.substr(0, dotpos);
                    auto asset_symbol_sub_itr = asset_indx.find(op.asset_name);
                    FC_ASSERT(asset_symbol_sub_itr != asset_indx.end(),
                              "Asset ${s} may only be created by issuer of ${p}, but ${p} has not been registered",
                              ("s", op.asset_name)("p", prefix));
                    FC_ASSERT(asset_symbol_sub_itr->issuer == op.issuer,
                              "Asset ${s} may only be created by issuer of ${p}, ${i}",
                              ("s", op.asset_name)("p", prefix)("i", this->db.get_account(op.issuer).name));
                }

                if (op.bitasset_opts) {
                    const asset_object &backing = this->db.get_asset(op.bitasset_opts->short_backing_asset);
                    if (backing.is_market_issued()) {
                        const asset_bitasset_data_object &backing_bitasset_data = this->db.get_asset_bitasset_data(
                                backing.asset_name);
                        const asset_object &backing_backing = this->db.get_asset(backing_bitasset_data.options.short_backing_asset);
                        FC_ASSERT(!backing_backing.is_market_issued(),
                                  "May not create a bitasset backed by a bitasset backed by a bitasset.");
                        FC_ASSERT(this->db.get_account(op.issuer).name != STEEMIT_COMMITTEE_ACCOUNT ||
                                  backing_backing.asset_name == STEEM_SYMBOL_NAME,
                                  "May not create a blockchain-controlled market asset which is not backed by CORE.");
                    } else {
                        FC_ASSERT(this->db.get_account(op.issuer).name != STEEMIT_COMMITTEE_ACCOUNT ||
                                  backing.asset_name == STEEM_SYMBOL_NAME,
                                  "May not create a blockchain-controlled market asset which is not backed by CORE.");
                    }
                    FC_ASSERT(op.bitasset_opts->feed_lifetime_sec > STEEMIT_BLOCK_INTERVAL &&
                              op.bitasset_opts->force_settlement_delay_sec > STEEMIT_BLOCK_INTERVAL);
                }
                if (op.is_prediction_market) {
                    FC_ASSERT(op.bitasset_opts);
                    FC_ASSERT(op.precision == this->db.get_asset(op.bitasset_opts->short_backing_asset).precision);
                }

            } FC_CAPTURE_AND_RETHROW((op))

            try {
                this->db.adjust_balance(this->db.get_account(op.issuer), -this->db.get_name_cost(op.asset_name));

                this->db.template create<asset_dynamic_data_object>([&](asset_dynamic_data_object &a) {
                    a.asset_name = op.asset_name;
                    a.current_supply = 0;
                    a.fee_pool = 0; //op.calculate_fee(this->db.template current_fee_schedule()).value / 2;
                });

                if (op.bitasset_opts.valid()) {
                    this->db.template create<asset_bitasset_data_object>([&](asset_bitasset_data_object &a) {
                        a.asset_name = op.asset_name;
                        a.options = *op.bitasset_opts;
                        a.is_prediction_market = op.is_prediction_market;
                    });
                }

                this->db.template create<asset_object>([&](asset_object &a) {
                    a.issuer = op.issuer;
                    a.asset_name = op.asset_name;
                    a.precision = op.precision;
                    a.options = op.common_options;
                    if (a.options.core_exchange_rate.base.symbol == STEEM_SYMBOL_NAME) {
                        a.options.core_exchange_rate.quote.symbol = op.asset_name;
                    } else {
                        a.options.core_exchange_rate.base.symbol = op.asset_name;
                    }
                    if (op.bitasset_opts.valid()) {
                        a.market_issued = true;
                    }
                });
            } FC_CAPTURE_AND_RETHROW((op))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_issue_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                const asset_object &a = this->db.get_asset(o.asset_to_issue.symbol);
                FC_ASSERT(o.issuer == a.issuer);
                FC_ASSERT(!a.is_market_issued(), "Cannot manually issue a market-issued asset.");

                to_account = this->db.find_account(o.issue_to_account);
                FC_ASSERT(this->db.is_authorized_asset(*to_account, a));

                asset_dyn_data = this->db.find_asset_dynamic_data(a.asset_name);
                FC_ASSERT((asset_dyn_data->current_supply + o.asset_to_issue.amount) <= a.options.max_supply);
            } FC_CAPTURE_AND_RETHROW((o))

            try {
                this->db.adjust_balance(this->db.get_account(o.issue_to_account), o.asset_to_issue);

                this->db.modify(*asset_dyn_data, [&](asset_dynamic_data_object &data) {
                    data.current_supply += o.asset_to_issue.amount;
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_reserve_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                const asset_object &a = this->db.get_asset(o.amount_to_reserve.symbol);
                STEEMIT_ASSERT(!a.is_market_issued(), typename BOOST_IDENTITY_TYPE((exceptions::operations::asset_reserve::invalid_on_mia<Major, Hardfork, Release>)),
                               "Cannot reserve ${sym} because it is a market-issued asset", ("sym", a.asset_name));

                from_account = this->db.find_account(o.payer);
                FC_ASSERT(this->db.is_authorized_asset(*from_account, a));

                asset_dyn_data = this->db.find_asset_dynamic_data(a.asset_name);
                FC_ASSERT((asset_dyn_data->current_supply - o.amount_to_reserve.amount) >= 0);

            } FC_CAPTURE_AND_RETHROW((o))

            try {
                this->db.adjust_balance(this->db.get_account(o.payer), -o.amount_to_reserve);

                this->db.modify(*asset_dyn_data, [&](asset_dynamic_data_object &data) {
                    data.current_supply -= o.amount_to_reserve.amount;
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_fund_fee_pool_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                FC_ASSERT(this->db.find_asset(o.asset_name));
                FC_ASSERT(this->db.find_asset_dynamic_data(o.asset_name));

                this->db.adjust_balance(this->db.get_account(o.from_account), -protocol::asset<0, 17, 0>(o.amount, o.asset_name));

                this->db.modify(this->db.get_asset_dynamic_data(o.asset_name), [&](asset_dynamic_data_object &data) {
                    data.fee_pool += o.amount;
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_update_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                const asset_object &a = this->db.get_asset(o.asset_to_update);
                auto a_copy = a;
                a_copy.options = o.new_options;
                a_copy.validate();

                if (o.new_issuer) {
                    FC_ASSERT(this->db.find_account(*o.new_issuer));
                    if (a.is_market_issued() && *o.new_issuer == STEEMIT_COMMITTEE_ACCOUNT) {
                        const asset_object &backing = this->db.get_asset(this->db.get_asset_bitasset_data(a.asset_name).options.short_backing_asset);
                        if (backing.is_market_issued()) {
                            const asset_object &backing_backing = this->db.template get_asset(
                                    this->db.get_asset_bitasset_data(backing.asset_name).options.short_backing_asset);
                            FC_ASSERT(backing_backing.asset_name == STEEM_SYMBOL_NAME,
                                      "May not create a blockchain-controlled market asset which is not backed by CORE.");
                        } else
                            FC_ASSERT(backing.asset_name == STEEM_SYMBOL_NAME,
                                      "May not create a blockchain-controlled market asset which is not backed by CORE.");
                    }
                }

                if ((this->db.get_asset_dynamic_data(a.asset_name).current_supply != 0)) {
                    // new issuer_permissions must be subset of old issuer permissions
                    FC_ASSERT(!(o.new_options.issuer_permissions & ~a.options.issuer_permissions),
                              "Cannot reinstate previously revoked issuer permissions on an asset.");
                }

                // changed flags must be subset of old issuer permissions
                FC_ASSERT(!((o.new_options.flags ^ a.options.flags) & ~a.options.issuer_permissions),
                          "Flag change is forbidden by issuer permissions");

                asset_to_update = &a;
                FC_ASSERT(o.issuer == a.issuer, "", ("o.issuer", o.issuer)("a.issuer", a.issuer));

                FC_ASSERT(
                        o.new_options.whitelist_authorities.size() <= STEEMIT_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES);
                for (const auto &id : o.new_options.whitelist_authorities) {
                    this->db.get_account(id);
                }
                FC_ASSERT(
                        o.new_options.blacklist_authorities.size() <= STEEMIT_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES);
                for (const auto &id : o.new_options.blacklist_authorities) {
                    this->db.get_account(id);
                }
            } FC_CAPTURE_AND_RETHROW((o))

            try {
                // If we are now disabling force settlements, cancel all open force settlement orders
                if (o.new_options.flags & disable_force_settle && asset_to_update->can_force_settle()) {
                    const auto &idx = this->db.template get_index<force_settlement_index>().indices().template get<by_expiration>();
                    // Funky iteration code because we're removing objects as we go. We have to re-initialize itr every loop instead
                    // of simply incrementing it.
                    for (auto itr = idx.lower_bound(o.asset_to_update);
                         itr != idx.end() && itr->settlement_asset_symbol() == o.asset_to_update; itr = idx.lower_bound(
                            o.asset_to_update)) {
                        this->db.cancel_order(*itr);
                    }
                }

                this->db.template modify(*asset_to_update, [&](asset_object &a) {
                    if (o.new_issuer) {
                        a.issuer = *o.new_issuer;
                    }
                    a.options = o.new_options;
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_update_bitasset_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                const asset_object &a = this->db.get_asset(o.asset_to_update);

                FC_ASSERT(a.is_market_issued(), "Cannot update BitAsset-specific settings on a non-BitAsset.");

                const asset_bitasset_data_object &b = this->db.get_asset_bitasset_data(a.asset_name);
                FC_ASSERT(!b.has_settlement(), "Cannot update a BitAsset after a settlement has executed");
                if (o.new_options.short_backing_asset != b.options.short_backing_asset) {
                    FC_ASSERT(this->db.get_asset_dynamic_data(a.asset_name).current_supply == 0);
                    FC_ASSERT(this->db.find_asset(o.new_options.short_backing_asset));

                    if (a.issuer == STEEMIT_COMMITTEE_ACCOUNT) {
                        const asset_object &backing = this->db.get_asset(this->db.get_asset_bitasset_data(a.asset_name).options.short_backing_asset);
                        if (backing.is_market_issued()) {
                            const asset_object &backing_backing = this->db.get_asset(
                                    this->db.get_asset_bitasset_data(backing.asset_name).options.short_backing_asset);
                            FC_ASSERT(backing_backing.asset_name == STEEM_SYMBOL_NAME,
                                      "May not create a blockchain-controlled market asset which is not backed by CORE.");
                        } else
                            FC_ASSERT(backing.asset_name == STEEM_SYMBOL_NAME,
                                      "May not create a blockchain-controlled market asset which is not backed by CORE.");
                    }
                }

                bitasset_to_update = &b;
                FC_ASSERT(o.issuer == a.issuer, "", ("o.issuer", o.issuer)("a.issuer", a.issuer));

            } FC_CAPTURE_AND_RETHROW((o))

            try {
                bool should_update_feeds = false;
                // If the minimum number of feeds to calculate a median has changed, we need to recalculate the median
                if (o.new_options.minimum_feeds != bitasset_to_update->options.minimum_feeds) {
                    should_update_feeds = true;
                }

                this->db.modify(*bitasset_to_update, [&](asset_bitasset_data_object &b) {
                    b.options = o.new_options;

                    if (should_update_feeds) {
                        b.update_median_feeds(this->db.head_block_time());
                    }
                });

            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_update_feed_producers_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                FC_ASSERT(o.new_feed_producers.size() <= STEEMIT_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES);
                for (const auto &id : o.new_feed_producers) {
                    this->db.get_account(id);
                }

                const asset_object &a = this->db.get_asset(o.asset_to_update);

                FC_ASSERT(a.is_market_issued(), "Cannot update feed producers on a non-BitAsset.");
                FC_ASSERT(!(a.options.flags & committee_fed_asset),
                          "Cannot set feed producers on a committee-fed asset.");
                FC_ASSERT(!(a.options.flags & witness_fed_asset), "Cannot set feed producers on a witness-fed asset.");

                const asset_bitasset_data_object &b = this->db.get_asset_bitasset_data(a.asset_name);
                bitasset_to_update = &b;
                FC_ASSERT(a.issuer == o.issuer);
            } FC_CAPTURE_AND_RETHROW((o))

            try {
                this->db.modify(*bitasset_to_update, [&](asset_bitasset_data_object &a) {
                    //This is tricky because I have a set of publishers coming in, but a map of publisher to feed is storethis->db.template 
                    //I need to update the map such that the keys match the new publishers, but not munge the old price feeds from
                    //publishers who are being kept.
                    //First, remove any old publishers who are no longer publishers
                    for (auto itr = a.feeds.begin(); itr != a.feeds.end();) {
                        if (!o.new_feed_producers.count(itr->first)) {
                            itr = a.feeds.erase(itr);
                        } else {
                            ++itr;
                        }
                    }
                    //Now, add any new publishers
                    for (const auto &new_feed_producer : o.new_feed_producers) {
                        if (!a.feeds.count(new_feed_producer)) {
                            a.feeds[new_feed_producer];
                        }
                    }
                    a.update_median_feeds(this->db.head_block_time());
                });
                this->db.check_call_orders(this->db.get_asset(o.asset_to_update));

            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_global_settle_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &op) {
            try {
                asset_to_settle = this->db.find_asset(op.asset_to_settle);
                FC_ASSERT(asset_to_settle->is_market_issued());
                FC_ASSERT(asset_to_settle->can_global_settle());
                FC_ASSERT(asset_to_settle->issuer == op.issuer);
                FC_ASSERT(this->db.get_asset_dynamic_data(asset_to_settle->asset_name).current_supply > 0);
                const auto &idx = this->db.template get_index<call_order_index>().indices().template get<by_collateral>();
                assert(!idx.empty());
                auto itr = idx.lower_bound(boost::make_tuple(
                        price<Major, Hardfork, Release>::min(this->db.get_asset_bitasset_data(asset_to_settle->asset_name).options.short_backing_asset,
                                   op.asset_to_settle)));
                assert(itr != idx.end() && itr->debt_type() == op.asset_to_settle);
                const call_order_object &least_collateralized_short = *itr;
                FC_ASSERT(least_collateralized_short.get_debt() * op.settle_price <=
                          least_collateralized_short.get_collateral(),
                          "Cannot force settle at supplied price: least collateralized short lacks sufficient collateral to settle.");

            } FC_CAPTURE_AND_RETHROW((op))

            try {
                this->db.globally_settle_asset(this->db.get_asset(op.asset_to_settle), op.settle_price);
            } FC_CAPTURE_AND_RETHROW((op))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_settle_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &op) {
            try {
                asset_to_settle = this->db.find_asset(op.amount.symbol);
                FC_ASSERT(asset_to_settle->is_market_issued());
                const auto &bitasset = this->db.get_asset_bitasset_data(asset_to_settle->asset_name);
                FC_ASSERT(asset_to_settle->precision == op.amount.precision(), "Settlement asset precision differs");
                FC_ASSERT(asset_to_settle->can_force_settle() || bitasset.has_settlement());
                if (bitasset.is_prediction_market)
                    FC_ASSERT(bitasset.has_settlement(),
                              "global settlement must occur before force settling a prediction market");
                else if (bitasset.current_feed.settlement_price.is_null() || !bitasset.has_settlement())
                    FC_THROW_EXCEPTION(exceptions::chain::insufficient_feeds<>, "Cannot force settle with no price feed.");
                FC_ASSERT(this->db.get_balance(this->db.get_account(op.account), *asset_to_settle) >= op.amount);
            } FC_CAPTURE_AND_RETHROW((op))

            try {
                this->db.adjust_balance(this->db.get_account(op.account), -op.amount);

                const auto &bitasset = this->db.get_asset_bitasset_data(asset_to_settle->asset_name);
                if (bitasset.has_settlement()) {
                    const auto &mia_dyn = this->db.get_asset_dynamic_data(asset_to_settle->asset_name);

                    auto settled_amount = op.amount * bitasset.settlement_price;
                    if (op.amount.amount == mia_dyn.current_supply) {
                        settled_amount.amount = bitasset.settlement_fund; // avoid rounding problems
                    } else {
                        FC_ASSERT(settled_amount.amount <= bitasset.settlement_fund); // should be strictly < except for PM with zero outcome
                    }

                    FC_ASSERT(settled_amount.amount <= bitasset.settlement_fund);

                    this->db.modify(bitasset, [&](asset_bitasset_data_object &obj) {
                        obj.settlement_fund -= settled_amount.amount;
                    });

                    this->db.adjust_balance(this->db.get_account(op.account), settled_amount);

                    this->db.modify(mia_dyn, [&](asset_dynamic_data_object &obj) {
                        obj.current_supply -= op.amount.amount;
                    });
                } else {

                }
            } FC_CAPTURE_AND_RETHROW((op))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_force_settle_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &op) {
            try {
                asset_to_settle = this->db.find_asset(op.amount.symbol);
                FC_ASSERT(asset_to_settle->is_market_issued());
                const auto &bitasset = this->db.get_asset_bitasset_data(asset_to_settle->asset_name);
                FC_ASSERT(asset_to_settle->can_force_settle() || bitasset.has_settlement());
                if (bitasset.is_prediction_market)
                    FC_ASSERT(bitasset.has_settlement(),
                              "global settlement must occur before force settling a prediction market");
                else if (bitasset.current_feed.settlement_price.is_null())
                    FC_THROW_EXCEPTION(exceptions::chain::insufficient_feeds<>, "Cannot force settle with no price feed.");
                FC_ASSERT(this->db.get_balance(this->db.get_account(op.account), *asset_to_settle) >= op.amount);
            } FC_CAPTURE_AND_RETHROW((op))

            try {
                this->db.adjust_balance(this->db.get_account(op.account), -op.amount);

                const auto &bitasset = this->db.get_asset_bitasset_data(asset_to_settle->asset_name);
                if (bitasset.has_settlement()) {
                    auto settled_amount = op.amount * bitasset.settlement_price;
                    FC_ASSERT(settled_amount.amount <= bitasset.settlement_fund);

                    this->db.modify(bitasset, [&](asset_bitasset_data_object &obj) {
                        obj.settlement_fund -= settled_amount.amount;
                    });

                    this->db.adjust_balance(this->db.get_account(op.account), settled_amount);

                    const auto &mia_dyn = this->db.get_asset_dynamic_data(asset_to_settle->asset_name);

                    this->db.modify(mia_dyn, [&](asset_dynamic_data_object &obj) {
                        obj.current_supply -= op.amount.amount;
                    });
                } else {
                    this->db.template create<force_settlement_object>([&](force_settlement_object &s) {
                        s.owner = op.account;
                        s.settlement_id = op.settlement_id;
                        s.balance = op.amount;
                        s.settlement_date = this->db.head_block_time() + this->db.get_asset_bitasset_data(asset_to_settle->asset_name).options.force_settlement_delay_sec;
                    });
                }
            } FC_CAPTURE_AND_RETHROW((op))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_publish_feeds_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                const asset_object &base = this->db.get_asset(o.asset_name);
                //Verify that this feed is for a market-issued asset and that asset is backed by the base
                FC_ASSERT(base.is_market_issued());

                const asset_bitasset_data_object &bitasset = this->db.get_asset_bitasset_data(base.asset_name);
                if (bitasset.is_prediction_market) {
                    FC_ASSERT(!bitasset.has_settlement(), "No further feeds may be published after a settlement event");
                }

                FC_ASSERT(o.feed.settlement_price.quote.symbol == bitasset.options.short_backing_asset);
                if (!o.feed.core_exchange_rate.is_null()) {
                    FC_ASSERT(o.feed.core_exchange_rate.quote.symbol == STEEM_SYMBOL_NAME);
                }


                //Verify that the publisher is authoritative to publish a feed
                if (base.options.flags & witness_fed_asset) {
                    const account_authority_object &witness_authority = this->db.template get<account_authority_object, by_account>(
                            STEEMIT_WITNESS_ACCOUNT);
                    FC_ASSERT(witness_authority.active.account_auths.count(o.publisher));
                } else if (base.options.flags & committee_fed_asset) {
                    const account_authority_object &committee_authority = this->db.template get<account_authority_object, by_account>(
                            STEEMIT_COMMITTEE_ACCOUNT);

                    FC_ASSERT(committee_authority.active.account_auths.count(o.publisher));
                } else {
                    FC_ASSERT(bitasset.feeds.count(o.publisher));
                }

            } FC_CAPTURE_AND_RETHROW((o))

            try {
                const asset_object &base = this->db.get_asset(o.asset_name);
                const asset_bitasset_data_object &bad = this->db.get_asset_bitasset_data(base.asset_name);

                auto old_feed = bad.current_feed;
                // Store medians for this asset
                database &d = this->db;
                this->db.modify(bad, [&o, &d](asset_bitasset_data_object &a) {
                    a.feeds[o.publisher] = make_pair(d.head_block_time(), o.feed);
                    a.update_median_feeds(d.head_block_time());
                });

                if (!(old_feed == bad.current_feed)) {
                    if (bad.has_settlement()) {
                        const asset_dynamic_data_object &mia_dyn = this->db.get_asset_dynamic_data(base.asset_name);
                        if (!bad.current_feed.settlement_price.is_null() &&
                            ~price<Major, Hardfork, Release>::call_price(asset<Major, Hardfork, Release>(mia_dyn.current_supply, o.asset_name),
                                               asset<Major, Hardfork, Release>(bad.settlement_fund, bad.options.short_backing_asset),
                                               bad.current_feed.maintenance_collateral_ratio) <
                            bad.current_feed.settlement_price) {
                            this->db.revive_bitasset(base);
                        }
                    }
                    this->db.check_call_orders(base);
                }

            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset_claim_fees_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                FC_ASSERT(this->db.get_asset(o.amount_to_claim.symbol).issuer == o.issuer,
                          "Asset fees may only be claimed by the issuer");
            } FC_CAPTURE_AND_RETHROW((o))

            try {
                const asset_object &a = this->db.get_asset(o.amount_to_claim.symbol);
                const asset_dynamic_data_object &addo = this->db.get_asset_dynamic_data(a.asset_name);
                FC_ASSERT(o.amount_to_claim.amount <= addo.accumulated_fees,
                          "Attempt to claim more fees than have accumulated", ("addo", addo));

                this->db.modify(addo, [&](asset_dynamic_data_object &_addo) {
                    _addo.accumulated_fees -= o.amount_to_claim.amount;
                });

                this->db.adjust_balance(this->db.get_account(o.issuer), o.amount_to_claim);
            } FC_CAPTURE_AND_RETHROW((o))
        }
    }
} // golos::chain

#include <golos/chain/evaluators/asset_evaluator.tpp>