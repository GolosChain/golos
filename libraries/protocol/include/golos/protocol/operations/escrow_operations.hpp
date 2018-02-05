#ifndef GOLOS_ESCROW_OPERATIONS_HPP
#define GOLOS_ESCROW_OPERATIONS_HPP

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

namespace golos {
    namespace protocol {
        /**
         *  The purpose of this operation is to enable someone to send money contingently to
         *  another individual. The funds leave the *from* account and go into a temporary balance
         *  where they are held until *from* releases it to *to* or *to* refunds it to *from*.
         *
         *  In the event of a dispute the *agent* can divide the funds between the to/from account.
         *  Disputes can be raised any time before or on the dispute deadline time, after the escrow
         *  has been approved by all parties.
         *
         *  This operation only creates a proposed escrow transfer. Both the *agent* and *to* must
         *  agree to the terms of the arrangement by approving the escrow.
         *
         *  The escrow agent is paid the fee on approval of all parties. It is up to the escrow agent
         *  to determine the fee.
         *
         *  Escrow transactions are uniquely identified by 'from' and 'escrow_id', the 'escrow_id' is defined
         *  by the sender.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct escrow_transfer_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            account_name_type to;
            account_name_type agent;
            uint32_t escrow_id = 30;

            asset<Major, Hardfork, Release> sbd_amount = {0, SBD_SYMBOL_NAME};
            asset<Major, Hardfork, Release> steem_amount = {0, STEEM_SYMBOL_NAME};
            asset<Major, Hardfork, Release> fee;

            time_point_sec ratification_deadline;
            time_point_sec escrow_expiration;

            string json_meta;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(from);
            }
        };


        /**
         *  The agent and to accounts must approve an escrow transaction for it to be valid on
         *  the blockchain. Once a part approves the escrow, the cannot revoke their approval.
         *  Subsequent escrow approve operations, regardless of the approval, will be rejected.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct escrow_approve_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            account_name_type to;
            account_name_type agent;
            account_name_type who; // Either to or agent

            uint32_t escrow_id = 30;
            bool approve = true;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(who);
            }
        };


        /**
         *  If either the sender or receiver of an escrow payment has an issue, they can
         *  raise it for dispute. Once a payment is in dispute, the agent has authority over
         *  who gets what.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct escrow_dispute_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            account_name_type to;
            account_name_type agent;
            account_name_type who;

            uint32_t escrow_id = 30;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(who);
            }
        };


        /**
         *  This operation can be used by anyone associated with the escrow transfer to
         *  release funds if they have permission.
         *
         *  The permission scheme is as follows:
         *  If there is no dispute and escrow has not expired, either party can release funds to the other.
         *  If escrow expires and there is no dispute, either party can release funds to either party.
         *  If there is a dispute regardless of expiration, the agent can release funds to either party
         *     following whichever agreement was in place between the parties.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct escrow_release_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type from;
            account_name_type to; ///< the original 'to'
            account_name_type agent;
            account_name_type who; ///< the account that is attempting to release the funds, determines valid 'receiver'
            account_name_type receiver; ///< the account that should receive funds (might be from, might be to)

            uint32_t escrow_id = 30;
            asset<Major, Hardfork, Release> sbd_amount = {0, SBD_SYMBOL_NAME}; ///< the amount of sbd to release
            asset<Major, Hardfork, Release> steem_amount = {0, STEEM_SYMBOL_NAME}; ///< the amount of steem to release

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(who);
            }
        };
    }
}

FC_REFLECT((golos::protocol::escrow_transfer_operation<0, 16, 0>),
           (from)(to)(sbd_amount)(steem_amount)(escrow_id)(agent)(fee)(json_meta)(ratification_deadline)(
                   escrow_expiration));
FC_REFLECT((golos::protocol::escrow_transfer_operation<0, 17, 0>),
           (from)(to)(sbd_amount)(steem_amount)(escrow_id)(agent)(fee)(json_meta)(ratification_deadline)(
                   escrow_expiration));

FC_REFLECT((golos::protocol::escrow_approve_operation<0, 16, 0>),
           (from)(to)(agent)(who)(escrow_id)(approve));
FC_REFLECT((golos::protocol::escrow_approve_operation<0, 17, 0>),
           (from)(to)(agent)(who)(escrow_id)(approve));

FC_REFLECT((golos::protocol::escrow_dispute_operation<0, 16, 0>),
           (from)(to)(agent)(who)(escrow_id));
FC_REFLECT((golos::protocol::escrow_dispute_operation<0, 17, 0>),
           (from)(to)(agent)(who)(escrow_id));

FC_REFLECT((golos::protocol::escrow_release_operation<0, 16, 0>),
           (from)(to)(agent)(who)(receiver)(escrow_id)(sbd_amount)(steem_amount));
FC_REFLECT((golos::protocol::escrow_release_operation<0, 17, 0>),
           (from)(to)(agent)(who)(receiver)(escrow_id)(sbd_amount)(steem_amount));

#endif //GOLOS_ESCROW_OPERATIONS_HPP