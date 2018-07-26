#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/protocol/operation_util_impl.hpp>

namespace golos {
    namespace plugins {
        namespace follow {

            void follow_operation::validate() const {
                PLUGIN_CHECK_LOGIC(follower != following,
                        logic_errors::cannot_follow_yourself,
                        "You cannot follow yourself");
            }

            void reblog_operation::validate() const {
                PLUGIN_CHECK_LOGIC(account != author, 
                        logic_errors::cannot_reblog_own_content,
                        "You cannot reblog your own content");
            }

        }
    }
} //golos::follow

DEFINE_OPERATION_TYPE(golos::plugins::follow::follow_plugin_operation)
