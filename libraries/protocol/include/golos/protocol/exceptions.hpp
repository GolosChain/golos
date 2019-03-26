#pragma once

#include <fc/exception/exception.hpp>
#include <golos/protocol/protocol.hpp>

#define GOLOS_ASSERT_MESSAGE(FORMAT, ...) \
    FC_LOG_MESSAGE(error, FORMAT, __VA_ARGS__)

#define GOLOS_CTOR_ASSERT(expr, exception_type, exception_ctor) \
    if (!(expr)) { \
        exception_type _E; \
        exception_ctor(_E); \
        throw _E; \
    }

#define GOLOS_ASSERT(expr, exception_type, FORMAT, ...) \
    FC_MULTILINE_MACRO_BEGIN \
        if (!(expr)) { \
            throw exception_type(GOLOS_ASSERT_MESSAGE(FORMAT, __VA_ARGS__)); \
        } \
    FC_MULTILINE_MACRO_END

#define GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(TYPE, BASE, CODE, WHAT) \
    public: \
        enum code_enum { \
            code_value = CODE, \
        }; \
        TYPE(fc::log_message&& m) \
            : BASE(fc::move(m), CODE, BOOST_PP_STRINGIZE(TYPE), WHAT) {} \
        TYPE() \
            : BASE(CODE, BOOST_PP_STRINGIZE(TYPE), WHAT) {} \
        virtual std::shared_ptr<fc::exception> dynamic_copy_exception() const { \
            return std::make_shared<TYPE>(*this); \
        } \
        virtual NO_RETURN void dynamic_rethrow_exception() const { \
            if (code() == CODE) { \
                throw *this; \
            } else { \
                fc::exception::dynamic_rethrow_exception(); \
            } \
        } \
    protected: \
        TYPE(const BASE& c) \
            : BASE(c) {} \
        explicit TYPE(int64_t code, const std::string& name_value, const std::string& what_value) \
            : BASE(code, name_value, what_value) {} \
        explicit TYPE(fc::log_message&& m, int64_t code, const std::string& name_value, const std::string& what_value) \
            : BASE(std::move(m), code, name_value, what_value) {}

#define GOLOS_DECLARE_DERIVED_EXCEPTION(TYPE, BASE, CODE, WHAT) \
    class TYPE: public BASE { \
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(TYPE, BASE, CODE, WHAT) \
    };

#define GOLOS_CHECK_LOGIC(expr, TYPE, MSG, ...) \
        GOLOS_ASSERT(expr, golos::logic_exception, MSG, ("errid", TYPE)("namespace",golos::get_logic_error_namespace<decltype(TYPE)>())__VA_ARGS__)

#define GOLOS_CHECK_DATABASE(expr, TYPE, MSG, ...) \
        GOLOS_ASSERT(expr, golos::database_corrupted, MSG, ("errid", TYPE)__VA_ARGS__)


// TODO Remove after done refactor errors in plugins #791
//      This macros is obsolete and replaced with PLUGIN_API_VALIDATE_ARGS
#define GOLOS_DECLARE_PARAM(PARAM, GETTER) auto (PARAM) = [&]() {\
    try {return (GETTER);} \
    catch (const fc::exception &e) { \
        FC_THROW_EXCEPTION(golos::invalid_parameter, \
            "Invalid parameter \"${param}\": ${errmsg}", \
            ("param", BOOST_PP_STRINGIZE(PARAM)) \
            ("errmsg", e.to_string()) \
            ("error", e) \
            ); \
    } \
}()

#define GOLOS_CONVERT_PARAM(PARAM, VALUE, TYPE) \
    [&](){ \
        try { \
            return (VALUE).as<TYPE>(); \
        } catch (const fc::exception& e) { \
            FC_THROW_EXCEPTION(invalid_parameter, "Invalid value \"${value}\" for parameter \"${param}\": ${errmsg}", \
                    ("param", FC_STRINGIZE(PARAM)) \
                    ("value", VALUE) \
                    ("errmsg", e.to_string()) \
                    ("error", e)); \
        } \
        return TYPE(); /* make compiler happy */\
    }()

#define GOLOS_CHECK_PARAM(PARAM, VALIDATOR) GOLOS_CHECK_PARAM_I(PARAM, PARAM, VALIDATOR, "")
#define GOLOS_CHECK_OP_PARAM(OP, PARAM, VALIDATOR) GOLOS_CHECK_PARAM_I(PARAM, OP.PARAM, VALIDATOR, "operation ")

#define GOLOS_CHECK_PARAM_I(PARAM, VALUE, VALIDATOR, TYPE) \
    FC_MULTILINE_MACRO_BEGIN \
        try { \
            VALIDATOR; \
        } catch (const golos::invalid_value& e) { \
            FC_THROW_EXCEPTION(invalid_parameter, "Invalid value \"${value}\" for " TYPE "parameter \"${param}\": ${errmsg}", \
                    ("param", FC_STRINGIZE(PARAM)) \
                    ("value", VALUE) \
                    ("errmsg", e.to_string()) \
                    ("error", static_cast<fc::exception>(e))); \
        } \
    FC_MULTILINE_MACRO_END

#define GOLOS_CHECK_OPTION(COND, MSG, ...) \
    GOLOS_ASSERT((COND), golos::invalid_option, MSG, __VA_ARGS__)

#define GOLOS_CHECK_VALUE(COND, MSG, ...) \
    GOLOS_ASSERT((COND), golos::invalid_value, MSG, __VA_ARGS__)

#define GOLOS_CHECK_LIMIT(limit, max_value) \
    GOLOS_ASSERT( limit <= max_value, golos::limit_too_large, "Exceeded limit value. Maximum allowed ${max}", \
            ("limit",limit)("max",max_value))

#define GOLOS_CHECK_LIMIT_PARAM(limit, max_value) \
    GOLOS_CHECK_PARAM(limit, GOLOS_CHECK_LIMIT(limit, max_value))


#define GOLOS_THROW_MISSING_OBJECT(type, id, ...) \
    FC_THROW_EXCEPTION(golos::missing_object, "Missing ${type} with id \"${id}\"", \
            ("type",type)("id",id) __VA_ARGS__)

#define GOLOS_THROW_OBJECT_ALREADY_EXIST(type, id, ...) \
    FC_THROW_EXCEPTION(golos::object_already_exist, "Object ${type} with id \"${id}\" already exists", \
            ("type",type)("id",id) __VA_ARGS__)

#define GOLOS_CHECK_OBJECT_MISSING(DB, OBJ, ...) \
    DB.throw_if_exists_##OBJ(__VA_ARGS__)

#define GOLOS_THROW_INTERNAL_ERROR(MSG, ...) \
    FC_THROW_EXCEPTION(golos::internal_error, MSG, __VA_ARGS__)

namespace golos {


    // Function to get logic_error codes namespace
    template<typename T>
    std::string get_logic_error_namespace();

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        golos_exception, fc::exception,
              0, "golos base exception")

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        operation_exception, golos_exception,
        1000000, "Opertaion exception");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        unsupported_operation, operation_exception,
        1010000, "Unsupported operation");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        unsupported_api_method, unsupported_operation,
        1010100, "Unsupported api method");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        parameter_exception, operation_exception,
        1020000, "Parameter exception");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        invalid_arguments_count, parameter_exception,
        1020100, "Invalid argument count");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        missing_object, parameter_exception,
        1020200, "Missing object");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        object_already_exist, parameter_exception,
        1020300, "Object already exist");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        invalid_parameter, parameter_exception,
        1020400, "Invalid parameter value");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        business_exception, golos_exception,
        2000000, "Business logic error");

    class bandwidth_exception : public business_exception {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            bandwidth_exception, business_exception,
            2010000, "bandwidth exceeded error");
    public:
        enum bandwidth_types {
            post_bandwidth,
            comment_bandwidth,
            vote_bandwidth,
            change_owner_authority_bandwidth,
            custom_json_bandwidth,
        };
    };

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        insufficient_funds, business_exception,
        2020000, "Account does not have sufficient funds")

    class logic_exception : public business_exception {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            logic_exception, business_exception,
            2030000, "business logic error");
    public:
        enum error_types {
            reached_limit_for_pending_withdraw_requests    = 1,
            parent_of_comment_cannot_change,
            parent_perlink_of_comment_cannot_change,

            // Vote operation
            voter_declined_voting_rights,
            account_is_currently_challenged,
            votes_are_not_allowed,
            does_not_have_voting_power,
            voting_weight_is_too_small,
            cannot_vote_within_last_minute_before_payout,
            cannot_vote_with_zero_rshares,
            voter_has_used_maximum_vote_changes,
            already_voted_in_similar_way,

            // Comment operation
            cannot_update_comment_because_nothing_changed,
            reached_comment_max_depth,
            replies_are_not_allowed,
            cannot_delete_comment_with_replies,
            cannot_delete_comment_with_positive_votes,
            cannot_delete_post_with_worker_proposal,
            cannot_delete_post_with_worker_techspec,
            cannot_delete_post_with_worker_result,
            comment_options_requires_no_rshares,
            curation_rewards_cannot_be_reenabled,
            voting_cannot_be_reenabled,
            comment_cannot_accept_greater_payout,
            comment_cannot_accept_greater_percent_GBG,
            cannot_specify_more_beneficiaries,
            comment_already_has_beneficiaries,
            beneficiaries_should_be_unique,
            comment_must_not_have_been_voted,

            // withdraw_vesting
            insufficient_fee_for_powerdown_registered_account,
            operation_would_not_change_vesting_withdraw_rate,
            cannot_create_zero_percent_destination,
            reached_maxumum_number_of_routes,
            more_100percent_allocated_to_destinations,

            // account_create_with_delegation
            not_enough_delegation,

            // challenge_authority_operation
            cannot_challenge_yourself,
            // prove_authority_evaluator
            account_is_not_challeneged,

            // escrow
            escrow_no_amount_set,
            escrow_wrong_time_limits,
            escrow_time_in_past,
            escrow_bad_to,
            escrow_bad_agent,
            escrow_bad_receiver,
            ratification_deadline_passed,
            account_already_approved_escrow,
            cannot_dispute_expired_escrow,
            escrow_must_be_approved_first,
            escrow_already_disputed,
            release_amount_exceeds_escrow_balance,
            only_agent_can_release_disputed,
            only_from_to_can_release_non_disputed,
            from_can_release_only_to_to,
            to_can_release_only_to_from,

            // request_account_recovery
            cannot_recover_if_not_partner,
            must_be_recovered_by_top_witness,
            // recover_account_operation
            cannot_set_recent_recovery,
            no_active_recovery_request,
            authority_does_not_match_request,
            no_recent_authority_in_history,

            // set_reset_account_operation
            cannot_set_same_reset_account,

            // break_free_referral_operation
            no_right_to_break_referral,

            // delegate vesting shares operations
            cannot_delegate_to_yourself,
            delegation_difference_too_low,
            delegation_limited_by_voting_power,
            cannot_delegate_below_minimum,
            cannot_change_delegator_interest_rate,
            cannot_change_delegator_payout_strategy,

            // proposals and transactions
            proposal_depth_too_high,
            tx_with_both_posting_active_ops,

            empty_approvals,
            add_and_remove_same_approval,
            cannot_add_approval_in_review_period,
            non_existing_approval,
            already_existing_approval,

            proposal_delete_not_allowed,

            // limit order
            limit_order_must_be_for_golos_gbg_market,
            cancelling_not_filled_order,

            // feed_publish_operation
            price_feed_must_be_for_golos_gbg_market,

            // account_witness_vote_operation
            cannot_vote_when_route_are_set,
            witness_vote_does_not_exist,
            witness_vote_already_exist,
            account_has_too_many_witness_votes,

            // account_witness_proxy_operation
            proxy_must_change,
            proxy_would_create_loop,
            proxy_chain_is_too_long,

            // convert operation
            no_price_feed_yet,

            // pow operation
            duplicate_work_discovered,
            miners_can_only_have_one_key_authority,
            work_must_be_performed_by_signed_key,
            work_not_for_last_block,
            work_for_block_older_last_irreversible_block,
            account_must_not_be_updated_in_this_block,
            insufficient_work_difficalty,
            account_already_scheduled_for_work,
            cannot_specify_owner_key_unless_creating_account,
            witness_must_be_created_before_minning,

            // workers
            worker_proposal_can_be_created_only_on_post,
            post_should_be_in_cashout_window,
            cannot_edit_worker_proposal_with_approved_techspec,
            cannot_delete_worker_proposal_with_approved_techspec,
            cannot_delete_worker_proposal_with_techspecs,
            worker_techspec_can_be_created_only_on_post,
            worker_techspec_can_be_created_only_for_existing_proposal,
            this_worker_proposal_already_has_approved_techspec,
            this_worker_techspec_is_already_used_for_another_worker_proposal,
            cannot_delete_paying_worker_techspec,
            approver_of_techspec_should_be_in_top19_of_witnesses,
            techspec_is_already_approved_or_closed,
            worker_result_can_be_created_only_on_post,
            worker_result_can_be_created_only_for_techspec_in_work,
            cannot_delete_worker_result_for_paying_techspec,
            this_post_already_used_as_worker_result,
            approver_of_result_should_be_in_top19_of_witnesses,
            worker_techspec_should_be_complete_or_paying_to_approve_result,
            worker_techspec_already_has_final_result,
            worker_can_be_assigned_only_to_proposal_with_approved_techspec,
            worker_cannot_be_assigned_to_premade_proposal,
            cannot_unassign_worker_from_finished_or_not_started_work,
            worker_can_be_unassigned_only_by_techspec_author_or_himself,
            insufficient_funds_to_approve_worker_techspec,
            approve_term_has_expired,
            approver_of_payment_should_be_in_top19_of_witnesses,
            worker_techspec_should_be_in_payment_state,
            you_already_have_voted_for_this_object_with_this_state,
            cannot_create_techspec_for_premade_worker_proposal,
            only_premade_worker_result_can_be_created_for_premade_worker_proposal,
            this_post_already_used_as_worker_techspec,
            premade_result_can_be_created_only_for_premade_work_proposal,
            this_worker_proposal_already_has_approved_result,

            // custom operations
            inner_authorities_does_not_match_outer,

            // database logic
            account_exceeded_bandwidth_per_vestring_share,
        };
    };

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        internal_error, golos_exception,
        4000000, "internal error");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        assert_exception, internal_error,
        4010000, "assert exception");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        invalid_value, internal_error,
        4020000, "invalid value exception");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        limit_too_large, invalid_value,
        4020100, "Exceeded limit value");

    class database_corrupted : public internal_error {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            database_corrupted, internal_error,
            4030000, "Database corrupted");
    public:
        enum error_types {
            wrong_block_num_was_read,
            wrong_position_marker_was_read,
            append_index_file_at_wrong_position,
            reading_data_beyond_end_of_file,
        };
    };

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        invalid_option, golos_exception,
        5000000, "invalid option");

} // golos


namespace golos { namespace protocol {
    GOLOS_DECLARE_DERIVED_EXCEPTION(
        transaction_exception, golos_exception,
        3000000, "transaction exception")

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        tx_invalid_operation, transaction_exception,
        3010000, "invalid operation in transaction");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        tx_missing_authority, transaction_exception,
        3020000, "missing authority");

    class tx_missing_active_auth: public tx_missing_authority {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            tx_missing_active_auth, tx_missing_authority,
            3020100, "missing required active authority");
    public:
        std::vector<account_name_type> missing_accounts;
        std::vector<public_key_type> used_signatures;
    };

    class tx_missing_owner_auth: public tx_missing_authority {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            tx_missing_owner_auth, tx_missing_authority,
            3020200, "missing required owner authority");
    public:
        std::vector<account_name_type> missing_accounts;
        std::vector<public_key_type> used_signatures;
    };

    class tx_missing_posting_auth: public tx_missing_authority {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            tx_missing_posting_auth, tx_missing_authority,
            3020300, "missing required posting authority");
    public:
        std::vector<account_name_type> missing_accounts;
        std::vector<public_key_type> used_signatures;
    };

    class tx_missing_other_auth: public tx_missing_authority {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            tx_missing_other_auth, tx_missing_authority,
            3020400, "missing required other authority");
    public:
        std::vector<authority> missing_auths;
    };

    class tx_irrelevant_sig: public transaction_exception {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            tx_irrelevant_sig, transaction_exception,
            3030000, "irrelevant signature included");
    public:
        std::vector<public_key_type> unused_signatures;
    };

    class tx_duplicate_sig: public transaction_exception {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            tx_duplicate_sig, transaction_exception,
            3060000, "duplicate signature included");
    };

    class tx_irrelevant_approval: public transaction_exception {
        GOLOS_DECLARE_DERIVED_EXCEPTION_BODY(
            tx_irrelevant_approval, transaction_exception,
            3070000, "irrelevant approval included");
    public:
        std::vector<account_name_type> unused_approvals;
    };

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        tx_duplicate_transaction, transaction_exception,
        3080000, "duplicate transaction");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        tx_too_long, transaction_exception,
        3090000, "transaction too long");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        tx_expired, transaction_exception,
        3100000, "expired transaction");

    GOLOS_DECLARE_DERIVED_EXCEPTION(
        tx_invalid_field, transaction_exception,
        3110000, "invalid transaction field");


} } // golos::protocol

FC_REFLECT_ENUM(golos::logic_exception::error_types,
        (reached_limit_for_pending_withdraw_requests)
        (parent_of_comment_cannot_change)
        (parent_perlink_of_comment_cannot_change)

        // Vote operation
        (voter_declined_voting_rights)
        (account_is_currently_challenged)
        (votes_are_not_allowed)
        (does_not_have_voting_power)
        (voting_weight_is_too_small)
        (cannot_vote_within_last_minute_before_payout)
        (cannot_vote_with_zero_rshares)
        (voter_has_used_maximum_vote_changes)
        (already_voted_in_similar_way)

        // Comment operation
        (cannot_update_comment_because_nothing_changed)
        (reached_comment_max_depth)
        (replies_are_not_allowed)
        (cannot_delete_comment_with_replies)
        (cannot_delete_comment_with_positive_votes)
        (cannot_delete_post_with_worker_proposal)
        (cannot_delete_post_with_worker_techspec)
        (cannot_delete_post_with_worker_result)
        (comment_options_requires_no_rshares)
        (curation_rewards_cannot_be_reenabled)
        (voting_cannot_be_reenabled)
        (comment_cannot_accept_greater_payout)
        (comment_cannot_accept_greater_percent_GBG)
        (cannot_specify_more_beneficiaries)
        (comment_already_has_beneficiaries)
        (beneficiaries_should_be_unique)
        (comment_must_not_have_been_voted)

        // withdraw_vesting
        (insufficient_fee_for_powerdown_registered_account)
        (operation_would_not_change_vesting_withdraw_rate)
        (cannot_create_zero_percent_destination)
        (reached_maxumum_number_of_routes)
        (more_100percent_allocated_to_destinations)

        // challenge_authority_operation
        (cannot_challenge_yourself)
        //prove_authority_evaluator
        (account_is_not_challeneged)

        // escrow
        (escrow_no_amount_set)
        (escrow_wrong_time_limits)
        (escrow_time_in_past)
        (escrow_bad_to)
        (escrow_bad_agent)
        (escrow_bad_receiver)
        (ratification_deadline_passed)
        (account_already_approved_escrow)
        (cannot_dispute_expired_escrow)
        (escrow_must_be_approved_first)
        (escrow_already_disputed)
        (release_amount_exceeds_escrow_balance)
        (only_agent_can_release_disputed)
        (only_from_to_can_release_non_disputed)
        (from_can_release_only_to_to)
        (to_can_release_only_to_from)

        // request_account_recovery
        (cannot_recover_if_not_partner)
        (must_be_recovered_by_top_witness)
        // recover_account_operation
        (cannot_set_recent_recovery)
        (no_active_recovery_request)
        (authority_does_not_match_request)
        (no_recent_authority_in_history)

        // set_reset_account_operation
        (cannot_set_same_reset_account)

        // break_free_referral_operation
        (no_right_to_break_referral)

        // delegate_vesting_shares
        (cannot_delegate_to_yourself)
        (delegation_difference_too_low)
        (delegation_limited_by_voting_power)
        (cannot_delegate_below_minimum)
        (cannot_change_delegator_interest_rate)
        (cannot_change_delegator_payout_strategy)

        // account_create_with_delegation
        (not_enough_delegation)

        // proposals
        (proposal_depth_too_high)
        (tx_with_both_posting_active_ops)

        (empty_approvals)
        (add_and_remove_same_approval)
        (cannot_add_approval_in_review_period)
        (non_existing_approval)
        (already_existing_approval)

        (proposal_delete_not_allowed)

        // limit order
        (limit_order_must_be_for_golos_gbg_market)
        (cancelling_not_filled_order)

        // feed_publis_operation
        (price_feed_must_be_for_golos_gbg_market)

        // account_witness_vote_operation
        (cannot_vote_when_route_are_set)
        (witness_vote_does_not_exist)
        (witness_vote_already_exist)
        (account_has_too_many_witness_votes)

        // account_witness_proxy_operation
        (proxy_must_change)
        (proxy_would_create_loop)
        (proxy_chain_is_too_long)

        // convert operation
        (no_price_feed_yet)

        // pow operation
        (duplicate_work_discovered)
        (miners_can_only_have_one_key_authority)
        (work_must_be_performed_by_signed_key)
        (work_not_for_last_block)
        (work_for_block_older_last_irreversible_block)
        (account_must_not_be_updated_in_this_block)
        (insufficient_work_difficalty)
        (account_already_scheduled_for_work)
        (cannot_specify_owner_key_unless_creating_account)
        (witness_must_be_created_before_minning)

        // workers
        (worker_proposal_can_be_created_only_on_post)
        (post_should_be_in_cashout_window)
        (cannot_edit_worker_proposal_with_approved_techspec)
        (cannot_delete_worker_proposal_with_approved_techspec)
        (cannot_delete_worker_proposal_with_techspecs)
        (worker_techspec_can_be_created_only_on_post)
        (worker_techspec_can_be_created_only_for_existing_proposal)
        (this_worker_proposal_already_has_approved_techspec)
        (this_worker_techspec_is_already_used_for_another_worker_proposal)
        (cannot_delete_paying_worker_techspec)
        (approver_of_techspec_should_be_in_top19_of_witnesses)
        (techspec_is_already_approved_or_closed)
        (worker_result_can_be_created_only_on_post)
        (worker_result_can_be_created_only_for_techspec_in_work)
        (cannot_delete_worker_result_for_paying_techspec)
        (this_post_already_used_as_worker_result)
        (approver_of_result_should_be_in_top19_of_witnesses)
        (worker_techspec_should_be_complete_or_paying_to_approve_result)
        (worker_techspec_already_has_final_result)
        (worker_can_be_assigned_only_to_proposal_with_approved_techspec)
        (worker_cannot_be_assigned_to_premade_proposal)
        (cannot_unassign_worker_from_finished_or_not_started_work)
        (worker_can_be_unassigned_only_by_techspec_author_or_himself)
        (insufficient_funds_to_approve_worker_techspec)
        (approve_term_has_expired)
        (approver_of_payment_should_be_in_top19_of_witnesses)
        (worker_techspec_should_be_in_payment_state)
        (you_already_have_voted_for_this_object_with_this_state)
        (cannot_create_techspec_for_premade_worker_proposal)
        (only_premade_worker_result_can_be_created_for_premade_worker_proposal)
        (this_post_already_used_as_worker_techspec)
        (premade_result_can_be_created_only_for_premade_work_proposal)
        (this_worker_proposal_already_has_approved_result)

        // custom operations
        (inner_authorities_does_not_match_outer)

        // database logic
        (account_exceeded_bandwidth_per_vestring_share)
);

FC_REFLECT_ENUM(golos::bandwidth_exception::bandwidth_types,
        (post_bandwidth)
        (comment_bandwidth)
        (vote_bandwidth)
        (change_owner_authority_bandwidth)
        (custom_json_bandwidth)
);

FC_REFLECT_ENUM(golos::database_corrupted::error_types,
        (wrong_block_num_was_read)
        (wrong_position_marker_was_read)
        (append_index_file_at_wrong_position)
        (reading_data_beyond_end_of_file)
);
