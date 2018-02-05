#pragma once

#include <golos/protocol/operations/asset_operations.hpp>
#include <golos/protocol/operations/comment_operations.hpp>
#include <golos/protocol/operations/custom_operations.hpp>
#include <golos/protocol/operations/escrow_operations.hpp>
#include <golos/protocol/operations/market_operations.hpp>
#include <golos/protocol/operations/market_virtual_operations.hpp>
#include <golos/protocol/operations/steem_operations.hpp>
#include <golos/protocol/operations/steem_virtual_operations.hpp>
#include <golos/protocol/operations/asset_virtual_operations.hpp>
#include <golos/protocol/operations/witness_virtual_operations.hpp>
#include <golos/protocol/operations/account_operations.hpp>
#include <golos/protocol/operations/transfer_operations.hpp>
#include <golos/protocol/operations/proposal_operations.hpp>
#include <golos/protocol/operations/witness_operations.hpp>

namespace golos {
    namespace protocol {

        /** NOTE: do not change the order of any operations prior to the virtual operations
         * or it will trigger a hardfork.
         */
        typedef fc::static_variant<
                vote_operation<0, 16, 0>,
                comment_operation<0, 16, 0>,

                transfer_operation<0, 16, 0>,
                transfer_to_vesting_operation<0, 16, 0>,
                withdraw_vesting_operation<0, 16, 0>,

                limit_order_create_operation<0, 16, 0>,
                limit_order_cancel_operation<0, 16, 0>,

                feed_publish_operation<0, 16, 0>,
                convert_operation<0, 16, 0>,

                account_create_operation<0, 16, 0>,
                account_update_operation<0, 16, 0>,

                witness_update_operation<0, 16, 0>,
                account_witness_vote_operation<0, 16, 0>,
                account_witness_proxy_operation<0, 16, 0>,

                pow_operation<0, 16, 0>,

                custom_operation,

                report_over_production_operation<0, 16, 0>,

                delete_comment_operation<0, 16, 0>,
                custom_json_operation,
                comment_options_operation<0, 16, 0>,
                set_withdraw_vesting_route_operation<0, 16, 0>,
                limit_order_create2_operation<0, 16, 0>,
                challenge_authority_operation<0, 16, 0>,
                prove_authority_operation<0, 16, 0>,
                request_account_recovery_operation<0, 16, 0>,
                recover_account_operation<0, 16, 0>,
                change_recovery_account_operation<0, 16, 0>,

                escrow_transfer_operation<0, 16, 0>,
                escrow_dispute_operation<0, 16, 0>,
                escrow_release_operation<0, 16, 0>,

                pow2_operation<0, 16, 0>,
                escrow_approve_operation<0, 16, 0>,

                transfer_to_savings_operation<0, 16, 0>,
                transfer_from_savings_operation<0, 16, 0>,
                cancel_transfer_from_savings_operation<0, 16, 0>,

                custom_binary_operation,
                decline_voting_rights_operation<0, 16, 0>,
                reset_account_operation<0, 16, 0>,
                set_reset_account_operation<0, 16, 0>,
                comment_benefactor_reward_operation<0, 16, 0>,

                vote_operation<0, 17, 0>,
                comment_operation<0, 17, 0>,

                transfer_operation<0, 17, 0>,
                transfer_to_vesting_operation<0, 17, 0>,
                withdraw_vesting_operation<0, 17, 0>,

                limit_order_create_operation<0, 17, 0>,
                limit_order_cancel_operation<0, 17, 0>,

                feed_publish_operation<0, 17, 0>,
                convert_operation<0, 17, 0>,

                account_create_operation<0, 17, 0>,
                account_update_operation<0, 17, 0>,

                witness_update_operation<0, 17, 0>,
                account_witness_vote_operation<0, 17, 0>,
                account_witness_proxy_operation<0, 17, 0>,

                pow_operation<0, 17, 0>,

                report_over_production_operation<0, 17, 0>,

                delete_comment_operation<0, 17, 0>,
                comment_options_operation<0, 17, 0>,
                set_withdraw_vesting_route_operation<0, 17, 0>,
                limit_order_create2_operation<0, 17, 0>,
                challenge_authority_operation<0, 17, 0>,
                prove_authority_operation<0, 17, 0>,
                request_account_recovery_operation<0, 17, 0>,
                recover_account_operation<0, 17, 0>,
                change_recovery_account_operation<0, 17, 0>,

                escrow_transfer_operation<0, 17, 0>,
                escrow_dispute_operation<0, 17, 0>,
                escrow_release_operation<0, 17, 0>,

                pow2_operation<0, 17, 0>,
                escrow_approve_operation<0, 17, 0>,

                transfer_to_savings_operation<0, 17, 0>,
                transfer_from_savings_operation<0, 17, 0>,
                cancel_transfer_from_savings_operation<0, 17, 0>,

                decline_voting_rights_operation<0, 17, 0>,
                reset_account_operation<0, 17, 0>,
                set_reset_account_operation<0, 17, 0>,
                comment_benefactor_reward_operation<0, 17, 0>,
                delegate_vesting_shares_operation<0, 17, 0>,
                account_create_with_delegation_operation<0, 17, 0>,
                comment_payout_extension_operation<0, 17, 0>,

                asset_create_operation<0, 17, 0>,
                asset_update_operation<0, 17, 0>,
                asset_update_bitasset_operation<0, 17, 0>,
                asset_update_feed_producers_operation<0, 17, 0>,
                asset_issue_operation<0, 17, 0>,
                asset_reserve_operation<0, 17, 0>,
                asset_fund_fee_pool_operation<0, 17, 0>,
                asset_settle_operation<0, 17, 0>,
                asset_force_settle_operation<0, 17, 0>,
                asset_global_settle_operation<0, 17, 0>,
                asset_publish_feed_operation<0, 17, 0>,
                asset_claim_fees_operation<0, 17, 0>,

                call_order_update_operation<0, 17, 0>,

                account_whitelist_operation<0, 17, 0>,

                override_transfer_operation<0, 17, 0>,

                proposal_create_operation<0, 17, 0>,
                proposal_update_operation<0, 17, 0>,
                proposal_delete_operation<0, 17, 0>,

                bid_collateral_operation<0, 17, 0>,

                /// virtual operations below this point
                fill_convert_request_operation<0, 16, 0>,
                author_reward_operation<0, 16, 0>,
                curation_reward_operation<0, 16, 0>,
                comment_reward_operation<0, 16, 0>,
                liquidity_reward_operation<0, 16, 0>,
                interest_operation<0, 16, 0>,
                fill_vesting_withdraw_operation<0, 16, 0>,
                fill_order_operation<0, 16, 0>,
                shutdown_witness_operation<0, 16, 0>,
                fill_transfer_from_savings_operation<0, 16, 0>,
                hardfork_operation<0, 16, 0>,
                comment_payout_update_operation<0, 16, 0>,

                fill_convert_request_operation<0, 17, 0>,
                author_reward_operation<0, 17, 0>,
                curation_reward_operation<0, 17, 0>,
                comment_reward_operation<0, 17, 0>,
                liquidity_reward_operation<0, 17, 0>,
                interest_operation<0, 17, 0>,
                fill_vesting_withdraw_operation<0, 17, 0>,
                fill_order_operation<0, 17, 0>,
                shutdown_witness_operation<0, 17, 0>,
                fill_transfer_from_savings_operation<0, 17, 0>,
                hardfork_operation<0, 17, 0>,
                comment_payout_update_operation<0, 17, 0>,
                return_vesting_delegation_operation<0, 17, 0>,
                asset_settle_cancel_operation<0, 17, 0>,
                fill_call_order_operation<0, 17, 0>,
                fill_settlement_order_operation<0, 17, 0>,
                execute_bid_operation<0, 17, 0>,
                expire_witness_vote_operation<0, 17, 0>
                > operation;

        /*void operation_get_required_authorities( const operation& op,
                                                 flat_set<string>& active,
                                                 flat_set<string>& owner,
                                                 flat_set<string>& posting,
                                                 vector<authority>&  other );

        void operation_validate( const operation& op );*/

        bool is_market_operation(const operation &op);

        bool is_virtual_operation(const operation &op);

        struct operation_wrapper {
        public:
            operation_wrapper(const operation &op = operation()) : op(op) {
            }

            operation op;
        };
    }
} // golos::protocol

namespace fc {

    void to_variant(const golos::protocol::operation &, fc::variant &);

    void from_variant(const fc::variant &, golos::protocol::operation &);

} /* fc */

namespace golos {
    namespace protocol {

        void operation_validate(const operation &o);

        void operation_get_required_authorities(const operation &op, flat_set<account_name_type> &active,
                                                flat_set<account_name_type> &owner,
                                                flat_set<account_name_type> &posting, vector<authority> &other);

    }
}

FC_REFLECT_TYPENAME((golos::protocol::operation));
FC_REFLECT((golos::protocol::operation_wrapper), (op));
