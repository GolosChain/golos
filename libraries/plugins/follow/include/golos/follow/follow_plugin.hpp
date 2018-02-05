#pragma once

#include <golos/application/plugin.hpp>
#include <golos/chain/database.hpp>

#include <fc/thread/future.hpp>


namespace golos {
    namespace follow {
        using golos::application::application;

#define FOLLOW_PLUGIN_NAME "follow"

        namespace detail { class follow_plugin_impl; }

        class follow_plugin : public golos::application::plugin {
        public:
            follow_plugin(application *app);

            std::string plugin_name() const override {
                return FOLLOW_PLUGIN_NAME;
            }

            virtual void plugin_set_program_options(
                    boost::program_options::options_description &cli,
                    boost::program_options::options_description &cfg) override;

            virtual void plugin_initialize(const boost::program_options::variables_map &options) override;

            virtual void plugin_startup() override;

            friend class detail::follow_plugin_impl;

            std::unique_ptr<detail::follow_plugin_impl> my;
            uint32_t max_feed_size = 500;
        };

    }
} //golos::follow
