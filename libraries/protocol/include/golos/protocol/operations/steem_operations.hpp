#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>
#include <golos/protocol/chain_properties.hpp>

#include <fc/utf8.hpp>
#include <fc/crypto/equihash.hpp>

namespace golos {
    namespace protocol {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct challenge_authority_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type challenger;
            account_name_type challenged;
            bool require_owner = false;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(challenger);
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct prove_authority_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type challenged;
            bool require_owner = false;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                if (!require_owner) {
                    a.insert(challenged);
                }
            }

            void get_required_owner_authorities(flat_set <account_name_type> &a) const {
                if (require_owner) {
                    a.insert(challenged);
                }
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct vote_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type voter;
            account_name_type author;
            string permlink;
            int16_t weight = 0;

            void validate() const;

            void get_required_posting_authorities(flat_set <account_name_type> &a) const {
                a.insert(voter);
            }
        };

        /**
         * At any given point in time an account can be withdrawing from their
         * vesting shares. A user may change the number of shares they wish to
         * cash out at any time between 0 and their total vesting stake.
         *
         * After applying this operation, vesting_shares will be withdrawn
         * at a rate of vesting_shares/104 per week for two years starting
         * one week after this operation is included in the blockchain.
         *
         * This operation is not valid if the user has no vesting shares.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct withdraw_vesting_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type account;
            asset <Major, Hardfork, Release> vesting_shares;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(account);
            }
        };


        /**
         * Allows an account to setup a vesting withdraw but with the additional
         * request for the funds to be transferred directly to another account's
         * balance rather than the withdrawing account. In addition, those funds
         * can be immediately vested again, circumventing the conversion from
         * vests to steem and back, guaranteeing they maintain their value.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct set_withdraw_vesting_route_operation
                : public base_operation<Major, Hardfork, Release> {
            account_name_type from_account;
            account_name_type to_account;
            uint16_t percent = 0;
            bool auto_vest = false;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(from_account);
            }
        };

        /**
         *  Feeds can only be published by the top N witnesses which are included in every round and are
         *  used to define the exchange rate between steem and the dollar.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct feed_publish_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type publisher;
            price<Major, Hardfork, Release> exchange_rate;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(publisher);
            }
        };

        struct pow {
            public_key_type worker;
            digest_type input;
            signature_type signature;
            digest_type work;

            void create(const fc::ecc::private_key &w, const digest_type &i);

            void validate() const;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct pow_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type worker_account;
            block_id_type block_id;
            uint64_t nonce = 0;
            pow work;
            chain_properties<Major, Hardfork, Release> props;

            void validate() const;

            fc::sha256 work_input() const;

            const account_name_type &get_worker_account() const {
                return worker_account;
            }

            /** there is no need to verify authority, the proof of work is sufficient */
            void get_required_active_authorities(flat_set <account_name_type> &a) const {
            }
        };

        struct pow2_input {
            account_name_type worker_account;
            block_id_type prev_block;
            uint64_t nonce = 0;
        };

        struct pow2 {
            pow2_input input;
            uint32_t pow_summary = 0;

            void create(const block_id_type &prev_block, const account_name_type &account_name, uint64_t nonce);

            void validate() const;
        };

        struct equihash_pow {
            pow2_input input;
            fc::equihash::proof proof;
            block_id_type prev_block;
            uint32_t pow_summary = 0;

            void create(const block_id_type &recent_block, const account_name_type &account_name, uint32_t nonce);

            void validate() const;
        };

        typedef fc::static_variant<pow2, equihash_pow> pow2_work;

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct pow2_operation : public base_operation<Major, Hardfork, Release> {
            pow2_work work;
            optional <public_key_type> new_owner_key;
            chain_properties<Major, Hardfork, Release> props;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const;

            void get_required_authorities(vector <authority> &a) const {
                if (new_owner_key) {
                    a.push_back(authority(1, *new_owner_key, 1));
                }
            }
        };


        /**
         * This operation is used to report a miner who signs two blocks
         * at the same time. To be valid, the violation must be reported within
         * STEEMIT_MAX_WITNESSES blocks of the head block (1 round) and the
         * producer must be in the ACTIVE witness set.
         *
         * Users not in the ACTIVE witness set should not have to worry about their
         * key getting compromised and being used to produced multiple blocks so
         * the attacker can report it and steel their vesting steem.
         *
         * The result of the operation is to transfer the full VESTING STEEM balance
         * of the block producer to the reporter.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct report_over_production_operation
                : public base_operation<Major, Hardfork, Release> {
            account_name_type reporter;
            signed_block_header first_block;
            signed_block_header second_block;

            void validate() const;
        };


        /**
         * All account recovery requests come from a listed recovery account. This
         * is secure based on the assumption that only a trusted account should be
         * a recovery account. It is the responsibility of the recovery account to
         * verify the identity of the account holder of the account to recover by
         * whichever means they have agreed upon. The blockchain assumes identity
         * has been verified when this operation is broadcast.
         *
         * This operation creates an account recovery request which the account to
         * recover has 24 hours to respond to before the request expires and is
         * invalidated.
         *
         * There can only be one active recovery request per account at any one time.
         * Pushing this operation for an account to recover when it already has
         * an active request will either update the request to a new new owner authority
         * and extend the request expiration to 24 hours from the current head block
         * time or it will delete the request. To cancel a request, simply set the
         * weight threshold of the new owner authority to 0, making it an open authority.
         *
         * Additionally, the new owner authority must be satisfiable. In other words,
         * the sum of the key weights must be greater than or equal to the weight
         * threshold.
         *
         * This operation only needs to be signed by the the recovery account.
         * The account to recover confirms its identity to the blockchain in
         * the recover account operation.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct request_account_recovery_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type recovery_account;       ///< The recovery account is listed as the recovery account on the account to recover.

            account_name_type account_to_recover;     ///< The account to recover. This is likely due to a compromised owner authority.

            authority new_owner_authority;    ///< The new owner authority the account to recover wishes to have. This is secret
            ///< known by the account to recover and will be confirmed in a recover_account_operation

            extensions_type extensions;             ///< Extensions. Not currently used.

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(recovery_account);
            }

            void validate() const;
        };


        /**
         * Recover an account to a new authority using a previous authority and verification
         * of the recovery account as proof of identity. This operation can only succeed
         * if there was a recovery request sent by the account's recover account.
         *
         * In order to recover the account, the account holder must provide proof
         * of past ownership and proof of identity to the recovery account. Being able
         * to satisfy an owner authority that was used in the past 30 days is sufficient
         * to prove past ownership. The get_owner_history function in the database API
         * returns past owner authorities that are valid for account recovery.
         *
         * Proving identity is an off chain contract between the account holder and
         * the recovery account. The recovery request contains a new authority which
         * must be satisfied by the account holder to regain control. The actual process
         * of verifying authority may become complicated, but that is an application
         * level concern, not a blockchain concern.
         *
         * This operation requires both the past and future owner authorities in the
         * operation because neither of them can be derived from the current chain state.
         * The operation must be signed by keys that satisfy both the new owner authority
         * and the recent owner authority. Failing either fails the operation entirely.
         *
         * If a recovery request was made inadvertantly, the account holder should
         * contact the recovery account to have the request deleted.
         *
         * The two setp combination of the account recovery request and recover is
         * safe because the recovery account never has access to secrets of the account
         * to recover. They simply act as an on chain endorsement of off chain identity.
         * In other systems, a fork would be required to enforce such off chain state.
         * Additionally, an account cannot be permanently recovered to the wrong account.
         * While any owner authority from the past 30 days can be used, including a compromised
         * authority, the account can be continually recovered until the recovery account
         * is confident a combination of uncompromised authorities were used to
         * recover the account. The actual process of verifying authority may become
         * complicated, but that is an application level concern, not the blockchain's
         * concern.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct recover_account_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type account_to_recover;        ///< The account to be recovered

            authority new_owner_authority;       ///< The new owner authority as specified in the request account recovery operation.

            authority recent_owner_authority;    ///< A previous owner authority that the account holder will use to prove past ownership of the account to be recovered.

            extensions_type extensions;                ///< Extensions. Not currently used.

            void get_required_authorities(vector <authority> &a) const {
                a.push_back(new_owner_authority);
                a.push_back(recent_owner_authority);
            }

            void validate() const;
        };


        /**
         *  This operation allows recovery_accoutn to change account_to_reset's owner authority to
         *  new_owner_authority after 60 days of inactivity.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct reset_account_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type reset_account;
            account_name_type account_to_reset;
            authority new_owner_authority;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(reset_account);
            }

            void validate() const;
        };

        /**
         * This operation allows 'account' owner to control which account has the power
         * to execute the 'reset_account_operation' after 60 days.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct set_reset_account_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type account;
            account_name_type current_reset_account;
            account_name_type reset_account;

            void validate() const;

            void get_required_owner_authorities(flat_set <account_name_type> &a) const {
                if (current_reset_account.size()) {
                    a.insert(account);
                }
            }

            void get_required_posting_authorities(flat_set <account_name_type> &a) const {
                if (!current_reset_account.size()) {
                    a.insert(account);
                }
            }
        };


        /**
         * Each account lists another account as their recovery account.
         * The recovery account has the ability to create account_recovery_requests
         * for the account to recover. An account can change their recovery account
         * at any time with a 30 day delay. This delay is to prevent
         * an attacker from changing the recovery account to a malicious account
         * during an attack. These 30 days match the 30 days that an
         * owner authority is valid for recovery purposes.
         *
         * On account creation the recovery account is set either to the creator of
         * the account (The account that pays the creation fee and is a signer on the transaction)
         * or to the empty string if the account was mined. An account with no recovery
         * has the top voted witness as a recovery account, at the time the recover
         * request is created. Note: This does mean the effective recovery account
         * of an account with no listed recovery account can change at any time as
         * witness vote weights. The top voted witness is explicitly the most trusted
         * witness according to stake.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct change_recovery_account_operation
                : public base_operation<Major, Hardfork, Release> {
            account_name_type account_to_recover;     ///< The account that would be recovered in case of compromise
            account_name_type new_recovery_account;   ///< The account that creates the recover request
            extensions_type extensions;             ///< Extensions. Not currently used.

            void get_required_owner_authorities(flat_set <account_name_type> &a) const {
                a.insert(account_to_recover);
            }

            void validate() const;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct decline_voting_rights_operation
                : public base_operation<Major, Hardfork, Release> {
            account_name_type account;
            bool decline = true;

            void get_required_owner_authorities(flat_set <account_name_type> &a) const {
                a.insert(account);
            }

            void validate() const;
        };

        /**
         * Delegate vesting shares from one account to the other. The vesting shares are still owned
         * by the original account, but content voting rights and bandwidth allocation are transferred
         * to the receiving account. This sets the delegation to `vesting_shares`, increasing it or
         * decreasing it as needed. (i.e. a delegation of 0 removes the delegation)
         *
         * When a delegation is removed the shares are placed in limbo for a week to prevent a satoshi
         * of VESTS from voting on the same content twice.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct delegate_vesting_shares_operation
                : public base_operation<Major, Hardfork, Release> {
            account_name_type delegator;        ///< The account delegating vesting shares
            account_name_type delegatee;        ///< The account receiving vesting shares
            asset <Major, Hardfork, Release> vesting_shares;   ///< The amount of vesting shares delegated

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                a.insert(delegator);
            }

            void validate() const;
        };
    }
} // golos::protocol


FC_REFLECT((golos::protocol::reset_account_operation<0, 16, 0>), (reset_account)(account_to_reset)(new_owner_authority))
FC_REFLECT((golos::protocol::reset_account_operation<0, 17, 0>), (reset_account)(account_to_reset)(new_owner_authority))

FC_REFLECT((golos::protocol::set_reset_account_operation<0, 16, 0>), (account)(current_reset_account)(reset_account))
FC_REFLECT((golos::protocol::set_reset_account_operation<0, 17, 0>), (account)(current_reset_account)(reset_account))

FC_REFLECT((golos::protocol::report_over_production_operation<0, 16, 0>), (reporter)(first_block)(second_block))
FC_REFLECT((golos::protocol::report_over_production_operation<0, 17, 0>), (reporter)(first_block)(second_block))

FC_REFLECT((golos::protocol::feed_publish_operation<0, 16, 0>), (publisher)(exchange_rate))
FC_REFLECT((golos::protocol::feed_publish_operation<0, 17, 0>), (publisher)(exchange_rate))

FC_REFLECT((golos::protocol::pow), (worker)(input)(signature)(work))
FC_REFLECT((golos::protocol::pow2), (input)(pow_summary))
FC_REFLECT((golos::protocol::pow2_input), (worker_account)(prev_block)(nonce))
FC_REFLECT((golos::protocol::equihash_pow), (input)(proof)(prev_block)(pow_summary))

FC_REFLECT_TYPENAME((golos::protocol::pow2_work))
FC_REFLECT((golos::protocol::pow_operation<0, 16, 0>), (worker_account)(block_id)(nonce)(work)(props))
FC_REFLECT((golos::protocol::pow_operation<0, 17, 0>), (worker_account)(block_id)(nonce)(work)(props))

FC_REFLECT((golos::protocol::pow2_operation<0, 16, 0>), (work)(new_owner_key)(props))
FC_REFLECT((golos::protocol::pow2_operation<0, 17, 0>), (work)(new_owner_key)(props))

FC_REFLECT((golos::protocol::withdraw_vesting_operation<0, 16, 0>), (account)(vesting_shares))
FC_REFLECT((golos::protocol::withdraw_vesting_operation<0, 17, 0>), (account)(vesting_shares))

FC_REFLECT((golos::protocol::set_withdraw_vesting_route_operation<0, 16, 0>), (from_account)(to_account)(percent)(auto_vest))
FC_REFLECT((golos::protocol::set_withdraw_vesting_route_operation<0, 17, 0>), (from_account)(to_account)(percent)(auto_vest))

FC_REFLECT((golos::protocol::vote_operation<0, 16, 0>), (voter)(author)(permlink)(weight))
FC_REFLECT((golos::protocol::vote_operation<0, 17, 0>), (voter)(author)(permlink)(weight))

FC_REFLECT((golos::protocol::challenge_authority_operation<0, 16, 0>), (challenger)(challenged)(require_owner));
FC_REFLECT((golos::protocol::challenge_authority_operation<0, 17, 0>), (challenger)(challenged)(require_owner));

FC_REFLECT((golos::protocol::prove_authority_operation<0, 16, 0>), (challenged)(require_owner));
FC_REFLECT((golos::protocol::prove_authority_operation<0, 17, 0>), (challenged)(require_owner));

FC_REFLECT((golos::protocol::request_account_recovery_operation<0, 16, 0>), (recovery_account)(account_to_recover)(new_owner_authority)(extensions));
FC_REFLECT((golos::protocol::request_account_recovery_operation<0, 17, 0>), (recovery_account)(account_to_recover)(new_owner_authority)(extensions));

FC_REFLECT((golos::protocol::recover_account_operation<0, 16, 0>), (account_to_recover)(new_owner_authority)(recent_owner_authority)(extensions));
FC_REFLECT((golos::protocol::recover_account_operation<0, 17, 0>), (account_to_recover)(new_owner_authority)(recent_owner_authority)(extensions));

FC_REFLECT((golos::protocol::change_recovery_account_operation<0, 16, 0>), (account_to_recover)(new_recovery_account)(extensions));
FC_REFLECT((golos::protocol::change_recovery_account_operation<0, 17, 0>), (account_to_recover)(new_recovery_account)(extensions));

FC_REFLECT((golos::protocol::decline_voting_rights_operation<0, 16, 0>), (account)(decline));
FC_REFLECT((golos::protocol::decline_voting_rights_operation<0, 17, 0>), (account)(decline));

FC_REFLECT((golos::protocol::delegate_vesting_shares_operation<0, 17, 0>), (delegator)(delegatee)(vesting_shares));