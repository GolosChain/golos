#ifndef GOLOS_CUSTOM_EVALUATOR_HPP
#define GOLOS_CUSTOM_EVALUATOR_HPP

#include <golos/chain/evaluator.hpp>

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

namespace golos {
    namespace chain {
        class custom_evaluator : public evaluator<custom_evaluator, 0, 17, 0> {
        public:
            typedef protocol::custom_operation operation_type;

            custom_evaluator(database &db) : evaluator<custom_evaluator, 0, 17, 0>(db) {
            }

            void do_apply(const protocol::custom_operation &o);
        };

        class custom_json_evaluator : public evaluator<custom_json_evaluator, 0, 17, 0> {
        public:
            typedef protocol::custom_json_operation operation_type;

            custom_json_evaluator(database &db) : evaluator<custom_json_evaluator, 0, 17, 0>(db) {
            }

            void do_apply(const protocol::custom_json_operation &o);
        };

        class custom_binary_evaluator : public evaluator<custom_binary_evaluator, 0, 17, 0> {
        public:
            typedef protocol::custom_binary_operation operation_type;

            custom_binary_evaluator(database &db) : evaluator<custom_binary_evaluator, 0, 17, 0>(db) {
            }

            void do_apply(const protocol::custom_binary_operation &o);
        };
    }
}

#endif //GOLOS_CUSTOM_EVALUATOR_HPP
