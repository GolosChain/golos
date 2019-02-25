#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>

namespace golos { namespace protocol {

    void worker_proposal_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));

        GOLOS_CHECK_PARAM(type, {
            GOLOS_CHECK_VALUE(type < worker_proposal_type::_size, "This value is reserved");
        });
    }

    void worker_proposal_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
    }

    template<typename Operation>
    void worker_techspec_validate(const Operation& op) {
        GOLOS_CHECK_PARAM_ACCOUNT(op.author);
        GOLOS_CHECK_PARAM(op.permlink, validate_permlink(op.permlink));
        GOLOS_CHECK_PARAM_ACCOUNT(op.worker_proposal_author);
        GOLOS_CHECK_PARAM(op.worker_proposal_permlink, validate_permlink(op.worker_proposal_permlink));

        GOLOS_CHECK_PARAM(op.specification_cost, {
            GOLOS_CHECK_ASSET_GOLOS(op.specification_cost);
            GOLOS_CHECK_VALUE_GE(op.specification_cost.amount, 0);
        });
        GOLOS_CHECK_PARAM(op.development_cost, {
            GOLOS_CHECK_ASSET_GOLOS(op.development_cost);
            GOLOS_CHECK_VALUE_GE(op.development_cost.amount, 0);
        });

        GOLOS_CHECK_PARAM(op.payments_count, {
            GOLOS_CHECK_VALUE_GE(op.payments_count, 1);
        });
        GOLOS_CHECK_PARAM(op.payments_interval, {
            GOLOS_CHECK_VALUE_GE(op.payments_interval, 1);

            if (op.payments_count == 1) {
                GOLOS_CHECK_VALUE_EQ(op.payments_interval, 1);
            }
        });
    }

    void worker_techspec_operation::validate() const {
        worker_techspec_validate(*this);
    }

    void worker_techspec_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
    }

    void worker_techspec_approve_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(approver);
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));

        GOLOS_CHECK_PARAM(state, {
            GOLOS_CHECK_VALUE(state < worker_techspec_approve_state::_size, "This value is reserved");
        });
    }

    void worker_assign_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(assigner);
        GOLOS_CHECK_PARAM_ACCOUNT(worker_techspec_author);
        GOLOS_CHECK_PARAM(worker_techspec_permlink, validate_permlink(worker_techspec_permlink));
        if (worker.size()) {
            GOLOS_CHECK_PARAM_ACCOUNT(worker);
            GOLOS_CHECK_PARAM(assigner, {
                GOLOS_CHECK_VALUE(assigner == worker_techspec_author, "Worker can be assigned only by techspec author");
            });
        }
    }

    void worker_result_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
        GOLOS_CHECK_PARAM(worker_techspec_permlink, validate_permlink(worker_techspec_permlink));
    }

    void worker_result_premade_operation::validate() const {
        worker_techspec_validate(*this);
    }

    void worker_result_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
    }

    void worker_result_approve_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(approver);
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));

        GOLOS_CHECK_PARAM(state, {
            GOLOS_CHECK_VALUE(state < worker_techspec_approve_state::_size, "This value is reserved");
        });
    }

    void worker_payment_approve_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(approver);
        GOLOS_CHECK_PARAM_ACCOUNT(worker_result_author);
        GOLOS_CHECK_PARAM(worker_result_permlink, validate_permlink(worker_result_permlink));

        GOLOS_CHECK_PARAM(state, {
            GOLOS_CHECK_VALUE(state < worker_techspec_approve_state::_size, "This value is reserved");
        });
    }

} } // golos::protocol
