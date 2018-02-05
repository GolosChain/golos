#include <golos/account_statistics/account_statistics_api.hpp>

namespace golos {
    namespace account_statistics {

        namespace detail {
            class account_statistics_api_impl {
            public:
                account_statistics_api_impl(golos::application::application &app)
                        : _app(app) {
                }

                golos::application::application &_app;
            };
        } // detail

        account_statistics_api::account_statistics_api(const golos::application::api_context &ctx) {
            _my = std::make_shared<detail::account_statistics_api_impl>(ctx.app);
        }

        void account_statistics_api::on_api_startup() {
        }

    }
} // golos::account_statistics