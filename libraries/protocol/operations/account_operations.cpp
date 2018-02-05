#include <golos/protocol/operations/account_operations.hpp>

#include <fc/utf8.hpp>
#include <fc/io/json.hpp>

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
        void account_create_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(new_account_name);
            FC_ASSERT(fee.symbol_name() == STEEM_SYMBOL_NAME, "Account creation fee must be STEEM");
            owner.validate();
            active.validate();

            if (json_metadata.size() > 0) {
                FC_ASSERT(fc::is_utf8(json_metadata), "JSON Metadata not formatted in UTF8");
                FC_ASSERT(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            }

            FC_ASSERT(fee >= typename BOOST_IDENTITY_TYPE((asset<Major, Hardfork, Release>))(0, STEEM_SYMBOL_NAME),
                      "Account creation fee cannot be negative");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void account_create_with_delegation_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(new_account_name);
            validate_account_name(creator);
            FC_ASSERT(fee.symbol_name() == STEEM_SYMBOL_NAME, "Account creation fee must be STEEM");
            FC_ASSERT(delegation.symbol_type_value() == VESTS_SYMBOL, "Delegation must be VESTS");

            owner.validate();
            active.validate();
            posting.validate();

            if (json_metadata.size() > 0) {
                FC_ASSERT(fc::is_utf8(json_metadata), "JSON Metadata not formatted in UTF8");
                FC_ASSERT(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            }

            asset<Major, Hardfork, Release> default_steem(0, STEEM_SYMBOL_NAME);
            asset<Major, Hardfork, Release> default_vesting(0, VESTS_SYMBOL);

            FC_ASSERT(fee >= default_steem, "Account creation fee cannot be negative");
            FC_ASSERT(delegation >= default_vesting, "Delegation cannot be negative");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void account_update_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(account);
            /*if( owner )
               owner->validate();
            if( active )
               active->validate();
            if( posting )
               posting->validate();*/

            if (json_metadata.size() > 0) {
                FC_ASSERT(fc::is_utf8(json_metadata), "JSON Metadata not formatted in UTF8");
                FC_ASSERT(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void account_whitelist_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(fee.amount >= 0);
            FC_ASSERT(new_listing < 0x4);
        }
    }
}

#include <golos/protocol/operations/account_operations.tpp>