#ifndef GOLOS_WITNESS_OPERATIONS_HPP
#define GOLOS_WITNESS_OPERATIONS_HPP

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>
#include <golos/protocol/chain_properties.hpp>

#include <fc/utf8.hpp>
#include <fc/crypto/equihash.hpp>

namespace golos {
    namespace protocol {
        /**
         *  Users who wish to become a witness must pay a fee acceptable to
         *  the current witnesses to apply for the position and allow voting
         *  to begin.
         *
         *  If the owner isn't a witness they will become a witness.  Witnesses
         *  are charged a fee equal to 1 weeks worth of witness pay which in
         *  turn is derived from the current share supply.  The fee is
         *  only applied if the owner is not already a witness.
         *
         *  If the block_signing_key is null then the witness is removed from
         *  contention.  The network will pick the top 21 witnesses for
         *  producing blocks.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct witness_update_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type owner;
            string url;
            public_key_type block_signing_key;
            chain_properties<Major, Hardfork, Release> props;
            asset <Major, Hardfork, Release> fee; ///< the fee paid to register a new witness, should be 10x current block production pay

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(owner);
            }
        };


        /**
         * All accounts with a VFS can vote for or against any witness.
         *
         * If a proxy is specified then all existing votes are removed.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct account_witness_vote_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type account;
            account_name_type witness;
            bool approve = true;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(account);
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct account_witness_proxy_operation
                : public base_operation<Major, Hardfork, Release> {
            account_name_type account;
            account_name_type proxy;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(account);
            }
        };
    }
}

FC_REFLECT((golos::protocol::witness_update_operation<0, 16, 0>), (owner)(url)(block_signing_key)(props)(fee))
FC_REFLECT((golos::protocol::witness_update_operation<0, 17, 0>), (owner)(url)(block_signing_key)(props)(fee))

FC_REFLECT((golos::protocol::account_witness_vote_operation<0, 16, 0>), (account)(witness)(approve))
FC_REFLECT((golos::protocol::account_witness_vote_operation<0, 17, 0>), (account)(witness)(approve))

FC_REFLECT((golos::protocol::account_witness_proxy_operation<0, 16, 0>), (account)(proxy))
FC_REFLECT((golos::protocol::account_witness_proxy_operation<0, 17, 0>), (account)(proxy))

#endif //GOLOS_WITNESS_OPERATIONS_HPP
