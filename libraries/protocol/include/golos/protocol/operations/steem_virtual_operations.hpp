#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

#include <fc/utf8.hpp>

namespace golos {
    namespace protocol {

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct author_reward_operation : public virtual_operation<Major, Hardfork, Release> {
            author_reward_operation() {
            }

            author_reward_operation(const account_name_type &a, const string &p,
                                    const asset <Major, Hardfork, Release> &s,
                                    const asset <Major, Hardfork, Release> &st,
                                    const asset <Major, Hardfork, Release> &v) : author(a), permlink(p), sbd_payout(s),
                    steem_payout(st), vesting_payout(v) {
            }

            account_name_type author;
            string permlink;
            asset <Major, Hardfork, Release> sbd_payout;
            asset <Major, Hardfork, Release> steem_payout;
            asset <Major, Hardfork, Release> vesting_payout;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct curation_reward_operation : public virtual_operation<Major, Hardfork, Release> {
            curation_reward_operation() {
            }

            curation_reward_operation(const string &c, const asset <Major, Hardfork, Release> &r, const string &a,
                                      const string &p) : curator(c), reward(r), comment_author(a), comment_permlink(p) {
            }

            account_name_type curator;
            asset <Major, Hardfork, Release> reward;
            account_name_type comment_author;
            string comment_permlink;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct comment_reward_operation : public virtual_operation<Major, Hardfork, Release> {
            comment_reward_operation() {
            }

            comment_reward_operation(const account_name_type &a, const string &pl,
                                     const asset <Major, Hardfork, Release> &p) : author(a), permlink(pl), payout(p) {
            }

            account_name_type author;
            string permlink;
            asset <Major, Hardfork, Release> payout;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct liquidity_reward_operation : public virtual_operation<Major, Hardfork, Release> {
            liquidity_reward_operation(string o = string(),
                                       asset <Major, Hardfork, Release> p = asset<Major, Hardfork, Release>()) : owner(
                    o), payout(p) {
            }

            account_name_type owner;
            asset <Major, Hardfork, Release> payout;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct interest_operation : public virtual_operation<Major, Hardfork, Release> {
            interest_operation(const string &o = "",
                               const asset <Major, Hardfork, Release> &i = asset<Major, Hardfork, Release>(0,
                                                                                                           SBD_SYMBOL_NAME))
                    : owner(o), interest(i) {
            }

            account_name_type owner;
            asset <Major, Hardfork, Release> interest;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct fill_convert_request_operation
                : public virtual_operation<Major, Hardfork, Release> {
            fill_convert_request_operation() {
            }

            fill_convert_request_operation(const string &o, const uint32_t id,
                                           const asset <Major, Hardfork, Release> &in,
                                           const asset <Major, Hardfork, Release> &out) : owner(o), requestid(id),
                    amount_in(in), amount_out(out) {
            }

            account_name_type owner;
            uint32_t requestid = 0;
            asset <Major, Hardfork, Release> amount_in;
            asset <Major, Hardfork, Release> amount_out;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct fill_vesting_withdraw_operation
                : public virtual_operation<Major, Hardfork, Release> {
            fill_vesting_withdraw_operation() {
            }

            fill_vesting_withdraw_operation(const string &f, const string &t, const asset <Major, Hardfork, Release> &w,
                                            const asset <Major, Hardfork, Release> &d) : from_account(f), to_account(t),
                    withdrawn(w), deposited(d) {
            }

            account_name_type from_account;
            account_name_type to_account;
            asset <Major, Hardfork, Release> withdrawn;
            asset <Major, Hardfork, Release> deposited;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct fill_transfer_from_savings_operation
                : public virtual_operation<Major, Hardfork, Release> {
            fill_transfer_from_savings_operation() {
            }

            fill_transfer_from_savings_operation(const account_name_type &f, const account_name_type &t,
                                                 const asset <Major, Hardfork, Release> &a, const uint32_t r,
                                                 const string &m) : from(f), to(t), amount(a), request_id(r), memo(m) {
            }

            account_name_type from;
            account_name_type to;
            asset <Major, Hardfork, Release> amount;
            uint32_t request_id = 0;
            string memo;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct hardfork_operation : public virtual_operation<Major, Hardfork, Release> {
            hardfork_operation() {
            }

            hardfork_operation(uint32_t hf_id) : hardfork_id(hf_id) {
            }

            uint32_t hardfork_id = 0;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct comment_payout_update_operation
                : public virtual_operation<Major, Hardfork, Release> {
            comment_payout_update_operation() {
            }

            comment_payout_update_operation(const account_name_type &a, const string &p) : author(a), permlink(p) {
            }

            account_name_type author;
            string permlink;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct comment_benefactor_reward_operation
                : public virtual_operation<Major, Hardfork, Release> {
            comment_benefactor_reward_operation() {
            }

            comment_benefactor_reward_operation(const account_name_type &b, const account_name_type &a, const string &p,
                                                const asset <Major, Hardfork, Release> &r) : benefactor(b), author(a),
                    permlink(p), reward(r) {
            }

            account_name_type benefactor;
            account_name_type author;
            string permlink;
            asset <Major, Hardfork, Release> reward;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct return_vesting_delegation_operation
                : public virtual_operation<Major, Hardfork, Release> {
            return_vesting_delegation_operation() {
            }

            return_vesting_delegation_operation(const account_name_type &a, const asset <Major, Hardfork, Release> &v)
                    : account(a), vesting_shares(v) {
            }

            account_name_type account;
            asset <Major, Hardfork, Release> vesting_shares;
        };
    }
} //golos::protocol

FC_REFLECT((golos::protocol::author_reward_operation<0, 16, 0>), (author)(permlink)(sbd_payout)(steem_payout)(vesting_payout))
FC_REFLECT((golos::protocol::author_reward_operation<0, 17, 0>), (author)(permlink)(sbd_payout)(steem_payout)(vesting_payout))

FC_REFLECT((golos::protocol::curation_reward_operation<0, 16, 0>), (curator)(reward)(comment_author)(comment_permlink))
FC_REFLECT((golos::protocol::curation_reward_operation<0, 17, 0>), (curator)(reward)(comment_author)(comment_permlink))

FC_REFLECT((golos::protocol::comment_reward_operation<0, 16, 0>), (author)(permlink)(payout))
FC_REFLECT((golos::protocol::comment_reward_operation<0, 17, 0>), (author)(permlink)(payout))

FC_REFLECT((golos::protocol::fill_convert_request_operation<0, 16, 0>), (owner)(requestid)(amount_in)(amount_out))
FC_REFLECT((golos::protocol::fill_convert_request_operation<0, 17, 0>), (owner)(requestid)(amount_in)(amount_out))

FC_REFLECT((golos::protocol::liquidity_reward_operation<0, 16, 0>), (owner)(payout))
FC_REFLECT((golos::protocol::liquidity_reward_operation<0, 17, 0>), (owner)(payout))

FC_REFLECT((golos::protocol::interest_operation<0, 16, 0>), (owner)(interest))
FC_REFLECT((golos::protocol::interest_operation<0, 17, 0>), (owner)(interest))

FC_REFLECT((golos::protocol::fill_vesting_withdraw_operation<0, 16, 0>), (from_account)(to_account)(withdrawn)(deposited))
FC_REFLECT((golos::protocol::fill_vesting_withdraw_operation<0, 17, 0>), (from_account)(to_account)(withdrawn)(deposited))

FC_REFLECT((golos::protocol::fill_transfer_from_savings_operation<0, 16, 0>), (from)(to)(amount)(request_id)(memo))
FC_REFLECT((golos::protocol::fill_transfer_from_savings_operation<0, 17, 0>), (from)(to)(amount)(request_id)(memo))

FC_REFLECT((golos::protocol::hardfork_operation<0, 16, 0>), (hardfork_id))
FC_REFLECT((golos::protocol::hardfork_operation<0, 17, 0>), (hardfork_id))

FC_REFLECT((golos::protocol::comment_payout_update_operation<0, 16, 0>), (author)(permlink))
FC_REFLECT((golos::protocol::comment_payout_update_operation<0, 17, 0>), (author)(permlink))

FC_REFLECT((golos::protocol::comment_benefactor_reward_operation<0, 16, 0>), (benefactor)(author)(permlink)(reward))
FC_REFLECT((golos::protocol::comment_benefactor_reward_operation<0, 17, 0>), (benefactor)(author)(permlink)(reward))

FC_REFLECT((golos::protocol::return_vesting_delegation_operation<0, 16, 0>), (account)(vesting_shares))
FC_REFLECT((golos::protocol::return_vesting_delegation_operation<0, 17, 0>), (account)(vesting_shares))