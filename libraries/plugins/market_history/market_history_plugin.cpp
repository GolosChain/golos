#include <golos/market_history/market_history_api.hpp>

#include <golos/chain/operation_notification.hpp>

namespace golos {
    namespace market_history {

        namespace detail {

            class market_history_plugin_impl {
            public:
                market_history_plugin_impl(market_history_plugin &plugin) : _self(plugin) {
                }

                struct operation_process_fill_order {
                    market_history_plugin &_plugin;
                    fc::time_point_sec _now;

                    operation_process_fill_order(market_history_plugin &mhp, fc::time_point_sec n) : _plugin(mhp),
                                                                                                     _now(n) {
                    }

                    typedef void result_type;

                    /** do nothing for other operation types */
                    template<typename T>
                    void operator()(const T &) const {

                    }

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                    void operator()(const fill_order_operation<Major, Hardfork, Release> &o) const {
                        //ilog( "processing ${o}", ("o",o) );
                        const auto &buckets = _plugin.get_tracked_buckets();
                        auto &db = _plugin.database();
                        const auto &bucket_idx = db.get_index<bucket_index>();
                        const auto &history_idx = db.get_index<order_history_index>().indices().get<by_key>();

                        auto time = db.head_block_time();

                        history_key hkey;
                        hkey.base = o.current_pays.symbol_name();
                        hkey.quote = o.open_pays.symbol_name();
                        if (hkey.base > hkey.quote) {
                            std::swap(hkey.base, hkey.quote);
                        }
                        hkey.sequence = std::numeric_limits<int64_t>::min();

                        auto itr = history_idx.lower_bound(hkey);

                        if (itr->key.base == hkey.base && itr->key.quote == hkey.quote) {
                            hkey.sequence = itr->key.sequence - 1;
                        } else {
                            hkey.sequence = 0;
                        }

                        db.create<order_history_object>([&](order_history_object &ho) {
                            ho.key = hkey;
                            ho.time = time;
                            ho.op = o;
                        });

                        hkey.sequence += 200;
                        itr = history_idx.lower_bound(hkey);
                        /*
                        while( itr != history_idx.end() )
                        {
                           if( itr->key.base == hkey.base && itr->key.quote == hkey.quote )
                           {
                              db.remove( *itr );
                              itr = history_idx.lower_bound( hkey );
                           }
                           else break;
                        }
                        */


                        auto max_history = _plugin.get_max_history_per_bucket();
                        for (const auto bucket : buckets) {
                            auto cutoff = (fc::time_point() + fc::seconds(bucket * max_history));

                            bucket_key key;
                            key.base = o.current_pays.symbol_name();
                            key.quote = o.open_pays.symbol_name();


                            /** for every matched order there are two fill order operations created, one for
                             * each side.  We can filter the duplicates by only considering the fill operations where
                             * the base > quote
                             */
                            if (key.base > key.quote) {
                                //ilog( "     skipping because base > quote" );
                                continue;
                            }

                            price<Major, Hardfork, Release> v_trade_price = (o.current_pays / o.open_pays);

                            price<0, 17, 0> trade_price(asset<0, 17, 0>(v_trade_price.base.template amount, v_trade_price.base.template symbol_name()), asset<0, 17, 0>(v_trade_price.quote.template amount, v_trade_price.quote.template symbol_name()));

                            key.seconds = bucket;
                            key.open = fc::time_point() +
                                       fc::seconds((_now.sec_since_epoch() / key.seconds) * key.seconds);

                            const auto &by_key_idx = bucket_idx.indices().get<by_key>();
                            auto itr = by_key_idx.find(key);
                            if (itr == by_key_idx.end()) { // create new bucket
                                /* const auto& obj = */
                                db.create<bucket_object>([&](bucket_object &b) {
                                    b.key = key;
                                    b.quote_volume += trade_price.quote.amount;
                                    b.base_volume += trade_price.base.amount;
                                    b.open_base = trade_price.base.amount;
                                    b.open_quote = trade_price.quote.amount;
                                    b.close_base = trade_price.base.amount;
                                    b.close_quote = trade_price.quote.amount;
                                    b.high_base = b.close_base;
                                    b.high_quote = b.close_quote;
                                    b.low_base = b.close_base;
                                    b.low_quote = b.close_quote;
                                });
                                //wlog( "    creating bucket ${b}", ("b",obj) );
                            } else { // update existing bucket
                                //wlog( "    before updating bucket ${b}", ("b",*itr) );
                                db.modify(*itr, [&](bucket_object &b) {
                                    b.base_volume += trade_price.base.amount;
                                    b.quote_volume += trade_price.quote.amount;
                                    b.close_base = trade_price.base.amount;
                                    b.close_quote = trade_price.quote.amount;
                                    if (b.high() < trade_price) {
                                        b.high_base = b.close_base;
                                        b.high_quote = b.close_quote;
                                    }
                                    if (b.low() > trade_price) {
                                        b.low_base = b.close_base;
                                        b.low_quote = b.close_quote;
                                    }
                                });
                                //wlog( "    after bucket bucket ${b}", ("b",*itr) );
                            }

                            if (max_history != 0) {
                                key.open = fc::time_point_sec();
                                auto itr = by_key_idx.lower_bound(key);

                                while (itr != by_key_idx.end() && itr->key.base == key.base &&
                                       itr->key.quote == key.quote && itr->key.seconds == bucket &&
                                       itr->key.open < cutoff) {
                                    //  elog( "    removing old bucket ${b}", ("b", *itr) );
                                    auto old_itr = itr;
                                    ++itr;
                                    db.remove(*old_itr);
                                }
                            }
                        }
                    }

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                    void operator()(const fill_call_order_operation<Major, Hardfork, Release> &o) const {
                        //ilog( "processing ${o}", ("o",o) );
                        const auto &buckets = _plugin.get_tracked_buckets();
                        auto &db = _plugin.database();
                        const auto &bucket_idx = db.get_index<bucket_index>();
                        const auto &history_idx = db.get_index<order_history_index>().indices().get<by_key>();

                        auto time = db.head_block_time();

                        history_key hkey;
                        hkey.base = o.pays.symbol_name();
                        hkey.quote = o.receives.symbol_name();
                        if (hkey.base > hkey.quote)
                            std::swap(hkey.base, hkey.quote);
                        hkey.sequence = std::numeric_limits<int64_t>::min();

                        auto itr = history_idx.lower_bound(hkey);

                        if (itr->key.base == hkey.base && itr->key.quote == hkey.quote)
                            hkey.sequence = itr->key.sequence - 1;
                        else
                            hkey.sequence = 0;

                        db.create<order_history_object>([&](order_history_object &ho) {
                            ho.key = hkey;
                            ho.time = time;
                            ho.op = o;
                        });

                        hkey.sequence += 200;
                        itr = history_idx.lower_bound(hkey);

                        auto max_history = _plugin.get_max_history_per_bucket();
                        for (auto bucket : buckets) {
                            auto cutoff = (fc::time_point() + fc::seconds(bucket * max_history));

                            bucket_key key;
                            key.base = o.pays.symbol_name();
                            key.quote = o.receives.symbol_name();


                            /** for every matched order there are two fill order operations created, one for
                             * each side.  We can filter the duplicates by only considering the fill operations where
                             * the base > quote
                             */
                            if (key.base > key.quote) {
                                //ilog( "     skipping because base > quote" );
                                continue;
                            }

                            price<0, 17, 0> trade_price = o.pays / o.receives;

                            key.seconds = bucket;
                            key.open = fc::time_point() +
                                       fc::seconds((_now.sec_since_epoch() / key.seconds) * key.seconds);

                            const auto &by_key_idx = bucket_idx.indices().get<by_key>();
                            auto itr = by_key_idx.find(key);
                            if (itr == by_key_idx.end()) { // create new bucket
                                /* const auto& obj = */
                                db.create<bucket_object>([&](bucket_object &b) {
                                    b.key = key;
                                    b.quote_volume += trade_price.quote.amount;
                                    b.base_volume += trade_price.base.amount;
                                    b.open_base = trade_price.base.amount;
                                    b.open_quote = trade_price.quote.amount;
                                    b.close_base = trade_price.base.amount;
                                    b.close_quote = trade_price.quote.amount;
                                    b.high_base = b.close_base;
                                    b.high_quote = b.close_quote;
                                    b.low_base = b.close_base;
                                    b.low_quote = b.close_quote;
                                });
                                //wlog( "    creating bucket ${b}", ("b",obj) );
                            } else { // update existing bucket
                                //wlog( "    before updating bucket ${b}", ("b",*itr) );
                                db.modify(*itr, [&](bucket_object &b) {
                                    b.base_volume += trade_price.base.amount;
                                    b.quote_volume += trade_price.quote.amount;
                                    b.close_base = trade_price.base.amount;
                                    b.close_quote = trade_price.quote.amount;
                                    if (b.high() < trade_price) {
                                        b.high_base = b.close_base;
                                        b.high_quote = b.close_quote;
                                    }
                                    if (b.low() > trade_price) {
                                        b.low_base = b.close_base;
                                        b.low_quote = b.close_quote;
                                    }
                                });
                                //wlog( "    after bucket bucket ${b}", ("b",*itr) );
                            }

                            if (max_history != 0) {
                                key.open = fc::time_point_sec();
                                auto itr = by_key_idx.lower_bound(key);

                                while (itr != by_key_idx.end() && itr->key.base == key.base &&
                                       itr->key.quote == key.quote && itr->key.seconds == bucket &&
                                       itr->key.open < cutoff) {
                                    //  elog( "    removing old bucket ${b}", ("b", *itr) );
                                    auto old_itr = itr;
                                    ++itr;
                                    db.remove(*old_itr);
                                }
                            }
                        }
                    }

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                    void operator()(const fill_settlement_order_operation<Major, Hardfork, Release> &o) const {
                        //ilog( "processing ${o}", ("o",o) );
                        const auto &buckets = _plugin.get_tracked_buckets();
                        auto &db = _plugin.database();
                        const auto &bucket_idx = db.get_index<bucket_index>();
                        const auto &history_idx = db.get_index<order_history_index>().indices().get<by_key>();

                        auto time = db.head_block_time();

                        history_key hkey;
                        hkey.base = o.pays.symbol_name();
                        hkey.quote = o.receives.symbol_name();
                        if (hkey.base > hkey.quote)
                            std::swap(hkey.base, hkey.quote);
                        hkey.sequence = std::numeric_limits<int64_t>::min();

                        auto itr = history_idx.lower_bound(hkey);

                        if (itr->key.base == hkey.base && itr->key.quote == hkey.quote)
                            hkey.sequence = itr->key.sequence - 1;
                        else
                            hkey.sequence = 0;

                        db.create<order_history_object>([&](order_history_object &ho) {
                            ho.key = hkey;
                            ho.time = time;
                            ho.op = o;
                        });

                        hkey.sequence += 200;
                        itr = history_idx.lower_bound(hkey);

                        auto max_history = _plugin.get_max_history_per_bucket();
                        for (auto bucket : buckets) {
                            auto cutoff = (fc::time_point() + fc::seconds(bucket * max_history));

                            bucket_key key;
                            key.base = o.pays.symbol_name();
                            key.quote = o.receives.symbol_name();


                            /** for every matched order there are two fill order operations created, one for
                             * each side.  We can filter the duplicates by only considering the fill operations where
                             * the base > quote
                             */
                            if (key.base > key.quote) {
                                //ilog( "     skipping because base > quote" );
                                continue;
                            }

                            price<0, 17, 0> trade_price = o.pays / o.receives;

                            key.seconds = bucket;
                            key.open = fc::time_point() +
                                       fc::seconds((_now.sec_since_epoch() / key.seconds) * key.seconds);

                            const auto &by_key_idx = bucket_idx.indices().get<by_key>();
                            auto itr = by_key_idx.find(key);
                            if (itr == by_key_idx.end()) { // create new bucket
                                /* const auto& obj = */
                                db.create<bucket_object>([&](bucket_object &b) {
                                    b.key = key;
                                    b.quote_volume += trade_price.quote.amount;
                                    b.base_volume += trade_price.base.amount;
                                    b.open_base = trade_price.base.amount;
                                    b.open_quote = trade_price.quote.amount;
                                    b.close_base = trade_price.base.amount;
                                    b.close_quote = trade_price.quote.amount;
                                    b.high_base = b.close_base;
                                    b.high_quote = b.close_quote;
                                    b.low_base = b.close_base;
                                    b.low_quote = b.close_quote;
                                });
                                //wlog( "    creating bucket ${b}", ("b",obj) );
                            } else { // update existing bucket
                                //wlog( "    before updating bucket ${b}", ("b",*itr) );
                                db.modify(*itr, [&](bucket_object &b) {
                                    b.base_volume += trade_price.base.amount;
                                    b.quote_volume += trade_price.quote.amount;
                                    b.close_base = trade_price.base.amount;
                                    b.close_quote = trade_price.quote.amount;
                                    if (b.high() < trade_price) {
                                        b.high_base = b.close_base;
                                        b.high_quote = b.close_quote;
                                    }
                                    if (b.low() > trade_price) {
                                        b.low_base = b.close_base;
                                        b.low_quote = b.close_quote;
                                    }
                                });
                                //wlog( "    after bucket bucket ${b}", ("b",*itr) );
                            }

                            if (max_history != 0) {
                                key.open = fc::time_point_sec();
                                auto itr = by_key_idx.lower_bound(key);

                                while (itr != by_key_idx.end() && itr->key.base == key.base &&
                                       itr->key.quote == key.quote && itr->key.seconds == bucket &&
                                       itr->key.open < cutoff) {
                                    //  elog( "    removing old bucket ${b}", ("b", *itr) );
                                    auto old_itr = itr;
                                    ++itr;
                                    db.remove(*old_itr);
                                }
                            }
                        }
                    }
                };

                virtual ~market_history_plugin_impl() {
                }

                /**
                 * This method is called as a callback after a block is applied
                 * and will process/index all operations that were applied in the block.
                 */
                void update_market_histories(const operation_notification &o);

                market_history_plugin &_self;
                flat_set<uint32_t> _tracked_buckets = flat_set<uint32_t>  {15, 60, 300, 3600, 86400};
                int32_t maximum_history_per_bucket_size = 1000;
            };

            void market_history_plugin_impl::update_market_histories(const operation_notification &o) {
                if (maximum_history_per_bucket_size == 0) {
                    return;
                }
                if (_tracked_buckets.size() == 0) {
                    return;
                }

                o.op.visit(operation_process_fill_order(_self, fc::time_point::now()));
            }

        } // detail

        market_history_plugin::market_history_plugin(application *app) : plugin(app),
                                                                         _my(new detail::market_history_plugin_impl(
                                                                                 *this)) {
        }

        market_history_plugin::~market_history_plugin() {
        }

        void market_history_plugin::plugin_set_program_options(boost::program_options::options_description &cli,
                                                               boost::program_options::options_description &cfg) {
            cli.add_options()("market-history-bucket-size",
                              boost::program_options::value<string>()->default_value("[15,60,300,3600,86400]"),
                              "Track market history by grouping orders into buckets of equal size measured in seconds specified as a JSON array of numbers")(
                    "market-history-buckets-per-size", boost::program_options::value<uint32_t>()->default_value(5760),
                    "How far back in time to track history for each bucket size, measured in the number of buckets (default: 5760)");
            cfg.add(cli);
        }

        void market_history_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
            try {
                ilog("market_history: plugin_initialize() begin");
                chain::database &db = database();

                db.post_apply_operation.connect([&](const operation_notification &o) {
                    _my->update_market_histories(o);
                });

                db.add_plugin_index<bucket_index>();
                db.add_plugin_index<order_history_index>();

                if (options.count("bucket-size")) {
                    std::string buckets = options["bucket-size"].as<string>();
                    _my->_tracked_buckets = fc::json::from_string(buckets).as<flat_set<uint32_t>>();
                }
                if (options.count("history-per-size")) {
                    _my->maximum_history_per_bucket_size = options["history-per-size"].as<uint32_t>();
                }

                wlog("bucket-size ${b}", ("b", _my->_tracked_buckets));
                wlog("history-per-size ${h}", ("h", _my->maximum_history_per_bucket_size));

                ilog("market_history: plugin_initialize() end");
            } FC_CAPTURE_AND_RETHROW()
        }

        void market_history_plugin::plugin_startup() {
            ilog("market_history plugin: plugin_startup() begin");

            app().register_api_factory<market_history_api>("market_history_api");

            ilog("market_history plugin: plugin_startup() end");
        }

        flat_set<uint32_t> market_history_plugin::get_tracked_buckets() const {
            return _my->_tracked_buckets;
        }

        uint32_t market_history_plugin::get_max_history_per_bucket() const {
            return _my->maximum_history_per_bucket_size;
        }
    }
} // golos::market_history

STEEMIT_DEFINE_PLUGIN(market_history, golos::market_history::market_history_plugin)
