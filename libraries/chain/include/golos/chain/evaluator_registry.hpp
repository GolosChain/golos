#pragma once

#include <golos/chain/evaluator.hpp>

namespace golos {
    namespace chain {

        template<typename OperationType>
        class evaluator_registry {
        public:
            evaluator_registry(database &d)
                    : _db(d) {
                for (int i = 0; i < OperationType::count(); i++) {
                    _op_evaluators.emplace_back();
                }
            }

            ~evaluator_registry() = default;


            template<typename EvaluatorType, typename... Args>
            void register_evaluator(Args... args) {
                auto it = OperationType::template tag<typename EvaluatorType::operation_type>::value;
                auto& proxy = _op_evaluators[it];
                if(proxy.get() == nullptr) {
                    _op_evaluators[it].reset(new evaluator_proxy<OperationType>(_db, args...));
                }
                _op_evaluators[it]->template add_evaluator<EvaluatorType>();
            }

            evaluator<OperationType> &get_evaluator(const OperationType &op,uint32_t current_hardfork_version) {
                int i_which = op.which();
                uint64_t u_which = uint64_t(i_which);
                if (i_which < 0)
                    assert("Negative operation tag" && false);
                if (u_which >= _op_evaluators.size())
                    assert("No registered evaluator for this operation" && false);
                std::unique_ptr<evaluator_proxy<OperationType>> &eval = _op_evaluators[u_which];

                if (!eval)
                    assert("No registered evaluator for this operation" && false);
                return eval->get_evaluator(current_hardfork_version);
            }

            std::vector<std::unique_ptr<evaluator_proxy<OperationType>>> _op_evaluators;
            database &_db;
        };

    }
}
