#include <golos/protocol/operations/witness_operations.hpp>

namespace golos {
    namespace protocol {
        inline void validate_account_name(const string &name) {
            FC_ASSERT(is_valid_account_name(name), "Account name ${n} is invalid", ("n", name));
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void witness_update_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(owner);
            FC_ASSERT(url.size() > 0, "URL size must be greater than 0");
            FC_ASSERT(fc::is_utf8(url), "URL is not valid UTF8");

            asset<Major, Hardfork, Release> default_steem(0, STEEM_SYMBOL_NAME);
            FC_ASSERT(fee >= default_steem, "Fee cannot be negative");
            props.validate();
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void account_witness_vote_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(account);
            validate_account_name(witness);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void account_witness_proxy_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(account);
            if (proxy.size()) {
                validate_account_name(proxy);
            }
            FC_ASSERT(proxy != account, "Cannot proxy to self");
        }
    }
}

#include <golos/protocol/operations/witness_operations.tpp>