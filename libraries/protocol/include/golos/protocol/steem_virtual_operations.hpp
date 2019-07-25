#pragma once

#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>
#include <golos/protocol/asset.hpp>

#include <fc/utf8.hpp>

namespace golos { namespace protocol {

        struct author_reward_operation : public virtual_operation {
            author_reward_operation() {
            }

            author_reward_operation(const account_name_type &a, const string &p, const asset &s, const asset &st, const asset &v, const asset &sst, const asset &vg)
                    : author(a), permlink(p), sbd_payout(s), steem_payout(st),
                      vesting_payout(v), sbd_and_steem_in_golos(sst), vesting_payout_in_golos(vg) {
            }

            account_name_type author;
            string permlink;
            asset sbd_payout;
            asset steem_payout;
            asset vesting_payout;
            asset sbd_and_steem_in_golos; // not reflected
            asset vesting_payout_in_golos; // not reflected
        };


        struct curation_reward_operation : public virtual_operation {
            curation_reward_operation() {
            }

            curation_reward_operation(const string &c, const asset &r, const string &a, const string &p, const asset &rg)
                    : curator(c), reward(r), comment_author(a),
                      comment_permlink(p), reward_in_golos(rg) {
            }

            account_name_type curator;
            asset reward;
            account_name_type comment_author;
            string comment_permlink;
            asset reward_in_golos; // not reflected
        };

        struct auction_window_reward_operation : public virtual_operation {
            auction_window_reward_operation() {
            }

            auction_window_reward_operation(const asset &r, const string &a, const string &p)
                    : reward(r), comment_author(a), comment_permlink(p) {
            }

            asset reward;
            account_name_type comment_author;
            string comment_permlink;
        };


        struct comment_reward_operation : public virtual_operation {
            comment_reward_operation() {
            }

            comment_reward_operation(const account_name_type &a, const string &pl, const asset &p)
                    : author(a), permlink(pl), payout(p) {
            }

            account_name_type author;
            string permlink;
            asset payout;
        };


        struct liquidity_reward_operation : public virtual_operation {
            liquidity_reward_operation(string o = string(), asset p = asset())
                    : owner(o), payout(p) {
            }

            account_name_type owner;
            asset payout;
        };


        struct interest_operation : public virtual_operation {
            interest_operation(const string &o = "", const asset &i = asset(0, SBD_SYMBOL))
                    : owner(o), interest(i) {
            }

            account_name_type owner;
            asset interest;
        };


        struct fill_convert_request_operation : public virtual_operation {
            fill_convert_request_operation() {
            }

            fill_convert_request_operation(const string &o, const uint32_t id, const asset &in, const asset &out)
                    : owner(o), requestid(id), amount_in(in), amount_out(out) {
            }

            account_name_type owner;
            uint32_t requestid = 0;
            asset amount_in;
            asset amount_out;
        };


        struct fill_vesting_withdraw_operation : public virtual_operation {
            fill_vesting_withdraw_operation() {
            }

            fill_vesting_withdraw_operation(const string &f, const string &t, const asset &w, const asset &d)
                    : from_account(f), to_account(t), withdrawn(w),
                      deposited(d) {
            }

            account_name_type from_account;
            account_name_type to_account;
            asset withdrawn;
            asset deposited;
        };


        struct shutdown_witness_operation : public virtual_operation {
            shutdown_witness_operation() {
            }

            shutdown_witness_operation(const string &o) : owner(o) {
            }

            account_name_type owner;
        };


        struct fill_order_operation : public virtual_operation {
            fill_order_operation() {
            }

            fill_order_operation(const string &c_o, uint32_t c_id, const asset &c_p, const string &o_o, uint32_t o_id, const asset &o_p)
                    : current_owner(c_o), current_orderid(c_id),
                      current_pays(c_p), open_owner(o_o), open_orderid(o_id),
                      open_pays(o_p) {
            }

            account_name_type current_owner;
            uint32_t current_orderid = 0;
            asset current_pays;
            account_name_type open_owner;
            uint32_t open_orderid = 0;
            asset open_pays;
        };


        struct fill_transfer_from_savings_operation : public virtual_operation {
            fill_transfer_from_savings_operation() {
            }

            fill_transfer_from_savings_operation(const account_name_type &f, const account_name_type &t, const asset &a, const uint32_t r, const string &m)
                    : from(f), to(t), amount(a), request_id(r), memo(m) {
            }

            account_name_type from;
            account_name_type to;
            asset amount;
            uint32_t request_id = 0;
            string memo;
        };

        struct hardfork_operation : public virtual_operation {
            hardfork_operation() {
            }

            hardfork_operation(uint32_t hf_id) : hardfork_id(hf_id) {
            }

            uint32_t hardfork_id = 0;
        };

        struct comment_payout_update_operation : public virtual_operation {
            comment_payout_update_operation() {
            }

            comment_payout_update_operation(const account_name_type &a, const string &p)
                    : author(a), permlink(p) {
            }

            account_name_type author;
            string permlink;
        };

        struct comment_benefactor_reward_operation : public virtual_operation {
            comment_benefactor_reward_operation() {
            }

            comment_benefactor_reward_operation(const account_name_type &b, const account_name_type &a, const string &p, const asset &r, const asset& rig)
                    : benefactor(b), author(a), permlink(p), reward(r), reward_in_golos(rig) {
            }

            account_name_type benefactor;
            account_name_type author;
            string permlink;
            asset reward;
            asset reward_in_golos; // not reflected
        };

        struct producer_reward_operation : public virtual_operation {
            producer_reward_operation() {}
            producer_reward_operation(const string& p, const asset& v) : producer(p), vesting_shares(v) {}

            account_name_type producer;
            asset             vesting_shares;
        };

        struct delegation_reward_operation : public virtual_operation {
            delegation_reward_operation() {
            }

            delegation_reward_operation(const account_name_type& dr, const account_name_type& de, const delegator_payout_strategy& ps, const asset& vs, const asset& vsg)
                    : delegator(dr), delegatee(de), payout_strategy(ps), vesting_shares(vs), vesting_shares_in_golos(vsg) {
            }

            account_name_type delegator;
            account_name_type delegatee;
            delegator_payout_strategy payout_strategy;
            asset vesting_shares;
            asset vesting_shares_in_golos; // not reflected
        };

        struct techspec_reward_operation : public virtual_operation {
            techspec_reward_operation() {
            }
            techspec_reward_operation(const account_name_type& a, const string& p, const asset& r)
                    : author(a), permlink(p), reward(r) {
            }

            account_name_type author;
            string permlink;
            asset reward;
        };

        struct worker_reward_operation : public virtual_operation {
            worker_reward_operation() {
            }
            worker_reward_operation(const account_name_type& w, const account_name_type& wta, const string& wtp, const asset& r)
                    : worker(w), worker_techspec_author(wta), worker_techspec_permlink(wtp), reward(r) {
            }

            account_name_type worker;
            account_name_type worker_techspec_author;
            string worker_techspec_permlink;
            asset reward;
        };

        struct return_vesting_delegation_operation: public virtual_operation {
            return_vesting_delegation_operation() {
            }
            return_vesting_delegation_operation(const account_name_type& a, const asset& v)
            :   account(a), vesting_shares(v) {
            }

            account_name_type account;
            asset vesting_shares;
        };

        struct total_comment_reward_operation : public virtual_operation {
            total_comment_reward_operation() {
            }

            total_comment_reward_operation(const account_name_type& a, const string &p, const asset& ar, const asset& br, const asset& cr, int64_t nr)
                    : author(a), permlink(p), author_reward(ar), benefactor_reward(br), curator_reward(cr), net_rshares(nr) {
            }

            account_name_type author;
            string permlink;
            asset author_reward;
            asset benefactor_reward;
            asset curator_reward;
            int64_t net_rshares;
        };

        struct techspec_expired_operation : public virtual_operation {
            techspec_expired_operation() {
            }
            techspec_expired_operation(const account_name_type& a, const string& p, bool w)
                    : author(a), permlink(p), was_approved(w) {
            }

            account_name_type author;
            string permlink;
            bool was_approved;
        };
} } //golos::protocol

FC_REFLECT((golos::protocol::author_reward_operation), (author)(permlink)(sbd_payout)(steem_payout)(vesting_payout))
FC_REFLECT((golos::protocol::curation_reward_operation), (curator)(reward)(comment_author)(comment_permlink))
FC_REFLECT((golos::protocol::auction_window_reward_operation), (reward)(comment_author)(comment_permlink))
FC_REFLECT((golos::protocol::comment_reward_operation), (author)(permlink)(payout))
FC_REFLECT((golos::protocol::fill_convert_request_operation), (owner)(requestid)(amount_in)(amount_out))
FC_REFLECT((golos::protocol::liquidity_reward_operation), (owner)(payout))
FC_REFLECT((golos::protocol::interest_operation), (owner)(interest))
FC_REFLECT((golos::protocol::fill_vesting_withdraw_operation), (from_account)(to_account)(withdrawn)(deposited))
FC_REFLECT((golos::protocol::shutdown_witness_operation), (owner))
FC_REFLECT((golos::protocol::fill_order_operation), (current_owner)(current_orderid)(current_pays)(open_owner)(open_orderid)(open_pays))
FC_REFLECT((golos::protocol::fill_transfer_from_savings_operation), (from)(to)(amount)(request_id)(memo))
FC_REFLECT((golos::protocol::hardfork_operation), (hardfork_id))
FC_REFLECT((golos::protocol::comment_payout_update_operation), (author)(permlink))
FC_REFLECT((golos::protocol::comment_benefactor_reward_operation), (benefactor)(author)(permlink)(reward))
FC_REFLECT((golos::protocol::return_vesting_delegation_operation), (account)(vesting_shares))
FC_REFLECT((golos::protocol::producer_reward_operation), (producer)(vesting_shares))
FC_REFLECT((golos::protocol::delegation_reward_operation), (delegator)(delegatee)(payout_strategy)(vesting_shares))
FC_REFLECT((golos::protocol::total_comment_reward_operation), (author)(permlink)(author_reward)(benefactor_reward)(curator_reward)(net_rshares))
FC_REFLECT((golos::protocol::techspec_reward_operation), (author)(permlink)(reward))
FC_REFLECT((golos::protocol::worker_reward_operation), (worker)(worker_techspec_author)(worker_techspec_permlink)(reward))
FC_REFLECT((golos::protocol::techspec_expired_operation), (author)(permlink)(was_approved))
