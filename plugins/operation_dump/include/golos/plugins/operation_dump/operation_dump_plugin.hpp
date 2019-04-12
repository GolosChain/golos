#pragma once

#include <appbase/plugin.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/plugins/chain/plugin.hpp>

namespace golos { namespace plugins { namespace operation_dump {

namespace bpo = boost::program_options;
using namespace golos::chain;

class operation_dump_plugin final : public appbase::plugin<operation_dump_plugin> {
public:
    APPBASE_PLUGIN_REQUIRES((chain::plugin))

    operation_dump_plugin();

    ~operation_dump_plugin();

    void set_program_options(bpo::options_description& cli, bpo::options_description& cfg) override;

    void plugin_initialize(const bpo::variables_map& options) override;

    void plugin_startup() override;

    void plugin_shutdown() override;

    static const std::string& name();

private:
    class operation_dump_plugin_impl;

    std::unique_ptr<operation_dump_plugin_impl> my;
};

} } } //golos::plugins::operation_dump
