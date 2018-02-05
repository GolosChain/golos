#pragma once

#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/operations/operations.hpp>

namespace golos {
    namespace chain {

        class database;

        template<typename OperationType=golos::protocol::operation>
        class generic_evaluator {
        public:
            virtual void apply(const OperationType &op) = 0;

            virtual int get_type() const = 0;
        };

        template<typename EvaluatorType, uint8_t Major, uint8_t Hardfork, uint16_t Release,
                typename OperationType=golos::protocol::operation>
        class evaluator : public generic_evaluator<OperationType>,
                          public protocol::static_version<Major, Hardfork, Release> {
        public:
            typedef OperationType operation_sv_type;
            // typedef typename EvaluatorType::operation_type op_type;

            evaluator(database &d) : db(d) {
            }

            virtual void apply(const OperationType &o) final override {
                auto *eval = static_cast<EvaluatorType *>(this);
                const auto &op = o.template get<typename EvaluatorType::operation_type>();
                eval->do_apply(op);
            }

            virtual int get_type() const override {
                return OperationType::template tag<typename EvaluatorType::operation_type>::value;
            }

            database &get_database() {
                return db;
            }

        protected:
            database &db;
        };
    }
}