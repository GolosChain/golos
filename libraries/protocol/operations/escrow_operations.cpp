#include <golos/protocol/operations/escrow_operations.hpp>

#include <fc/utf8.hpp>
#include <fc/io/json.hpp>

namespace golos {
    namespace protocol {
        /// TODO: after the hardfork, we can rename this method validate_permlink because it is strictily less restrictive than before
        ///  Issue #56 contains the justificiation for allowing any UTF-8 string to serve as a permlink, content will be grouped by tags
        ///  going forward.
        inline void validate_permlink(const string &permlink) {
            FC_ASSERT(permlink.size() <
                      STEEMIT_MAX_PERMLINK_LENGTH, "permlink is too long");
            FC_ASSERT(fc::is_utf8(permlink), "permlink not formatted in UTF8");
        }

        inline void validate_account_name(const string &name) {
            FC_ASSERT(is_valid_account_name(name), "Account name ${n} is invalid", ("n", name));
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_transfer_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
            validate_account_name(to);
            validate_account_name(agent);
            FC_ASSERT(fee.amount >= 0, "fee cannot be negative");
            FC_ASSERT(sbd_amount.amount >= 0, "sbd amount cannot be negative");
            FC_ASSERT(steem_amount.amount >= 0, "steem amount cannot be negative");
            FC_ASSERT(sbd_amount.amount > 0 || steem_amount.amount > 0, "escrow must transfer a non-zero amount");
            FC_ASSERT(from != agent && to != agent, "agent must be a third party");
            FC_ASSERT((fee.symbol_name() == STEEM_SYMBOL_NAME) || (fee.symbol_name() == SBD_SYMBOL_NAME), "fee must be STEEM or SBD");
            FC_ASSERT(sbd_amount.symbol_name() == SBD_SYMBOL_NAME, "sbd amount must contain SBD");
            FC_ASSERT(steem_amount.symbol_name() == STEEM_SYMBOL_NAME, "steem amount must contain STEEM");
            FC_ASSERT(ratification_deadline < escrow_expiration,
                      "ratification deadline must be before escrow expiration");
            if (json_meta.size() > 0) {
                FC_ASSERT(fc::is_utf8(json_meta), "JSON Metadata not formatted in UTF8");
                FC_ASSERT(fc::json::is_valid(json_meta), "JSON Metadata not valid JSON");
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_approve_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
            validate_account_name(to);
            validate_account_name(agent);
            validate_account_name(who);
            FC_ASSERT(who == to || who == agent, "to or agent must approve escrow");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_dispute_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
            validate_account_name(to);
            validate_account_name(agent);
            validate_account_name(who);
            FC_ASSERT(who == from || who == to, "who must be from or to");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_release_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
            validate_account_name(to);
            validate_account_name(agent);
            validate_account_name(who);
            validate_account_name(receiver);
            FC_ASSERT(who == from || who == to || who == agent, "who must be from or to or agent");
            FC_ASSERT(receiver == from || receiver == to, "receiver must be from or to");
            FC_ASSERT(sbd_amount.amount >= 0, "sbd amount cannot be negative");
            FC_ASSERT(steem_amount.amount >= 0, "steem amount cannot be negative");
            FC_ASSERT(sbd_amount.amount > 0 || steem_amount.amount > 0, "escrow must release a non-zero amount");
            FC_ASSERT(sbd_amount.symbol_name() == SBD_SYMBOL_NAME, "sbd amount must contain SBD");
            FC_ASSERT(steem_amount.symbol_name() == STEEM_SYMBOL_NAME, "steem amount must contain STEEM");
        }
    }
}

#include <golos/protocol/operations/escrow_operations.tpp>