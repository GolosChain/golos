#include <golos/market_history/market_history_api.hpp>

#include <golos/chain/objects/steem_objects.hpp>

#include <golos/application/application.hpp>

#include <fc/crypto/bigint.hpp>
#include <fc/bloom_filter.hpp>

namespace golos {
    namespace market_history {

        namespace detail {

            class market_history_api_impl : public std::enable_shared_from_this<market_history_api_impl> {
            public:
                market_history_api_impl(golos::application::application &_app) : app(_app) {
                }

                struct operation_process_fill_order_visitor {
                    vector<optional<asset_object>> &assets;

                    operation_process_fill_order_visitor(vector<optional<asset_object>> &input_assets) : assets(input_assets) {
                    }

                    typedef market_trade result_type;

                    double price_to_real(const share_type a, int p) const {
                        return double(a.value) / std::pow(10, p);
                    };

                    template<typename T>
                    market_trade operator()(const T &o) const {return {};}

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                    market_trade operator()(const fill_order_operation<Major, Hardfork, Release> &o) const;

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                    market_trade operator()(const fill_call_order_operation<Major, Hardfork, Release> &o) const;

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                    market_trade operator()(const fill_settlement_order_operation<Major, Hardfork, Release> &o) const;
                };

                market_ticker get_ticker(const string &base, const string &quote) const;

                market_volume get_volume(const string &base, const string &quote) const;

                order_book get_order_book(const string &base, const string &quote, unsigned limit) const;

                vector<market_trade> get_trade_history(const string &base, const string &quote,
                                                       fc::time_point_sec start, fc::time_point_sec stop,
                                                       unsigned limit = 100) const;

                vector<order_history_object> get_fill_order_history(const string &a, const string &b,
                                                                    uint32_t limit) const;

                vector<bucket_object> get_market_history(const string &a, const string &b, uint32_t bucket_seconds,
                                                         fc::time_point_sec start, fc::time_point_sec end) const;

                flat_set<uint32_t> get_market_history_buckets() const;

                vector<limit_order_object> get_limit_orders(const string &a, const string &b, uint32_t limit) const;

                std::vector<golos::application::extended_limit_order> get_limit_orders_by_owner(
                        const string &owner) const;

                std::vector<call_order_object> get_call_orders_by_owner(const string &owner) const;

                std::vector<force_settlement_object> get_settle_orders_by_owner(const string &owner) const;

                vector<call_order_object> get_call_orders(const string &a, uint32_t limit) const;

                vector<force_settlement_object> get_settle_orders(const string &a, uint32_t limit) const;

                vector<call_order_object> get_margin_positions(const account_name_type &name) const;

                vector<collateral_bid_object> get_collateral_bids(const asset_name_type asset, uint32_t limit,
                                                                  uint32_t start, uint32_t skip) const;

                void subscribe_to_market(std::function<void(const variant &)> callback, string a, string b);

                void unsubscribe_from_market(string a, string b);

                std::vector<liquidity_balance> get_liquidity_queue(const string &start_account, uint32_t limit) const;

                vector<optional<asset_object>> lookup_asset_symbols(const vector<asset_name_type> &asset_symbols) const;

                // Subscriptions
                void set_subscribe_callback(std::function<void(const variant &)> cb, bool clear_filter);

                void set_pending_transaction_callback(std::function<void(const variant &)> cb);

                void set_block_applied_callback(std::function<void(const variant &block_id)> cb);

                void cancel_all_subscriptions();

                // signal handlers
                void on_applied_block(const chain::signed_block &b);

                mutable fc::bloom_filter _subscribe_filter;
                std::function<void(const fc::variant &)> _subscribe_callback;
                std::function<void(const fc::variant &)> _pending_trx_callback;
                std::function<void(const fc::variant &)> _block_applied_callback;

                boost::signals2::scoped_connection _block_applied_connection;

                map<pair<asset_name_type, asset_name_type>,
                        std::function<void(const variant &)>> _market_subscriptions;

                golos::application::application &app;
            };

            template<>
            market_trade market_history_api_impl::operation_process_fill_order_visitor::operator()<0, 16, 0>(const fill_order_operation<0, 16, 0> &o) const {
            market_trade trade;

            if (assets[0]->asset_name == o.open_pays.symbol_name()) {
            trade.amount = price_to_real(o.current_pays.amount, assets[1]->precision);
            trade.value = price_to_real(o.open_pays.amount, assets[0]->precision);
        } else {
        trade.amount = price_to_real(o.open_pays.amount, assets[1]->precision);
        trade.value = price_to_real(o.current_pays.amount, assets[0]->precision);
    }

    return trade;
}

template<>
market_trade market_history_api_impl::operation_process_fill_order_visitor::operator()<0, 17, 0>(const fill_order_operation<0, 17, 0> &o) const {
market_trade trade;

if (assets[0]->asset_name == o.open_pays.symbol_name()) {
trade.amount = price_to_real(o.current_pays.amount, assets[1]->precision);
trade.value = price_to_real(o.open_pays.amount, assets[0]->precision);
} else {
trade.amount = price_to_real(o.open_pays.amount, assets[1]->precision);
trade.value = price_to_real(o.current_pays.amount, assets[0]->precision);
}

return trade;
}

template<>
market_trade market_history_api_impl::operation_process_fill_order_visitor::operator()<0, 16, 0>(const fill_call_order_operation<0, 16, 0> &o) const{
market_trade trade;

if (assets[0]->asset_name == o.receives.symbol_name()) {
trade.amount = price_to_real(o.pays.amount, assets[1]->precision);
trade.value = price_to_real(o.receives.amount, assets[0]->precision);
} else {
trade.amount = price_to_real(o.receives.amount, assets[1]->precision);
trade.value = price_to_real(o.pays.amount, assets[0]->precision);
}

return trade;
}

template<>
market_trade market_history_api_impl::operation_process_fill_order_visitor::operator()<0, 17, 0>(const fill_call_order_operation<0, 17, 0> &o) const {
market_trade trade;

if (assets[0]->asset_name == o.receives.symbol_name()) {
trade.amount = price_to_real(o.pays.amount, assets[1]->precision);
trade.value = price_to_real(o.receives.amount, assets[0]->precision);
} else {
trade.amount = price_to_real(o.receives.amount, assets[1]->precision);
trade.value = price_to_real(o.pays.amount, assets[0]->precision);
}

return trade;
}

template<>
market_trade market_history_api_impl::operation_process_fill_order_visitor::operator()<0, 16, 0>(const fill_settlement_order_operation<0, 16, 0> &o) const {

market_trade trade;

if (assets[0]->asset_name == o.receives.symbol_name()) {
trade.amount = price_to_real(o.pays.amount, assets[1]->precision);
trade.value = price_to_real(o.receives.amount, assets[0]->precision);
} else {
trade.amount = price_to_real(o.receives.amount, assets[1]->precision);
trade.value = price_to_real(o.pays.amount, assets[0]->precision);
}

return trade;
}

template<>
market_trade market_history_api_impl::operation_process_fill_order_visitor::operator()<0, 17, 0>(const fill_settlement_order_operation<0, 17, 0> &o) const {
market_trade trade;

if (assets[0]->asset_name == o.receives.symbol_name()) {
trade.amount = price_to_real(o.pays.amount, assets[1]->precision);
trade.value = price_to_real(o.receives.amount, assets[0]->precision);
} else {
trade.amount = price_to_real(o.receives.amount, assets[1]->precision);
trade.value = price_to_real(o.pays.amount, assets[0]->precision);
}

return trade;
}

            //////////////////////////////////////////////////////////////////////
            //                                                                  //
            // Subscriptions                                                    //
            //                                                                  //
            //////////////////////////////////////////////////////////////////////

            void market_history_api_impl::set_subscribe_callback(std::function<void(const variant &)> cb,
                                                                 bool clear_filter) {
                _subscribe_callback = cb;
                if (clear_filter || !cb) {
                    static fc::bloom_parameters param;
                    param.projected_element_count = 10000;
                    param.false_positive_probability = 1.0 / 10000;
                    param.maximum_size = 1024 * 8 * 8 * 2;
                    param.compute_optimal_parameters();
                    _subscribe_filter = fc::bloom_filter(param);
                }
            }

            void market_history_api_impl::set_pending_transaction_callback(std::function<void(const variant &)> cb) {
                _pending_trx_callback = cb;
            }

            void market_history_api_impl::on_applied_block(const chain::signed_block &b) {
                try {
                    _block_applied_callback(fc::variant(signed_block_header(b)));
                } catch (...) {
                    _block_applied_connection.release();
                }
            }

            void market_history_api_impl::set_block_applied_callback(
                    std::function<void(const variant &block_header)> cb) {
                _block_applied_callback = cb;
                _block_applied_connection = golos::application::connect_signal(app.chain_database()->applied_block,
                                                                                 *this,
                                                                                 &market_history_api_impl::on_applied_block);
            }

            void market_history_api_impl::cancel_all_subscriptions() {
                set_subscribe_callback(std::function<void(const fc::variant &)>(), true);
            }

            void market_history_api_impl::subscribe_to_market(std::function<void(const variant &)> callback, string a,
                                                              string b) {
                if (a > b) {
                    std::swap(a, b);
                }
                FC_ASSERT(a != b);
                _market_subscriptions[std::make_pair(a, b)] = callback;
            }

            void market_history_api_impl::unsubscribe_from_market(string a, string b) {
                if (a > b) {
                    std::swap(a, b);
                }
                FC_ASSERT(a != b);
                _market_subscriptions.erase(std::make_pair(a, b));
            }

            vector<optional<asset_object>> market_history_api_impl::lookup_asset_symbols(const vector<asset_name_type> &asset_symbols) const {
                const auto &assets_by_symbol = app.chain_database()->get_index<asset_index>().indices().get<by_asset_name>();
                vector<optional<asset_object>> result;
                std::transform(asset_symbols.begin(), asset_symbols.end(), std::back_inserter(result),
                               [this, &assets_by_symbol](const vector<asset_name_type>::value_type &symbol) -> optional<asset_object> {
                                   auto itr = assets_by_symbol.find(symbol);
                                   return itr == assets_by_symbol.end() ? optional<asset_object>() : *itr;
                               });
                return result;
            }

            market_ticker market_history_api_impl::get_ticker(const string &base, const string &quote) const {
                const auto assets = lookup_asset_symbols({base, quote});
                FC_ASSERT(assets[0], "Invalid base asset symbol: ${s}", ("s", base));
                FC_ASSERT(assets[1], "Invalid quote asset symbol: ${s}", ("s", quote));

                market_ticker result;
                result.base = base;
                result.quote = quote;

                try {
                    const fc::time_point_sec now = fc::time_point::now();
                    const fc::time_point_sec yesterday = app.chain_database()->fetch_block_by_number(1)->timestamp <
                                                         fc::time_point_sec(now.sec_since_epoch() - 86400)
                                                         ? fc::time_point_sec(now.sec_since_epoch() - 86400)
                                                         : app.chain_database()->fetch_block_by_number(1)->timestamp;
                    const auto batch_size = 100;

                    vector<market_trade> trades = get_trade_history(base, quote, now, yesterday, batch_size);
                    if (!trades.empty()) {
                        result.latest = trades[0].price;

                        while (!trades.empty()) {
                            for (const market_trade &t: trades) {
                                result.base_volume += t.value;
                                result.quote_volume += t.amount;
                            }

                            trades = get_trade_history(base, quote, trades.back().date, yesterday, batch_size);
                        }

                        const auto last_trade_yesterday = get_trade_history(base, quote, yesterday,
                                                                            fc::time_point_sec(), 1);
                        if (!last_trade_yesterday.empty()) {
                            const auto price_yesterday = last_trade_yesterday[0].price;
                            result.percent_change = ((result.latest / price_yesterday) - 1) * 100;
                        }
                    } else {
                        const auto last_trade = get_trade_history(base, quote, now, fc::time_point_sec(), 1);
                        if (!last_trade.empty()) {
                            result.latest = last_trade[0].price;
                        }
                    }

                    const auto orders = get_order_book(base, quote, 1);
                    if (!orders.asks.empty()) {
                        result.lowest_ask = orders.asks[0].price;
                    }
                    if (!orders.bids.empty()) {
                        result.highest_bid = orders.bids[0].price;
                    }
                } FC_CAPTURE_AND_RETHROW((base)(quote))

                return result;
            }

            market_volume market_history_api_impl::get_volume(const string &base, const string &quote) const {
                const auto ticker = get_ticker(base, quote);

                market_volume result;
                result.base = ticker.base;
                result.quote = ticker.quote;
                result.base_volume = ticker.base_volume;
                result.quote_volume = ticker.quote_volume;

                return result;
            }

            order_book market_history_api_impl::get_order_book(const string &base, const string &quote,
                                                               unsigned limit) const {
                FC_ASSERT(limit <= 50);

                order_book result;
                result.base = base;
                result.quote = quote;

                auto assets = lookup_asset_symbols({base, quote});
                FC_ASSERT(assets[0], "Invalid base asset symbol: ${s}", ("s", base));
                FC_ASSERT(assets[1], "Invalid quote asset symbol: ${s}", ("s", quote));

                asset_name_type base_id = assets[0]->asset_name;
                auto orders = get_limit_orders(assets[0]->asset_name, assets[1]->asset_name, limit);


                std::function<double(const asset<0, 17, 0> &, int)> asset_to_real = [&](const asset<0, 17, 0> &a, int p) -> double {
                    return double(a.amount.value) / std::pow(10, p);
                };

                std::function<double(const price<0, 17, 0> &)> price_to_real = [&](const price<0, 17, 0> &p) -> double {
                    if (p.base.symbol == base_id) {
                        return asset_to_real(p.base, assets[0]->precision) /
                               asset_to_real(p.quote, assets[1]->precision);
                    }

                    return asset_to_real(p.quote, assets[0]->precision) / asset_to_real(p.base, assets[1]->precision);

                };

                for (const auto &o : orders) {
                    if (o.sell_price.base.symbol == base_id) {
                        order ord;
                        ord.price = price_to_real(o.sell_price);
                        ord.quote = asset_to_real(share_type(
                                (o.for_sale.value * o.sell_price.quote.amount.value) / o.sell_price.base.amount.value),
                                                  assets[1]->precision);
                        ord.base = asset_to_real(o.for_sale, assets[0]->precision);
                        result.bids.push_back(ord);
                    } else {
                        order ord;
                        ord.price = price_to_real(o.sell_price);
                        ord.quote = asset_to_real(o.for_sale, assets[1]->precision);
                        ord.base = asset_to_real(share_type(
                                (o.for_sale.value * o.sell_price.quote.amount.value) / o.sell_price.base.amount.value),
                                                 assets[0]->precision);
                        result.asks.push_back(ord);
                    }
                }

                return result;
            }

            std::vector<market_trade> market_history_api_impl::get_trade_history(const string &base,
                                                                                 const string &quote,
                                                                                 fc::time_point_sec start,
                                                                                 fc::time_point_sec stop,
                                                                                 unsigned limit) const {
                FC_ASSERT(limit <= 100);

                auto assets = lookup_asset_symbols({base, quote});
                FC_ASSERT(assets[0], "Invalid base asset symbol: ${s}", ("s", base));
                FC_ASSERT(assets[1], "Invalid quote asset symbol: ${s}", ("s", quote));

                auto base_id = assets[0]->asset_name;
                auto quote_id = assets[1]->asset_name;

                if (base_id > quote_id) {
                    std::swap(base_id, quote_id);
                }
                const auto &history_idx = app.chain_database()->get_index<order_history_index>().indices().get<by_key>();
                history_key hkey;
                hkey.base = base_id;
                hkey.quote = quote_id;
                hkey.sequence = std::numeric_limits<int64_t>::min();

                if (start.sec_since_epoch() == 0) {
                    start = fc::time_point_sec(fc::time_point::now());
                }

                uint32_t count = 0;
                auto itr = history_idx.lower_bound(hkey);
                vector<market_trade> result;

                while (itr != history_idx.end() && count < limit && !(itr->key.base != base_id || itr->key.quote != quote_id || itr->time < stop)) {
                    if (itr->time < start) {
                        market_trade trade = itr->op.visit(operation_process_fill_order_visitor(assets));

                        trade.date = itr->time;
                        trade.price = trade.value / trade.amount;

                        result.push_back(trade);
                        ++count;
                    }

                    // Trades are tracked in each direction.
                    ++itr;
                    ++itr;
                }

                return result;
            }

            vector<order_history_object> market_history_api_impl::get_fill_order_history(const string &a,
                                                                                         const string &b,
                                                                                         uint32_t limit) const {
                FC_ASSERT(app.chain_database());
                const auto &db = *app.chain_database();
                asset_name_type a_name = a, b_name = b;
                if (a_name > b_name) {
                    std::swap(a_name, b_name);
                }
                const auto &history_idx = db.get_index<golos::market_history::order_history_index>().indices().get<
                        by_key>();
                history_key hkey;
                hkey.base = a_name;
                hkey.quote = b_name;
                hkey.sequence = std::numeric_limits<int64_t>::min();

                uint32_t count = 0;
                auto itr = history_idx.lower_bound(hkey);
                vector<order_history_object> result;
                while (itr != history_idx.end() && count < limit) {
                    if (itr->key.base != a_name || itr->key.quote != b_name) {
                        break;
                    }
                    result.emplace_back(*itr);
                    ++itr;
                    ++count;
                }

                return result;
            }

            vector<bucket_object> market_history_api_impl::get_market_history(const std::string &a,
                                                                              const std::string &b,
                                                                              uint32_t bucket_seconds,
                                                                              fc::time_point_sec start,
                                                                              fc::time_point_sec end) const {
                try {
                    FC_ASSERT(app.chain_database());
                    const auto &db = *app.chain_database();
                    vector<bucket_object> result;
                    result.reserve(200);

                    asset_name_type a_name = a, b_name = b;
                    if (a_name > b_name) {
                        std::swap(a_name, b_name);
                    }

                    const auto &by_key_idx = db.get_index<bucket_index>().indices().get<by_key>();

                    auto itr = by_key_idx.lower_bound(bucket_key(a_name, b_name, bucket_seconds, start));
                    while (itr != by_key_idx.end() && itr->key.open <= end && result.size() < 200) {
                        if (!(itr->key.base == a_name && itr->key.quote == b_name &&
                              itr->key.seconds == bucket_seconds)) {
                            return result;
                        }
                        result.emplace_back(*itr);
                        ++itr;
                    }
                    return result;
                } FC_CAPTURE_AND_RETHROW((a)(b)(bucket_seconds)(start)(end))
            }

            flat_set<uint32_t> market_history_api_impl::get_market_history_buckets() const {
                auto buckets = app.get_plugin<market_history_plugin>(MARKET_HISTORY_PLUGIN_NAME)->get_tracked_buckets();
                return buckets;
            }

            /**
             *  @return the limit orders for both sides of the book for the two assets specified up to limit number on each side.
             */
            vector<limit_order_object> market_history_api_impl::get_limit_orders(const string &a, const string &b,
                                                                                 uint32_t limit) const {
                const auto &limit_order_idx = app.chain_database()->get_index<limit_order_index>();
                const auto &limit_price_idx = limit_order_idx.indices().get<by_price>();

                vector<limit_order_object> result;

                asset_name_type a_symbol = a;
                asset_name_type b_symbol = b;

                uint32_t count = 0;
                auto limit_itr = limit_price_idx.lower_bound(price<0, 17, 0>::max(a_symbol, b_symbol));
                auto limit_end = limit_price_idx.upper_bound(price<0, 17, 0>::min(a_symbol, b_symbol));
                while (limit_itr != limit_end && count < limit) {
                    result.push_back(*limit_itr);
                    ++limit_itr;
                    ++count;
                }
                count = 0;
                limit_itr = limit_price_idx.lower_bound(price<0, 17, 0>::max(b_symbol, a_symbol));
                limit_end = limit_price_idx.upper_bound(price<0, 17, 0>::min(b_symbol, a_symbol));
                while (limit_itr != limit_end && count < limit) {
                    result.push_back(*limit_itr);
                    ++limit_itr;
                    ++count;
                }

                return result;
            }

            vector<call_order_object> market_history_api_impl::get_call_orders(const string &a, uint32_t limit) const {
                const auto &call_index = app.chain_database()->get_index<call_order_index>().indices().get<by_price>();
                const asset_object &mia = app.chain_database()->get_asset(a);
                price<0, 17, 0> index_price = price<0, 17, 0>::min(app.chain_database()->get_asset_bitasset_data(mia.asset_name).options.short_backing_asset, mia.asset_name);

                return vector<call_order_object>(call_index.lower_bound(index_price.min()),
                                                 call_index.lower_bound(index_price.max()));
            }

            vector<force_settlement_object> market_history_api_impl::get_settle_orders(const string &a,
                                                                                       uint32_t limit) const {
                const auto &settle_index = app.chain_database()->get_index<force_settlement_index>().indices().get<
                        by_expiration>();
                const asset_object &mia = app.chain_database()->get_asset(a);
                return vector<force_settlement_object>(settle_index.lower_bound(mia.asset_name),
                                                       settle_index.upper_bound(mia.asset_name));
            }

            vector<call_order_object> market_history_api_impl::get_margin_positions(
                    const account_name_type &name) const {
                try {
                    const auto &idx = app.chain_database()->get_index<call_order_index>();
                    const auto &aidx = idx.indices().get<by_account>();
                    auto start = aidx.lower_bound(boost::make_tuple(name, STEEM_SYMBOL_NAME));
                    auto end = ++aidx.lower_bound(boost::make_tuple(name, STEEM_SYMBOL_NAME));
                    vector<call_order_object> result;
                    while (start != end) {
                        result.push_back(*start);
                        ++start;
                    }
                    return result;
                } FC_CAPTURE_AND_RETHROW((name))
            }

            vector<collateral_bid_object> market_history_api_impl::get_collateral_bids(const asset_name_type asset,
                                                                                       uint32_t limit, uint32_t start,
                                                                                       uint32_t skip) const {
                try {
                    FC_ASSERT(limit <= 100);
                    const asset_object &swan = app.chain_database()->get_asset(asset);
                    FC_ASSERT(swan.is_market_issued());
                    const asset_bitasset_data_object &bad = app.chain_database()->get_asset_bitasset_data(asset);
                    const asset_object &back = app.chain_database()->get_asset(bad.options.short_backing_asset);
                    const auto &idx = app.chain_database()->get_index<collateral_bid_index>();
                    const auto &aidx = idx.indices().get<by_price>();
                    auto start = aidx.lower_bound(boost::make_tuple(asset, price<0, 17, 0>::max(back.asset_name, asset),
                                                                    collateral_bid_object::id_type()));
                    auto end = aidx.lower_bound(boost::make_tuple(asset, price<0, 17, 0>::min(back.asset_name, asset),
                                                                  collateral_bid_object::id_type(
                                                                          STEEMIT_MAX_INSTANCE_ID)));
                    vector<collateral_bid_object> result;
                    while (skip-- > 0 && start != end) {
                        ++start;
                    }
                    while (start != end && limit-- > 0) {
                        result.emplace_back(*start);
                        ++start;
                    }
                    return result;
                } FC_CAPTURE_AND_RETHROW((asset)(limit)(skip))
            }

            std::vector<liquidity_balance> market_history_api_impl::get_liquidity_queue(const string &start_account,
                                                                                        uint32_t limit) const {
                FC_ASSERT(limit <= 1000);

                const auto &liq_idx = app.chain_database()->get_index<liquidity_reward_balance_index>().indices().get<
                        by_volume_weight>();
                auto itr = liq_idx.begin();
                std::vector<liquidity_balance> result;

                result.reserve(limit);

                if (start_account.length()) {
                    const auto &liq_by_acc = app.chain_database()->get_index<
                            liquidity_reward_balance_index>().indices().get<by_owner>();
                    auto acc = liq_by_acc.find(app.chain_database()->get_account(start_account).id);

                    if (acc != liq_by_acc.end()) {
                        itr = liq_idx.find(boost::make_tuple(acc->weight, acc->owner));
                    } else {
                        itr = liq_idx.end();
                    }
                }

                while (itr != liq_idx.end() && result.size() < limit) {
                    liquidity_balance bal;
                    bal.account = app.chain_database()->get(itr->owner).name;
                    bal.weight = itr->weight;
                    result.push_back(bal);

                    ++itr;
                }

                return result;
            }

            std::vector<golos::application::extended_limit_order> market_history_api_impl::get_limit_orders_by_owner(
                    const string &owner) const {
                std::vector<golos::application::extended_limit_order> result;
                const auto &idx = app.chain_database()->get_index<limit_order_index>().indices().get<by_account>();
                auto itr = idx.lower_bound(owner);
                while (itr != idx.end() && itr->seller == owner) {
                    result.emplace_back(*itr);

                    auto assets = lookup_asset_symbols(
                            {itr->sell_price.base.symbol, itr->sell_price.quote.symbol});
                    FC_ASSERT(assets[0], "Invalid base asset symbol: ${s}", ("s", itr->sell_price.base));
                    FC_ASSERT(assets[1], "Invalid quote asset symbol: ${s}", ("s", itr->sell_price.quote));

                    std::function<double(const share_type, int)> price_to_real = [&](const share_type a,
                                                                                     int p) -> double {
                        return double(a.value) / std::pow(10, p);
                    };

                    if (itr->sell_price.base.symbol == STEEM_SYMBOL_NAME) {
                        result.back().real_price =
                                price_to_real((~result.back().sell_price).base.amount, assets[0]->precision) /
                                price_to_real((~result.back().sell_price).quote.amount, assets[1]->precision);
                    } else {
                        result.back().real_price =
                                price_to_real(result.back().sell_price.base.amount, assets[0]->precision) /
                                price_to_real(result.back().sell_price.quote.amount, assets[1]->precision);
                    }

                    ++itr;
                }
                return result;
            }

            std::vector<call_order_object> market_history_api_impl::get_call_orders_by_owner(
                    const string &owner) const {
                std::vector<call_order_object> result;
                const auto &idx = app.chain_database()->get_index<call_order_index>().indices().get<by_account>();
                auto itr = idx.lower_bound(owner);
                while (itr != idx.end() && itr->borrower == owner) {
                    result.emplace_back(*itr);
                    ++itr;
                }
                return result;
            }

            std::vector<force_settlement_object> market_history_api_impl::get_settle_orders_by_owner(
                    const string &owner) const {
                std::vector<force_settlement_object> result;
                const auto &idx = app.chain_database()->get_index<force_settlement_index>().indices().get<by_account>();
                auto itr = idx.lower_bound(owner);
                while (itr != idx.end() && itr->owner == owner) {
                    result.emplace_back(*itr);
                    ++itr;
                }
                return result;
            }

        } // detail

        market_history_api::market_history_api(const golos::application::api_context &ctx) {
            my = std::make_shared<detail::market_history_api_impl>(ctx.app);
        }

        void market_history_api::on_api_startup() {
        }

        market_ticker market_history_api::get_ticker(const string &base, const string &quote) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_ticker(base, quote);
            });
        }

        market_volume market_history_api::get_volume(const string &base, const string &quote) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_volume(base, quote);
            });
        }

        order_book market_history_api::get_order_book(const string &base, const string &quote, unsigned limit) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_order_book(base, quote, limit);
            });
        }

        std::vector<market_trade> market_history_api::get_trade_history(const string &base, const string &quote,
                                                                        fc::time_point_sec start,
                                                                        fc::time_point_sec stop, unsigned limit) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_trade_history(base, quote, start, stop, limit);
            });
        }

        vector<order_history_object> market_history_api::get_fill_order_history(const string &a, const string &b,
                                                                                uint32_t limit) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_fill_order_history(a, b, limit);
            });
        }

        vector<bucket_object> market_history_api::get_market_history(const string &a, const string &b,
                                                                     uint32_t bucket_seconds, fc::time_point_sec start,
                                                                     fc::time_point_sec end) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_market_history(a, b, bucket_seconds, start, end);
            });
        }

        flat_set<uint32_t> market_history_api::get_market_history_buckets() const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_market_history_buckets();
            });
        }


        vector<limit_order_object> market_history_api::get_limit_orders(const string &a, const string &b,
                                                                        uint32_t limit) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_limit_orders(a, b, limit);
            });
        }

        vector<call_order_object> market_history_api::get_call_orders(const string &a, uint32_t limit) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_call_orders(a, limit);
            });
        }

        vector<call_order_object> market_history_api::get_margin_positions(const string &name) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_margin_positions(name);
            });
        }

        vector<force_settlement_object> market_history_api::get_settle_orders(const string &a, uint32_t limit) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_settle_orders(a, limit);
            });
        }

        std::vector<liquidity_balance> market_history_api::get_liquidity_queue(const string &start_account,
                                                                               uint32_t limit) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_liquidity_queue(start_account, limit);
            });
        }

        void market_history_api::cancel_all_subscriptions() {
            my->app.chain_database()->with_read_lock([&]() {
                my->cancel_all_subscriptions();
            });
        }

        void market_history_api::set_subscribe_callback(std::function<void(const variant &)> cb, bool clear_filter) {
            my->app.chain_database()->with_read_lock([&]() {
                my->set_subscribe_callback(cb, clear_filter);
            });
        }

        void market_history_api::set_pending_transaction_callback(std::function<void(const variant &)> cb) {
            my->app.chain_database()->with_read_lock([&]() {
                my->set_pending_transaction_callback(cb);
            });
        }

        void market_history_api::set_block_applied_callback(std::function<void(const variant &block_id)> cb) {
            my->app.chain_database()->with_read_lock([&]() {
                my->set_block_applied_callback(cb);
            });
        }

        void market_history_api::unsubscribe_from_market(const string &a, const string &b) {
            my->unsubscribe_from_market(a, b);
        }

        std::vector<golos::application::extended_limit_order> market_history_api::get_limit_orders_by_owner(
                const string &owner) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_limit_orders_by_owner(owner);
            });
        }

        std::vector<call_order_object> market_history_api::get_call_orders_by_owner(const string &owner) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_call_orders_by_owner(owner);
            });
        }

        std::vector<force_settlement_object> market_history_api::get_settle_orders_by_owner(const string &owner) const {
            return my->app.chain_database()->with_read_lock([&]() {
                return my->get_settle_orders_by_owner(owner);
            });
        }

        void market_history_api::subscribe_to_market(std::function<void(const variant &)> callback, const string &a,
                                                     const string &b) {
            my->subscribe_to_market(std::move(callback), a, b);
        }

        vector<collateral_bid_object> market_history_api::get_collateral_bids(const asset_name_type asset,
                                                                              uint32_t limit, uint32_t start) const {
            return my->get_collateral_bids(asset, limit, start, 0);
        }
    }
} // golos::market_history