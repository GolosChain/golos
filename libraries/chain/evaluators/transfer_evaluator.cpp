#include <golos/chain/evaluators/transfer_evaluator.hpp>

#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/asset_object.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/database_exceptions.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_evaluator<Major, Hardfork, Release>::do_apply(
                const protocol::transfer_operation<Major, Hardfork, Release> &o) {

            const auto &from_account = this->db.template get_account(o.from);
            const auto &to_account = this->db.template get_account(o.to);

            const asset_object &asset_type = this->db.template get_asset(o.amount.symbol_name());

            STEEMIT_ASSERT(this->db.template is_authorized_asset(from_account, asset_type),
                           typename BOOST_IDENTITY_TYPE((exceptions::operations::transfer::from_account_not_whitelisted<
                                   Major, Hardfork, Release >)),
                           "'from' account ${from} is not whitelisted for asset ${asset}",
                           ("from", o.from)("asset", o.amount.symbol_name()));
            STEEMIT_ASSERT(this->db.template is_authorized_asset(to_account, asset_type),
                           typename BOOST_IDENTITY_TYPE((exceptions::operations::transfer::to_account_not_whitelisted<
                                   Major, Hardfork, Release >)),
                           "'to' account ${to} is not whitelisted for asset ${asset}",
                           ("to", o.to)("asset", o.amount.symbol_name()));

            if (asset_type.is_transfer_restricted()) {
                STEEMIT_ASSERT(from_account.name == asset_type.issuer || to_account.name == asset_type.issuer,
                               typename BOOST_IDENTITY_TYPE((exceptions::operations::transfer::restricted_transfer_asset<
                                       Major, Hardfork, Release >)),
                               "Asset {asset} has transfer_restricted flag enabled", ("asset", o.amount.symbol_name()));
            }

            if (from_account.active_challenged) {
                this->db.template modify(from_account, [&](account_object &a) {
                    a.active_challenged = false;
                    a.last_active_proved = this->db.template head_block_time();
                });
            }

            protocol::asset<0, 17, 0> required_amount(o.amount.amount, o.amount.symbol_name());

            FC_ASSERT(this->db.template get_balance(from_account, o.amount.symbol_name()) >= required_amount,
                      "Account does not have sufficient funds for transfer.");

            this->db.template adjust_balance(from_account, -required_amount);
            this->db.template adjust_balance(to_account, required_amount);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_to_vesting_evaluator<Major, Hardfork, Release>::do_apply(
                const protocol::transfer_to_vesting_operation<Major, Hardfork, Release> &o) {
            const auto &from_account = this->db.template get_account(o.from);
            const auto &to_account = o.to.size() ? this->db.template get_account(o.to) : from_account;

            protocol::asset<0, 17, 0> required_amount(o.amount.amount, o.amount.symbol_name());

            FC_ASSERT(this->db.template get_balance(from_account, STEEM_SYMBOL_NAME) >= required_amount,
                      "Account does not have sufficient {a} for transfer.", ("a", STEEM_SYMBOL_NAME));
            this->db.template adjust_balance(from_account, -required_amount);
            this->db.template create_vesting(to_account, required_amount);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_to_savings_evaluator<Major, Hardfork, Release>::do_apply(
                const transfer_to_savings_operation<Major, Hardfork, Release> &o) {
            const auto &from = this->db.template get_account(o.from);
            const auto &to = this->db.template get_account(o.to);

            protocol::asset<0, 17, 0> required_amount(o.amount.amount, o.amount.symbol_name());

            FC_ASSERT(this->db.template get_balance(from, o.amount.symbol_name()) >= required_amount,
                      "Account does not have sufficient funds to transfer to savings.");

            this->db.template adjust_balance(from, -required_amount);
            this->db.template adjust_savings_balance(to, required_amount);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_from_savings_evaluator<Major, Hardfork, Release>::do_apply(
                const protocol::transfer_from_savings_operation<Major, Hardfork, Release> &o) {
            const auto &from = this->db.template get_account(o.from);
            this->db.template get_account(o.to); // Verify to account exists

            FC_ASSERT(from.savings_withdraw_requests < STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT,
                      "Account has reached limit for pending withdraw requests.");

            protocol::asset<0, 17, 0> required_amount(o.amount.amount, o.amount.symbol_name());

            FC_ASSERT(this->db.template get_savings_balance(from, o.amount.symbol_name()) >= required_amount);
            this->db.template adjust_savings_balance(from, -required_amount);
            this->db.template create<savings_withdraw_object>([&](savings_withdraw_object &s) {
                s.from = o.from;
                s.to = o.to;
                s.amount = required_amount;
#ifndef STEEMIT_BUILD_LOW_MEMORY
                from_string(s.memo, o.memo);
#endif
                s.request_id = o.request_id;
                s.complete = this->db.template head_block_time() + STEEMIT_SAVINGS_WITHDRAW_TIME;
            });

            this->db.template modify(from, [&](account_object &a) {
                a.savings_withdraw_requests++;
            });
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void cancel_transfer_from_savings_evaluator<Major, Hardfork, Release>::do_apply(
                const protocol::cancel_transfer_from_savings_operation<Major, Hardfork, Release> &o) {
            const auto &swo = this->db.template get_savings_withdraw(o.from, o.request_id);
            this->db.template adjust_savings_balance(this->db.template get_account(swo.from), swo.amount);
            this->db.template remove(swo);

            const auto &from = this->db.template get_account(o.from);
            this->db.template modify(from, [&](account_object &a) {
                a.savings_withdraw_requests--;
            });
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void override_transfer_evaluator<Major, Hardfork, Release>::do_apply(
                const protocol::override_transfer_operation<Major, Hardfork, Release> &o) {
            try {
                const asset_object &asset_type = this->db.template get_asset(o.amount.symbol_name());
                STEEMIT_ASSERT(asset_type.can_override(),
                               typename BOOST_IDENTITY_TYPE((exceptions::operations::override_transfer::not_permitted<
                                       Major, Hardfork, Release >)),
                               "override_transfer not permitted for asset ${asset}", ("asset", o.amount.symbol_name()));
                FC_ASSERT(asset_type.issuer == o.issuer);

                const account_object &from_account = this->db.template get_account(o.from);
                const account_object &to_account = this->db.template get_account(o.to);

                FC_ASSERT(this->db.template is_authorized_asset(to_account, asset_type));
                FC_ASSERT(this->db.template is_authorized_asset(from_account, asset_type));

                FC_ASSERT(this->db.template get_balance(from_account, asset_type).amount >= o.amount.amount, "",
                          ("total_transfer", o.amount)("balance",
                                                       this->db.template get_balance(from_account, asset_type).amount));
                protocol::asset<0, 17, 0> required_amount(o.amount.amount, o.amount.symbol_name());

                this->db.template adjust_balance(this->db.template get_account(o.from), -required_amount);
                this->db.template adjust_balance(this->db.template get_account(o.to), required_amount);
            } FC_CAPTURE_AND_RETHROW((o))
        }
    }
}

#include <golos/chain/evaluators/transfer_evaluator.tpp>