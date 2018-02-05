#include <golos/chain/evaluators/witness_evaluator.hpp>

#include <golos/chain/objects/witness_object.hpp>

#include <golos/chain/database.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void witness_update_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            this->db.get_account(o.owner); // verify owner exists

            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                FC_ASSERT(o.url.size() <= STEEMIT_MAX_WITNESS_URL_LENGTH, "URL is too long");
            } else if (o.url.size() > STEEMIT_MAX_WITNESS_URL_LENGTH) {
                // after HF, above check can be moved to validate() if reindex doesn't show this warning
                wlog("URL is too long in block ${b}", ("b", this->db.head_block_num() + 1));
            }

            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_14__410)) {
                FC_ASSERT(o.props.account_creation_fee.symbol_name() == STEEM_SYMBOL_NAME);
            } else if (o.props.account_creation_fee.symbol_name() != STEEM_SYMBOL_NAME) {
                // after HF, above check can be moved to validate() if reindex doesn't show this warning
                wlog("Wrong fee symbol in block ${b}", ("b", this->db.head_block_num() + 1));
            }

            const auto &by_witness_name_idx = this->db.template get_index<witness_index>().indices().
                    template get<by_name>();
            auto wit_itr = by_witness_name_idx.find(o.owner);
            if (wit_itr != by_witness_name_idx.end()) {
                this->db.modify(*wit_itr, [&](witness_object &w) {
                    from_string(w.url, o.url);
                    w.signing_key = o.block_signing_key;
                    w.props = {protocol::asset<0, 17, 0>(o.props.account_creation_fee.amount,
                                               o.props.account_creation_fee.symbol_name()), o.props.maximum_block_size,
                               o.props.sbd_interest_rate};
                });
            } else {
                this->db.template create<witness_object>([&](witness_object &w) {
                    w.owner = o.owner;
                    from_string(w.url, o.url);
                    w.signing_key = o.block_signing_key;
                    w.created = this->db.head_block_time();
                    w.props = {protocol::asset<0, 17, 0>(o.props.account_creation_fee.amount,
                                               o.props.account_creation_fee.symbol_name()), o.props.maximum_block_size,
                               o.props.sbd_interest_rate};
                });
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void account_witness_proxy_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            const auto &account = this->db.get_account(o.account);
            FC_ASSERT(account.proxy != o.proxy, "Proxy must change.");

            FC_ASSERT(account.can_vote, "Account has declined the ability to vote and cannot proxy votes.");

            /// remove all current votes
            std::array < share_type, STEEMIT_MAX_PROXY_RECURSION_DEPTH + 1 > delta;
            delta[0] = -account.vesting_shares.amount;
            for (int i = 0; i < STEEMIT_MAX_PROXY_RECURSION_DEPTH; ++i) {
                delta[i + 1] = -account.proxied_vsf_votes[i];
            }
            this->db.adjust_proxied_witness_votes(account, delta);

            if (o.proxy.size()) {
                const auto &new_proxy = this->db.get_account(o.proxy);
                flat_set <account_object::id_type> proxy_chain({account.id, new_proxy.id});
                proxy_chain.reserve(STEEMIT_MAX_PROXY_RECURSION_DEPTH + 1);

                /// check for proxy loops and fail to update the proxy if it would create a loop
                auto cprox = &new_proxy;
                while (cprox->proxy.size() != 0) {
                    const auto next_proxy = this->db.get_account(cprox->proxy);
                    FC_ASSERT(proxy_chain.insert(next_proxy.id).second, "This proxy would create a proxy loop.");
                    cprox = &next_proxy;
                    FC_ASSERT(proxy_chain.size() <= STEEMIT_MAX_PROXY_RECURSION_DEPTH, "Proxy chain is too long.");
                }

                /// clear all individual vote records
                this->db.clear_witness_votes(account);

                this->db.modify(account, [&](account_object &a) {
                    a.proxy = o.proxy;
                });

                /// add all new votes
                for (int i = 0; i <= STEEMIT_MAX_PROXY_RECURSION_DEPTH; ++i) {
                    delta[i] = -delta[i];
                }
                this->db.adjust_proxied_witness_votes(account, delta);
            } else { /// we are clearing the proxy which means we simply update the account
                this->db.modify(account, [&](account_object &a) {
                    a.proxy = o.proxy;
                });
            }
        }


        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void account_witness_vote_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            const auto &voter = this->db.get_account(o.account);
            FC_ASSERT(voter.proxy.size() == 0,
                      "A proxy is currently set, please clear the proxy before voting for a witness.");

            if (o.approve) {
                FC_ASSERT(voter.can_vote, "Account has declined its voting rights.");
            }

            const auto &witness = this->db.get_witness(o.witness);

            const auto &by_account_witness_idx = this->db.template get_index<witness_vote_index>().indices().
                    template get<by_account_witness>();
            auto itr = by_account_witness_idx.find(boost::make_tuple(voter.name, witness.owner));

            if (itr == by_account_witness_idx.end()) {
                FC_ASSERT(o.approve, "Vote doesn't exist, user must indicate a desire to approve witness.");

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_2)) {
                    FC_ASSERT(voter.witnesses_voted_for < STEEMIT_MAX_ACCOUNT_WITNESS_VOTES,
                              "Account has voted for too many witnesses."); // TODO: Remove after hardfork 2

                    this->db.template create<witness_vote_object>([&](witness_vote_object &v) {
                        v.witness = witness.owner;
                        v.account = voter.name;
                        v.created = this->db.head_block_time();
                    });

                    if (this->db.has_hardfork(STEEMIT_HARDFORK_0_3)) {
                        this->db.adjust_witness_vote(witness, voter.witness_vote_weight());
                    } else {
                        this->db.adjust_proxied_witness_votes(voter, voter.witness_vote_weight());
                    }

                } else {

                    this->db.template create<witness_vote_object>([&](witness_vote_object &v) {
                        v.witness = witness.owner;
                        v.account = voter.name;
                        v.created = this->db.head_block_time();
                    });
                    this->db.modify(witness, [&](witness_object &w) {
                        w.votes += voter.witness_vote_weight();
                    });

                }
                this->db.modify(voter, [&](account_object &a) {
                    a.witnesses_voted_for++;
                });

            } else {
                FC_ASSERT(!o.approve, "Vote currently exists, user must indicate a desire to reject witness.");

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_2)) {
                    if (this->db.has_hardfork(STEEMIT_HARDFORK_0_3)) {
                        this->db.adjust_witness_vote(witness, -voter.witness_vote_weight());
                    } else {
                        this->db.adjust_proxied_witness_votes(voter, -voter.witness_vote_weight());
                    }
                } else {
                    this->db.modify(witness, [&](witness_object &w) {
                        w.votes -= voter.witness_vote_weight();
                    });
                }
                this->db.modify(voter, [&](account_object &a) {
                    a.witnesses_voted_for--;
                });
                this->db.remove(*itr);
            }
        }
    }
}

#include <golos/chain/evaluators/witness_evaluator.tpp>