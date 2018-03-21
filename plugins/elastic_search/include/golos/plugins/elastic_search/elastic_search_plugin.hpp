#pragma once
#include <golos/plugins/elastic_search/elastic_search_objects.hpp>

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/json_rpc/utility.hpp>
#include <golos/plugins/chain/plugin.hpp>


namespace golos {
namespace plugins {
namespace elastic_search {

    // Stores account history data in elasticsearch database
    class elastic_search_plugin final : public appbase::plugin<elastic_search_plugin> {
        public:

        APPBASE_PLUGIN_REQUIRES((chain::plugin))

        elastic_search_plugin();

        virtual ~elastic_search_plugin();

        void set_program_options(
                boost::program_options::options_description &cli,
                boost::program_options::options_description &cfg) override;

        void plugin_initialize(const boost::program_options::variables_map &options) override;

        void plugin_startup() override;

        void plugin_shutdown() override;

        constexpr const static char *plugin_name = "elastic_search";

        static const std::string &name() {
            static std::string name = plugin_name;
            return name;
        }

        private:
            class elastic_search_plugin_impl;

            std::unique_ptr<elastic_search_plugin_impl> _my;
        };

}}} // namespace golos::plugins::elastic_search
