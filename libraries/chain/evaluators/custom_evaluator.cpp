#include <golos/chain/evaluators/custom_evaluator.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>

#include <golos/chain/database.hpp>

namespace golos {
    namespace chain {
        void custom_evaluator::do_apply(const operation_type &o) {
        }

        void custom_json_evaluator::do_apply(const operation_type &o) {
            database &d = get_database();
            std::shared_ptr <custom_operation_interpreter> eval = d.get_custom_json_evaluator(o.id);
            if (!eval) {
                return;
            }

            try {
                eval->apply(o);
            } catch (const fc::exception &e) {
                if (d.is_producing()) {
                    throw e;
                }
            } catch (...) {
                elog("Unexpected exception applying custom json evaluator.");
            }
        }


        void custom_binary_evaluator::do_apply(const operation_type &o) {
            database &d = get_database();
            FC_ASSERT(d.has_hardfork(STEEMIT_HARDFORK_0_14__317));

            std::shared_ptr <custom_operation_interpreter> eval = d.get_custom_json_evaluator(o.id);
            if (!eval) {
                return;
            }

            try {
                eval->apply(o);
            } catch (const fc::exception &e) {
                if (d.is_producing()) {
                    throw e;
                }
            } catch (...) {
                elog("Unexpected exception applying custom json evaluator.");
            }
        }
    }
}
