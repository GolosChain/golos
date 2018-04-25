#pragma once

#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/operations.hpp>
#include <unordered_map>

namespace golos {
    namespace chain {

        class database;

        template<typename OperationType=golos::protocol::operation>
        class evaluator {
        public:
            virtual void apply(const OperationType &op) = 0;

            virtual int get_type() const = 0;

            virtual ~evaluator() = default;
        };

        template<typename EvaluatorType, typename OperationType=golos::protocol::operation>
        class evaluator_impl : public evaluator<OperationType> {
        public:
            typedef OperationType operation_sv_type;
            // typedef typename EvaluatorType::operation_type op_type;

            evaluator_impl(database &d)
                    : _db(d) {
            }

            virtual ~evaluator_impl() = default;

            virtual uint32_t get_version() const {
                return 17; /// default;
            }

            virtual void apply(const OperationType &o) override {
                auto *eval = static_cast< EvaluatorType * >(this);
                const auto &op = o.template get<typename EvaluatorType::operation_type>();
                eval->do_apply(op);
            }

            virtual int get_type() const override {
                return OperationType::template tag<typename EvaluatorType::operation_type>::value;
            }

            database &db() {
                return _db;
            }

        protected:
            database &_db;
        };


        template<typename OperationType>
        class evaluator_proxy {
        public:
            evaluator_proxy(database &db) : db_(db) {

            }

            ~evaluator_proxy() = default;

            template<typename EvaluatorType>
            void add_evaluator() {
                auto *evaluator = new EvaluatorType(db_);
                type = evaluator->get_type();
                versions.emplace(evaluator->get_version(), evaluator);
            }

            int get_type() const {
                return type;
            }

            evaluator<OperationType> &get_evaluator(uint32_t current_hardfork_version) {
                auto it = versions.find(current_hardfork_version);
                if (it != versions.end()) {
                    return *(it->second);
                } else {
                    throw std::exception();
                }
            }

        private:
            int type;
            database &db_;
            std::unordered_map<uint32_t, std::unique_ptr<evaluator<OperationType>>> versions;
        };


    }
}

#define DEFINE_EVALUATOR(X) \
class X ## _evaluator : public golos::chain::evaluator_impl< X ## _evaluator > \
{                                                                           \
   public:                                                                  \
      typedef X ## _operation operation_type;                               \
                                                                            \
      X ## _evaluator( database& db )                                       \
         : golos::chain::evaluator_impl< X ## _evaluator >( db )          \
      {}                                                                    \
                                                                            \
      void do_apply( const X ## _operation& o );                            \
};
