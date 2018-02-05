#pragma once

#include <golos/application/plugin.hpp>

#include <golos/chain/steem_object_types.hpp>

#include <golos/market_history/bucket_object.hpp>
#include <golos/market_history/order_history_object.hpp>

#ifndef MARKET_HISTORY_PLUGIN_NAME
#define MARKET_HISTORY_PLUGIN_NAME "market_history"
#endif


namespace golos {
    namespace market_history {

        using namespace chain;
        using golos::application::application;

        namespace detail {
            class market_history_plugin_impl;
        }

        /**
         *  The market history plugin can be configured to track any number of intervals via its configuration.  Once         per block it
         *  will scan the virtual operations and look for fill_order and fill_asset_order operations and then adjust the appropriate bucket objects for
         *  each fill order.
         */

        class market_history_plugin : public golos::application::plugin {
        public:
            market_history_plugin(application *app);

            virtual ~market_history_plugin();

            virtual std::string plugin_name() const override {
                return MARKET_HISTORY_PLUGIN_NAME;
            }

            virtual void plugin_set_program_options(
                    boost::program_options::options_description &cli,
                    boost::program_options::options_description &cfg) override;

            virtual void plugin_initialize(const boost::program_options::variables_map &options) override;

            virtual void plugin_startup() override;

            flat_set<uint32_t> get_tracked_buckets() const;

            uint32_t get_max_history_per_bucket() const;

        private:
            friend class detail::market_history_plugin_impl;

            std::unique_ptr<detail::market_history_plugin_impl> _my;
        };
    }
} // golos::market_history
