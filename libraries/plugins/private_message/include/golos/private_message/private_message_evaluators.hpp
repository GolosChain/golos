#pragma once

#include <golos/chain/evaluator.hpp>

#include <golos/private_message/private_message_operations.hpp>
#include <golos/private_message/private_message_plugin.hpp>

namespace golos {
    namespace private_message {
        class private_message_evaluator : public chain::evaluator<private_message_evaluator, 0, 17, 0,
                private_message_plugin_operation> {
        public:
            typedef private_message_operation operation_type;

            private_message_evaluator(chain::database &db, private_message_plugin *plugin) : chain::evaluator<
                    private_message_evaluator, 0, 17, 0, private_message_plugin_operation>(db),
                    _plugin(plugin) {
            }

            void do_apply(const private_message_operation &o);

            private_message_plugin *_plugin;
        };
    }
}
