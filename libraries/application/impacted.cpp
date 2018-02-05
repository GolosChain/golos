#include <golos/protocol/authority.hpp>

#include <golos/application/impacted.hpp>

namespace golos {
    namespace application {

        using namespace fc;
        using namespace golos::protocol;

        // TODO:  Review all of these, especially no-ops
        struct get_impacted_account_visitor {
            flat_set<account_name_type> &_impacted;

            get_impacted_account_visitor(flat_set<account_name_type> &impact) : _impacted(impact) {
            }

            typedef void result_type;

            template<typename T>
            void operator()(const T &op) {
                op.get_required_posting_authorities(_impacted);
                op.get_required_active_authorities(_impacted);
                op.get_required_owner_authorities(_impacted);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const account_create_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.new_account_name);
                _impacted.insert(op.creator);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const account_update_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const comment_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.author);
                if (op.parent_author.size()) {
                    _impacted.insert(op.parent_author);
                }
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const delete_comment_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.author);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const vote_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.voter);
                _impacted.insert(op.author);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const author_reward_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.author);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const curation_reward_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.curator);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const liquidity_reward_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const interest_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const fill_convert_request_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const transfer_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const transfer_to_vesting_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);

                if (op.to != account_name_type() && op.to != op.from) {
                    _impacted.insert(op.to);
                }
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const withdraw_vesting_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const witness_update_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const account_witness_vote_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account);
                _impacted.insert(op.witness);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const account_witness_proxy_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account);
                _impacted.insert(op.proxy);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const feed_publish_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.publisher);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const limit_order_create_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const fill_order_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.current_owner);
                _impacted.insert(op.open_owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const fill_call_order_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const fill_settlement_order_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const limit_order_cancel_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const pow_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.worker_account);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const fill_vesting_withdraw_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from_account);
                _impacted.insert(op.to_account);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const shutdown_witness_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.owner);
            }

            void operator()(const custom_operation &op) {
                for (auto s: op.required_auths) {
                    _impacted.insert(s);
                }
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const request_account_recovery_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account_to_recover);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const recover_account_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account_to_recover);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const change_recovery_account_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account_to_recover);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const escrow_transfer_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const escrow_approve_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const escrow_dispute_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const escrow_release_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
                _impacted.insert(op.agent);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const transfer_to_savings_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const transfer_from_savings_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
                _impacted.insert(op.to);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const cancel_transfer_from_savings_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.from);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const decline_voting_rights_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const comment_benefactor_reward_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.benefactor);
                _impacted.insert(op.author);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const delegate_vesting_shares_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.delegator);
                _impacted.insert(op.delegatee);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const return_vesting_delegation_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.account);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const asset_update_operation<Major, Hardfork, Release> &op) {
                if (op.new_issuer) {
                    _impacted.insert(*(op.new_issuer));
                }
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const asset_issue_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.issue_to_account);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const override_transfer_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.to);
                _impacted.insert(op.from);
                _impacted.insert(op.issuer);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const call_order_update_operation<Major, Hardfork, Release> &op) {
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const bid_collateral_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.bidder);
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            void operator()(const execute_bid_operation<Major, Hardfork, Release> &op) {
                _impacted.insert(op.bidder);
            }

            //void operator()( const operation& op ){}
        };

        void operation_get_impacted_accounts(const operation &op, flat_set<account_name_type> &result) {
            get_impacted_account_visitor vtor = get_impacted_account_visitor(result);
            op.visit(vtor);
        }

        void transaction_get_impacted_accounts(const transaction &tx, flat_set<account_name_type> &result) {
            for (const auto &op : tx.operations) {
                operation_get_impacted_accounts(op, result);
            }
        }

    }
}
