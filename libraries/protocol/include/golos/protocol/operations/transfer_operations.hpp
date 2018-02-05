#ifndef GOLOS_TRANSFER_OPERATIONS_HPP
#define GOLOS_TRANSFER_OPERATIONS_HPP

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

namespace golos {
    namespace protocol {
        /**
         * @ingroup operations
         *
         * @brief Transfers an amount of one asset<Major, Hardfork, Release> from one account to another
         *
         *  Fees are paid by the "from" account
         *
         *  @pre amount.amount > 0
         *  @pre from != to
         *  @post from account's balance will be reduced by fee and amount
         *  @post to account's balance will be increased by amount
         *  @return n/a
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct transfer_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            /// Account to transfer asset<Major, Hardfork, Release> to
            account_name_type to;
            /// The amount of asset<Major, Hardfork, Release> to transfer from @ref from to @ref to
            asset <Major, Hardfork, Release> amount;

            /// The memo is plain-text, any encryption on the memo is up to
            /// a higher level protocol.
            string memo;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                if (amount.symbol_name().size() <= 6 && amount.symbol_type_value() != VESTS_SYMBOL) {
                    a.insert(from);
                } else {
                    a.insert(from);
                }
            }

            void get_required_owner_authorities(flat_set <account_name_type> &a) const {
                if (amount.symbol_name().size() <= 6 && amount.symbol_type_value() == VESTS_SYMBOL) {
                    a.insert(from);
                }
            }
        };

        /**
         *  This operation converts STEEM into VFS (Vesting Fund Shares) at
         *  the current exchange rate. With this operation it is possible to
         *  give another account vesting shares so that faucets can
         *  pre-fund new accounts with vesting shares.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct transfer_to_vesting_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            account_name_type to; ///< if null, then same as from
            asset <Major, Hardfork, Release> amount; ///< must be STEEM

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(from);
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct transfer_to_savings_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            account_name_type to;
            asset <Major, Hardfork, Release> amount;
            string memo;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(from);
            }

            void validate() const;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct transfer_from_savings_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            integral_id_type request_id = 0;
            account_name_type to;
            asset <Major, Hardfork, Release> amount;
            string memo;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(from);
            }

            void validate() const;
        };


        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct cancel_transfer_from_savings_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            integral_id_type request_id = 0;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(from);
            }

            void validate() const;
        };

        /**
         *  @class override_transfer_operation
         *  @brief Allows the issuer of an asset<Major, Hardfork, Release> to transfer an asset<Major, Hardfork, Release> from any account to any account if they have override_authority
         *  @ingroup operations
         *
         *  @pre amount.asset_id->issuer == issuer
         *  @pre issuer != from  because this is pointless, use a normal transfer operation
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct override_transfer_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type issuer;
            /// Account to transfer asset from
            account_name_type from;
            /// Account to transfer asset to
            account_name_type to;
            /// The amount of asset to transfer from @ref from to @ref to
            asset <Major, Hardfork, Release> amount;

            /// User provided data encrypted to the memo key of the "to" account
            string memo;
            extensions_type extensions;

            void validate() const;
        };

    }
}

FC_REFLECT((golos::protocol::transfer_operation<0, 16, 0>), (from)(to)(amount)(memo))
FC_REFLECT((golos::protocol::transfer_operation<0, 17, 0>), (from)(to)(amount)(memo))

FC_REFLECT((golos::protocol::transfer_to_vesting_operation<0, 16, 0>), (from)(to)(amount))
FC_REFLECT((golos::protocol::transfer_to_vesting_operation<0, 17, 0>), (from)(to)(amount))

FC_REFLECT((golos::protocol::transfer_to_savings_operation<0, 16, 0>), (from)(to)(amount)(memo))
FC_REFLECT((golos::protocol::transfer_to_savings_operation<0, 17, 0>), (from)(to)(amount)(memo))

FC_REFLECT((golos::protocol::transfer_from_savings_operation<0, 16, 0>), (from)(request_id)(to)(amount)(memo))
FC_REFLECT((golos::protocol::transfer_from_savings_operation<0, 17, 0>), (from)(request_id)(to)(amount)(memo))

FC_REFLECT((golos::protocol::cancel_transfer_from_savings_operation<0, 16, 0>), (from)(request_id))
FC_REFLECT((golos::protocol::cancel_transfer_from_savings_operation<0, 17, 0>), (from)(request_id))

FC_REFLECT((golos::protocol::override_transfer_operation<0, 17, 0>), (issuer)(from)(to)(amount)(memo)(extensions))

#endif //GOLOS_TRANSFER_OPERATIONS_HPP