#include <golos/plugins/private_message/private_message_plugin.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/protocol/operation_util_impl.hpp>

#include <golos/protocol/exceptions.hpp>

namespace golos { namespace plugins { namespace private_message {

    void private_message_operation::validate() const {
        GOLOS_CHECK_LOGIC(from != to, logic_errors::cannot_send_to_yourself,
            "You cannot write to yourself");
    }
} } } // golos::plugins::private_message

DEFINE_OPERATION_TYPE(golos::plugins::private_message::private_message_plugin_operation);
