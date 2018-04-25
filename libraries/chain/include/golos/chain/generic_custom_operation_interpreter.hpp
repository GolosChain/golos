
#pragma once

#include <golos/protocol/steem_operations.hpp>
#include <golos/protocol/operation_util_impl.hpp>

#include <golos/chain/evaluator.hpp>
#include <golos/chain/evaluator_registry.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>

#include <fc/variant.hpp>

#include <string>
#include <vector>

namespace golos {
    namespace chain {

        class database;

        template<typename OperationType>
        class evaluator_registry_non_verisobal {
        public:
            evaluator_registry_non_verisobal(database &d)
                    : _db(d) {
                for (int i = 0; i < OperationType::count(); i++) {
                    _op_evaluators.emplace_back();
                }
            }

            virtual ~evaluator_registry_non_verisobal()= default;

            template<typename EvaluatorType, typename... Args>
            void register_evaluator(Args... args) {
                _op_evaluators[OperationType::template tag<typename EvaluatorType::operation_type>::value].reset(new EvaluatorType(_db, args...));
            }

            evaluator<OperationType> &get_evaluator(const OperationType &op) {
                int i_which = op.which();
                uint64_t u_which = uint64_t(i_which);
                if (i_which < 0)
                    assert("Negative operation tag" && false);
                if (u_which >= _op_evaluators.size())
                    assert("No registered evaluator for this operation" && false);
                unique_ptr<evaluator<OperationType>> &eval = _op_evaluators[u_which];

                if (!eval)
                    assert("No registered evaluator for this operation" && false);
                return *eval;
            }

            std::vector<std::unique_ptr<evaluator<OperationType>>> _op_evaluators;
            database &_db;
        };

        template<typename CustomOperationType>
        class generic_custom_operation_interpreter
                : public custom_operation_interpreter,
                  public evaluator_registry_non_verisobal<CustomOperationType> {
        public:
            generic_custom_operation_interpreter(database &db)
                    : evaluator_registry_non_verisobal<CustomOperationType>(db) {
            }

            void apply_operations(const vector<CustomOperationType> &custom_operations, const operation &outer_o) {
                auto plugin_session = this->_db.start_undo_session();

                flat_set<account_name_type> outer_active;
                flat_set<account_name_type> outer_owner;
                flat_set<account_name_type> outer_posting;
                std::vector<authority> outer_other;

                flat_set<account_name_type> inner_active;
                flat_set<account_name_type> inner_owner;
                flat_set<account_name_type> inner_posting;
                std::vector<authority> inner_other;

                operation_get_required_authorities(outer_o, outer_active, outer_owner, outer_posting, outer_other);

                for (const CustomOperationType &inner_o : custom_operations) {
                    operation_validate(inner_o);
                    operation_get_required_authorities(inner_o, inner_active, inner_owner, inner_posting, inner_other);
                }

                FC_ASSERT(inner_owner == outer_owner);
                FC_ASSERT(inner_active == outer_active);
                FC_ASSERT(inner_posting == outer_posting);
                FC_ASSERT(inner_other == outer_other);

                for (const CustomOperationType &inner_o : custom_operations) {
                    // gcc errors if this-> is not here
                    // error message is "declarations in dependent base are not found by unqualified lookup"
                    this->get_evaluator(inner_o).apply(inner_o);
                }

                plugin_session.squash();
            }

            virtual void apply(const protocol::custom_json_operation &outer_o) override {
                try {
                    fc::variant v = fc::json::from_string(outer_o.json);

                    std::vector<CustomOperationType> custom_operations;
                    if (v.is_array() && v.size() > 0 &&
                        v.get_array()[0].is_array()) {
                        // it looks like a list
                        from_variant(v, custom_operations);
                    } else {
                        custom_operations.emplace_back();
                        from_variant(v, custom_operations[0]);
                    }

                    apply_operations(custom_operations, operation(outer_o));
                } FC_CAPTURE_AND_RETHROW((outer_o))
            }

            virtual void apply(const protocol::custom_binary_operation &outer_o) override {
                try {
                    vector<CustomOperationType> custom_operations;

                    try {
                        custom_operations = fc::raw::unpack<vector<CustomOperationType>>(outer_o.data);
                    }
                    catch (fc::exception &) {
                        custom_operations.push_back(fc::raw::unpack<CustomOperationType>(outer_o.data));
                    }

                    apply_operations(custom_operations, operation(outer_o));
                }
                FC_CAPTURE_AND_RETHROW((outer_o))
            }

        };

    }
}
