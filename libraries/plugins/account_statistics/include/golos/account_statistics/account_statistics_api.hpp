#pragma once

#include <golos/account_statistics/account_statistics_plugin.hpp>

#include <fc/api.hpp>

namespace golos {
    namespace app {
        struct api_context;
    }
}

namespace golos {
    namespace account_statistics {

        namespace detail {
            class account_statistics_api_impl;
        }

        class account_statistics_api {
        public:
            account_statistics_api(const golos::application::api_context &ctx);

            void on_api_startup();

        private:
            std::shared_ptr<detail::account_statistics_api_impl> _my;
        };

    }
} // golos::account_statistics

FC_API(golos::account_statistics::account_statistics_api,)