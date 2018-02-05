#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/asset_object.hpp>
#include <golos/chain/database_exceptions.hpp>
#include <golos/chain/objects/market_object.hpp>
#include <golos/chain/evaluators/market_evaluator.hpp>

#include <golos/chain/database.hpp>
#include <golos/version/hardfork.hpp>

#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/operations/market_operations.hpp>

#include <fc/uint128.hpp>
#include <fc/smart_ref_impl.hpp>

namespace golos {
    namespace chain {
        /// TODO: after the hardfork, we can rename this method validate_permlink because it is strictily less restrictive than before
        ///  Issue #56 contains the justificiation for allowing any UTF-8 string to serve as a permlink, content will be grouped by tags
        ///  going forwarthis->db.template 
        inline void validate_permlink(const string &permlink) {
            FC_ASSERT(permlink.size() < STEEMIT_MAX_PERMLINK_LENGTH, "permlink is too long");
            FC_ASSERT(fc::is_utf8(permlink), "permlink not formatted in UTF8");
        }

        inline void validate_account_name(const string &name) {
            FC_ASSERT(is_valid_account_name(name), "Account name ${n} is invalid", ("n", name));
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void convert_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            const auto &owner = this->db.get_account(o.owner);
            asset<0, 17, 0> delta(o.amount.amount, o.amount.symbol_name());

            FC_ASSERT(this->db.get_balance(owner, o.amount.symbol_name()) >= delta,
                      "Account ${n} does not have sufficient balance for conversion. Balance: ${b}. Required: ${r}",
                      ("n", o.owner)("b", this->db.get_balance(owner, o.amount.symbol_name()))("r", o.amount));


            this->db.adjust_balance(owner, -delta);

            const auto &fhistory = this->db.get_feed_history();
            FC_ASSERT(!fhistory.current_median_history.is_null(), "Cannot convert SBD because there is no price feed.");

            auto steem_conversion_delay = STEEMIT_CONVERSION_DELAY_PRE_HF16;
            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                steem_conversion_delay = STEEMIT_CONVERSION_DELAY;
            }

            this->db.template create<convert_request_object>([&](convert_request_object &obj) {
                obj.owner = o.owner;
                obj.request_id = o.request_id;
                obj.amount = delta;
                obj.conversion_date = this->db.head_block_time() + steem_conversion_delay;
            });

        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void limit_order_create_evaluator<Major, Hardfork, Release,
                type_traits::static_range<Hardfork <= 16>>::do_apply(const operation_type &o) {
            FC_ASSERT((o.amount_to_sell.symbol_name() == STEEM_SYMBOL_NAME &&
                       o.min_to_receive.symbol_name() == SBD_SYMBOL_NAME) ||
                      (o.amount_to_sell.symbol_name() == SBD_SYMBOL_NAME &&
                       o.min_to_receive.symbol_name() == STEEM_SYMBOL_NAME),
                      "Limit order must be for the STEEM:SBD market");

            FC_ASSERT(o.expiration > this->db.head_block_time(), "Limit order has to expire after head block time.");

            const auto &owner = this->db.get_account(o.owner);

            asset<0, 17, 0> delta(o.amount_to_sell.amount, o.amount_to_sell.symbol_name());

            FC_ASSERT(this->db.get_balance(owner, o.amount_to_sell.symbol_name()) >= delta,
                      "Account does not have sufficient funds for limit order.");

            this->db.adjust_balance(owner, -delta);

            const auto &order = this->db.template create<limit_order_object>([&](limit_order_object &obj) {
                obj.created = this->db.head_block_time();
                obj.seller = o.owner;
                obj.order_id = o.order_id;
                obj.for_sale = o.amount_to_sell.amount;
                obj.sell_price = protocol::price<0, 17, 0>(
                        protocol::asset<0, 17, 0>(o.get_price().base.amount, o.get_price().base.symbol_name()),
                        protocol::asset<0, 17, 0>(o.get_price().quote.amount, o.get_price().quote.symbol_name()));
                obj.expiration = o.expiration;
            });

            bool filled = this->db.apply_order(order);

            if (o.fill_or_kill) {
                FC_ASSERT(filled, "Cancelling order because it was not filled. ");
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void limit_order_create_evaluator<Major, Hardfork, Release,
                type_traits::static_range<Hardfork >= 17>>::do_apply(const operation_type &o) {
            try {
                FC_ASSERT(o.expiration >= this->db.head_block_time());

                seller = this->db.find_account(o.owner);
                sell_asset = this->db.find_asset(o.amount_to_sell.symbol_name());
                receive_asset = this->db.find_asset(o.min_to_receive.symbol_name());

                if (!sell_asset->options.whitelist_markets.empty()) {
                    FC_ASSERT(sell_asset->options.whitelist_markets.find(receive_asset->asset_name) !=
                              sell_asset->options.whitelist_markets.end());
                }
                if (!sell_asset->options.blacklist_markets.empty()) {
                    FC_ASSERT(sell_asset->options.blacklist_markets.find(receive_asset->asset_name) ==
                              sell_asset->options.blacklist_markets.end());
                }

                FC_ASSERT(this->db.is_authorized_asset(*seller, *sell_asset));
                FC_ASSERT(this->db.is_authorized_asset(*seller, *receive_asset));

                asset<0, 17, 0> delta(o.amount_to_sell.amount, o.amount_to_sell.symbol_name());

                FC_ASSERT(this->db.get_balance(*seller, *sell_asset) >= delta, "insufficient balance",
                          ("balance", this->db.get_balance(*seller, *sell_asset))("amount_to_sell", o.amount_to_sell));

            } FC_CAPTURE_AND_RETHROW((o))

            try {
                const auto &seller_stats = this->db.get_account_statistics(seller->name);
                this->db.template modify(seller_stats, [&](account_statistics_object &bal) {
                    if (o.amount_to_sell.symbol_name() == STEEM_SYMBOL_NAME) {
                        bal.total_core_in_orders += o.amount_to_sell.amount;
                    }
                });

                asset<0, 17, 0> delta(o.amount_to_sell.amount, o.amount_to_sell.symbol_name());

                this->db.adjust_balance(this->db.get_account(o.owner), -delta);

                bool filled = this->db.apply_order(
                        this->db.template create<limit_order_object>([&](limit_order_object &obj) {
                            obj.created = this->db.head_block_time();
                            obj.order_id = o.order_id;
                            obj.seller = seller->name;
                            obj.for_sale = o.amount_to_sell.amount;
                            obj.sell_price = protocol::price<0, 17, 0>(
                                    protocol::asset<0, 17, 0>(o.get_price().base.amount,
                                                              o.get_price().base.symbol_name()),
                                    protocol::asset<0, 17, 0>(o.get_price().quote.amount,
                                                              o.get_price().quote.symbol_name()));
                            obj.expiration = o.expiration;
                            obj.deferred_fee = deferred_fee;
                        }));

                FC_ASSERT(!o.fill_or_kill || filled);
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void limit_order_create2_evaluator<Major, Hardfork, Release,
                type_traits::static_range<Hardfork >= 17>>::do_apply(const operation_type &o) {
            try {
                FC_ASSERT(o.expiration >= this->db.head_block_time());

                seller = this->db.find_account(o.owner);
                sell_asset = this->db.find_asset(o.amount_to_sell.symbol_name());
                receive_asset = this->db.find_asset(o.exchange_rate.quote.symbol_name());

                if (!sell_asset->options.whitelist_markets.empty()) {
                    FC_ASSERT(sell_asset->options.whitelist_markets.find(receive_asset->asset_name) !=
                              sell_asset->options.whitelist_markets.end());
                }
                if (!sell_asset->options.blacklist_markets.empty()) {
                    FC_ASSERT(sell_asset->options.blacklist_markets.find(receive_asset->asset_name) ==
                              sell_asset->options.blacklist_markets.end());
                }

                FC_ASSERT(this->db.is_authorized_asset(*seller, *sell_asset));
                FC_ASSERT(this->db.is_authorized_asset(*seller, *receive_asset));

                asset<0, 17, 0> delta(o.amount_to_sell.amount, o.amount_to_sell.symbol_name());

                FC_ASSERT(this->db.get_balance(*seller, *sell_asset) >= delta, "insufficient balance",
                          ("balance", this->db.get_balance(*seller, *sell_asset))("amount_to_sell", o.amount_to_sell));

            } FC_CAPTURE_AND_RETHROW((o))

            try {
                const auto &seller_stats = this->db.get_account_statistics(seller->name);
                this->db.template modify(seller_stats, [&](account_statistics_object &bal) {
                    if (o.amount_to_sell.symbol_name() == STEEM_SYMBOL_NAME) {
                        bal.total_core_in_orders += o.amount_to_sell.amount;
                    }
                });

                asset<0, 17, 0> delta(o.amount_to_sell.amount, o.amount_to_sell.symbol_name());

                this->db.adjust_balance(this->db.get_account(o.owner), -delta);

                bool filled = this->db.apply_order(
                        this->db.template create<limit_order_object>([&](limit_order_object &obj) {
                            obj.created = this->db.template head_block_time();
                            obj.order_id = o.order_id;
                            obj.seller = seller->name;
                            obj.for_sale = o.amount_to_sell.amount;
                            obj.sell_price = protocol::price<0, 17, 0>(
                                    protocol::asset<0, 17, 0>(o.get_price().base.amount,
                                                              o.get_price().base.symbol_name()),
                                    protocol::asset<0, 17, 0>(o.get_price().quote.amount,
                                                              o.get_price().quote.symbol_name()));
                            obj.expiration = o.expiration;
                            obj.deferred_fee = deferred_fee;
                        }));

                FC_ASSERT(!o.fill_or_kill || filled);
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void limit_order_create2_evaluator<Major, Hardfork, Release,
                type_traits::static_range<Hardfork <= 16>>::do_apply(const operation_type &o) {
            FC_ASSERT((o.amount_to_sell.symbol_name() == STEEM_SYMBOL_NAME &&
                       o.exchange_rate.quote.symbol_name() == SBD_SYMBOL_NAME) ||
                      (o.amount_to_sell.symbol_name() == SBD_SYMBOL_NAME &&
                       o.exchange_rate.quote.symbol_name() == STEEM_SYMBOL_NAME),
                      "Limit order must be for the STEEM:SBD market");

            FC_ASSERT(o.expiration > this->db.head_block_time(), "Limit order has to expire after head block time.");

            const auto &owner = this->db.get_account(o.owner);

            asset<0, 17, 0> delta(o.amount_to_sell.amount, o.amount_to_sell.symbol_name());

            FC_ASSERT(this->db.template get_balance(owner, o.amount_to_sell.symbol_name()) >=
                      typename BOOST_IDENTITY_TYPE((protocol::asset<0, 17, 0>))(o.amount_to_sell.amount,
                                                                                o.amount_to_sell.symbol_name()),
                      "Account does not have sufficient funds for limit order.");

            this->db.template adjust_balance(owner, -protocol::asset<0, 17, 0>(o.amount_to_sell.amount,
                                                                               o.amount_to_sell.symbol_name()));

            const auto &order = this->db.template create<limit_order_object>([&](limit_order_object &obj) {
                obj.created = this->db.template head_block_time();
                obj.seller = o.owner;
                obj.order_id = o.order_id;
                obj.for_sale = o.amount_to_sell.amount;
                obj.sell_price = protocol::price<0, 17, 0>(
                        protocol::asset<0, 17, 0>(o.exchange_rate.base.amount, o.exchange_rate.base.symbol_name()),
                        protocol::asset<0, 17, 0>(o.exchange_rate.quote.amount, o.exchange_rate.quote.symbol_name()));
                obj.expiration = o.expiration;
            });

            bool filled = this->db.template apply_order(order);

            if (o.fill_or_kill) {
                FC_ASSERT(filled, "Cancelling order because it was not filled.");
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void limit_order_cancel_evaluator<Major, Hardfork, Release,
                type_traits::static_range<Hardfork >= 17>>::do_apply(const operation_type &op) {
            try {
                _order = this->db.template find_limit_order(op.owner, op.order_id);
                FC_ASSERT(_order->seller == op.owner);
            } FC_CAPTURE_AND_RETHROW((op))

            try {
                auto base_asset = _order->sell_price.base.symbol_name();
                auto quote_asset = _order->sell_price.quote.symbol_name();

                this->db.template cancel_order(*_order, false /* don't create a virtual op*/);

                // Possible optimization: order can be called by canceling a limit order iff the canceled order was at the top of the book.
                // Do I need to check calls in both assets?
                this->db.template check_call_orders(this->db.template get_asset(base_asset));
                this->db.template check_call_orders(this->db.template get_asset(quote_asset));
            } FC_CAPTURE_AND_RETHROW((op))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void limit_order_cancel_evaluator<Major, Hardfork, Release,
                type_traits::static_range<Hardfork <= 16>>::do_apply(const operation_type &op) {
            this->db.template cancel_order(this->db.template get_limit_order(op.owner, op.order_id), false);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void call_order_update_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &op) {
            try {
                _paying_account = this->db.template find_account(op.funding_account);
                _debt_asset = this->db.template find_asset(op.delta_debt.symbol_name());

                FC_ASSERT(_debt_asset->is_market_issued(),
                          "Unable to cover ${sym} as it is not a collateralized asset.",
                          ("sym", _debt_asset->asset_name));

                _bitasset_data = this->db.template find_asset_bitasset_data(_debt_asset->asset_name);

                /// if there is a settlement for this asset, then no further margin positions may be taken and
                /// all existing margin positions should have been closed va database::globally_settle_asset
                FC_ASSERT(!_bitasset_data->has_settlement());

                FC_ASSERT(op.delta_collateral.symbol_name() == _bitasset_data->options.short_backing_asset);

                if (_bitasset_data->is_prediction_market) {
                    FC_ASSERT(op.delta_collateral.amount == op.delta_debt.amount);
                } else if (_bitasset_data->current_feed.settlement_price.is_null()) {
                    FC_THROW_EXCEPTION(exceptions::chain::insufficient_feeds<>,
                                       "Cannot borrow asset with no price feed.");
                }

                if (op.delta_debt.amount < 0) {
                    FC_ASSERT(this->db.template get_balance(*_paying_account, *_debt_asset) >= op.delta_debt,
                              "Cannot cover by ${c} when payer only has ${b}", ("c", op.delta_debt.amount)("b",
                                                                                                           this->db.template get_balance(
                                                                                                                   *_paying_account,
                                                                                                                   *_debt_asset).amount));
                }

                if (op.delta_collateral.amount > 0) {
                    FC_ASSERT(this->db.template get_balance(*_paying_account, this->db.template get_asset(
                            _bitasset_data->options.short_backing_asset)) >= op.delta_collateral,
                              "Cannot increase collateral by ${c} when payer only has ${b}",
                              ("c", op.delta_collateral.amount)("b", this->db.template get_balance(*_paying_account,
                                                                                                   this->db.template get_asset(
                                                                                                           op.delta_collateral.symbol_name())).amount));
                }
            } FC_CAPTURE_AND_RETHROW((op))

            try {


                if (op.delta_debt.amount != 0) {
                    this->db.template adjust_balance(this->db.template get_account(op.funding_account), op.delta_debt);

                    // Deduct the debt paid from the total supply of the debt asset.
                    this->db.template modify(this->db.template get_asset_dynamic_data(_debt_asset->asset_name),
                                             [&](asset_dynamic_data_object &dynamic_asset) {
                                                 dynamic_asset.current_supply += op.delta_debt.amount;
                                                 assert(dynamic_asset.current_supply >= 0);
                                             });
                }

                if (op.delta_collateral.amount != 0) {
                    this->db.template adjust_balance(this->db.template get_account(op.funding_account),
                                                     -op.delta_collateral);

                    // Adjust the total core in orders accodingly
                    if (op.delta_collateral.symbol_name() == STEEM_SYMBOL_NAME) {
                        this->db.template modify(this->db.template get_account_statistics(_paying_account->name),
                                                 [&](account_statistics_object &stats) {
                                                     stats.total_core_in_orders += op.delta_collateral.amount;
                                                 });
                    }
                }


                auto &call_idx = this->db.template get_index<call_order_index>().indices().
                        template get<by_account>();
                auto itr = call_idx.find(boost::make_tuple(op.funding_account, op.delta_debt.symbol_name()));
                const call_order_object *call_obj = nullptr;

                if (itr == call_idx.end()) {
                    FC_ASSERT(op.delta_collateral.amount > 0);
                    FC_ASSERT(op.delta_debt.amount > 0);

                    call_obj = &this->db.template create<call_order_object>([&](call_order_object &call) {
                        call.order_id = op.order_id;
                        call.borrower = op.funding_account;
                        call.collateral = op.delta_collateral.amount;
                        call.debt = op.delta_debt.amount;
                        call.call_price = price<Major, Hardfork, Release>::call_price(op.delta_debt,
                                                                                      op.delta_collateral,
                                                                                      _bitasset_data->current_feed.maintenance_collateral_ratio);

                    });
                } else {
                    call_obj = &*itr;

                    this->db.template modify(*call_obj, [&](call_order_object &call) {
                        call.collateral += op.delta_collateral.amount;
                        call.debt += op.delta_debt.amount;
                        if (call.debt > 0) {
                            call.call_price = price<Major, Hardfork, Release>::call_price(call.get_debt(),
                                                                                          call.get_collateral(),
                                                                                          _bitasset_data->current_feed.maintenance_collateral_ratio);
                        }
                    });
                }

                auto debt = call_obj->get_debt();
                if (debt.amount == 0) {
                    FC_ASSERT(call_obj->collateral == 0);
                    this->db.template remove(*call_obj);
                    return void();
                }

                FC_ASSERT(call_obj->collateral > 0 && call_obj->debt > 0);

                // then we must check for margin calls and other issues
                if (!_bitasset_data->is_prediction_market) {
                    call_order_object::id_type call_order_id = call_obj->id;

                    // check to see if the order needs to be margin called now, but don't allow black swans and require there to be
                    // limit orders available that could be used to fill the order.
                    if (this->db.template check_call_orders(*_debt_asset, false)) {
                        const auto order_obj = this->db.template find<call_order_object, by_id>(call_order_id);
                        // if we filled at least one call order, we are OK if we totally filled. 
                        STEEMIT_ASSERT(!order_obj,
                                       typename BOOST_IDENTITY_TYPE((exceptions::operations::call_order_update::unfilled_margin_call<
                                               Major, Hardfork, Release >)),
                                       "Updating call order would trigger a margin call that cannot be fully filled",
                                       ("a", ~order_obj->call_price)("b",
                                                                     _bitasset_data->current_feed.settlement_price));
                    } else {
                        const auto order_obj = this->db.template find<call_order_object, by_id>(call_order_id);
                        FC_ASSERT(order_obj, "no margin call was executed and yet the call object was deleted");
                        //edump( (~order_obj->call_price) ("<")( _bitasset_data->current_feed.settlement_price) );
                        // We didn't fill any call orders.  This may be because we
                        // aren't in margin call territory, or it may be because there
                        // were no matching orders.  In the latter case, we throw.
                        STEEMIT_ASSERT(~order_obj->call_price < _bitasset_data->current_feed.settlement_price,
                                       typename BOOST_IDENTITY_TYPE((exceptions::operations::call_order_update::unfilled_margin_call<
                                               Major, Hardfork, Release >)),
                                       "Updating call order would trigger a margin call that cannot be fully filled",
                                       ("a", ~order_obj->call_price)("b",
                                                                     _bitasset_data->current_feed.settlement_price));
                    }
                }
            } FC_CAPTURE_AND_RETHROW((op))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void bid_collateral_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            const account_object &paying_account = this->db.template get_account(o.bidder);

            try {
                const asset_object &debt_asset = this->db.template get_asset(o.debt_covered.symbol_name());
                FC_ASSERT(debt_asset.is_market_issued(), "Unable to cover ${sym} as it is not a collateralized asset.",
                          ("sym", debt_asset.asset_name));

                const asset_bitasset_data_object &bitasset_data = this->db.template get_asset_bitasset_data(
                        debt_asset.asset_name);

                FC_ASSERT(bitasset_data.has_settlement());

                FC_ASSERT(o.additional_collateral.symbol_name() == bitasset_data.options.short_backing_asset);

                FC_ASSERT(!bitasset_data.is_prediction_market, "Cannot bid on a prediction market!");

                if (o.additional_collateral.amount > 0) {
                    FC_ASSERT(this->db.template get_balance(paying_account, this->db.template get_asset_bitasset_data(
                            bitasset_data.options.short_backing_asset).asset_name) >= o.additional_collateral,
                              "Cannot bid ${c} collateral when payer only has ${b}",
                              ("c", o.additional_collateral.amount)("b", this->db.template get_balance(paying_account,
                                                                                                       this->db.template get_asset(
                                                                                                               o.additional_collateral.symbol_name())).amount));
                }

                const auto &index = this->db.template get_index<collateral_bid_index>().indices().
                        template get<by_account>();
                const auto &bid = index.find(boost::make_tuple(o.debt_covered.symbol_name(), o.bidder));
                if (bid != index.end()) {
                    _bid = &(*bid);
                } else
                    FC_ASSERT(o.debt_covered.amount > 0, "Can't find bid to cancel?!");
            } FC_CAPTURE_AND_RETHROW((o))


            try {
                if (_bid != nullptr) {
                    this->db.template cancel_bid(*_bid, false);
                }

                if (o.debt_covered.amount == 0) {
                    return;
                }

                this->db.template adjust_balance(paying_account, -o.additional_collateral);
                _bid = &this->db.template create<collateral_bid_object>([&](collateral_bid_object &bid) {
                    bid.bidder = o.bidder;
                    bid.inv_swan_price = o.additional_collateral / o.debt_covered;
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }
    }
}

#include <golos/chain/evaluators/market_evaluator.tpp>