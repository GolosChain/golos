#include <golos/protocol/operations/transfer_operations.hpp>

#include <fc/io/json.hpp>
#include <fc/utf8.hpp>

namespace golos {
    namespace protocol {
        /// TODO: after the hardfork, we can rename this method validate_permlink because it is strictily less restrictive than before
        ///  Issue #56 contains the justificiation for allowing any UTF-8 string to serve as a permlink, content will be grouped by tags
        ///  going forward.
        inline void validate_permlink(const string &permlink) {
            FC_ASSERT(permlink.size() < STEEMIT_MAX_PERMLINK_LENGTH, "permlink is too long");
            FC_ASSERT(fc::is_utf8(permlink), "permlink not formatted in UTF8");
        }

        inline void validate_account_name(const string &name) {
            FC_ASSERT(is_valid_account_name(name), "Account name ${n} is invalid", ("n", name));
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_operation<Major, Hardfork, Release>::validate() const {
            try {
                validate_account_name(from);
                validate_account_name(to);

                if (amount.symbol_name().size() <= 6) {
                    FC_ASSERT(amount.symbol_type_value() != VESTS_SYMBOL,
                              "transferring of Golos Power is not allowed.");
                }

                FC_ASSERT(amount.amount > 0, "Cannot transfer a negative amount (aka: stealing)");
                FC_ASSERT(memo.size() < STEEMIT_MAX_MEMO_SIZE, "Memo is too large");
                FC_ASSERT(fc::is_utf8(memo), "Memo is not UTF8");
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_to_vesting_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
            FC_ASSERT(amount.symbol_name() == STEEM_SYMBOL_NAME, "Amount must be STEEM");
            if (to != account_name_type()) {
                validate_account_name(to);
            }

            asset<Major, Hardfork, Release> default_steem(0, STEEM_SYMBOL_NAME);
            FC_ASSERT(amount > default_steem, "Must transfer a nonzero amount");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_to_savings_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
            validate_account_name(to);
            FC_ASSERT(amount.amount > 0);
            FC_ASSERT(amount.symbol_name() == STEEM_SYMBOL_NAME || amount.symbol_name() == SBD_SYMBOL_NAME);
            FC_ASSERT(memo.size() < STEEMIT_MAX_MEMO_SIZE, "Memo is too large");
            FC_ASSERT(fc::is_utf8(memo), "Memo is not UTF8");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void transfer_from_savings_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
            validate_account_name(to);
            FC_ASSERT(amount.amount > 0);
            FC_ASSERT(amount.symbol_name() == STEEM_SYMBOL_NAME || amount.symbol_name() == SBD_SYMBOL_NAME);
            FC_ASSERT(memo.size() < STEEMIT_MAX_MEMO_SIZE, "Memo is too large");
            FC_ASSERT(fc::is_utf8(memo), "Memo is not UTF8");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void cancel_transfer_from_savings_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(from);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void override_transfer_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(from != to);
            FC_ASSERT(amount.amount > 0);
            FC_ASSERT(issuer != from);
        }
    }
}

#include <golos/protocol/operations/transfer_operations.tpp>