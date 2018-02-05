#include <golos/chain/evaluators/proposal_evaluator.hpp>
#include <golos/chain/objects/proposal_object.hpp>
#include <golos/chain/objects/account_object.hpp>

#include <fc/exception/exception.hpp>
#include <fc/smart_ref_impl.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_create_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                FC_ASSERT(o.expiration_time >
                          this->db.template head_block_time(), "Proposal has already expired on creation.");
                FC_ASSERT(o.expiration_time <= this->db.template head_block_time() +
                                               STEEMIT_MAX_PROPOSAL_LIFETIME_SEC,
                        "Proposal expiration time is too far in the future.");
                FC_ASSERT(!o.review_period_seconds ||
                          fc::seconds(*o.review_period_seconds) <
                          (o.expiration_time - this->db.template head_block_time()),
                        "Proposal review period must be less than its overall lifetime.");

                {
                    // If we're dealing with the committee authority, make sure this transaction has a sufficient review period.
                    flat_set<account_name_type> auths;
                    vector<authority> other;
                    for (const operation_wrapper &op : o.proposed_operations) {
                        operation_get_required_authorities(op.op, auths, auths, auths, other);
                    }

                    FC_ASSERT(other.size() == 0); // TODO: what about other???

                    if (auths.find(STEEMIT_COMMITTEE_ACCOUNT) != auths.end()) {
//                        STEEMIT_ASSERT(
//                                o.review_period_seconds.valid(),
//                                proposal_create_review_period_required,
//                                "Review period not given, but at least ${min} required",
//                                ("min", global_parameters.committee_proposal_review_period)
//                        );
//                        STEEMIT_ASSERT(
//                                *o.review_period_seconds >=
//                                global_parameters.committee_proposal_review_period,
//                                proposal_create_review_period_insufficient,
//                                "Review period of ${t} specified, but at least ${min} required",
//                                ("t", *o.review_period_seconds)
//                                        ("min", global_parameters.committee_proposal_review_period)
//                        );
                    }
                }

                for (const operation_wrapper &op : o.proposed_operations) {
                    _proposed_trx.operations.push_back(op.op);
                }
                _proposed_trx.validate();

                this->db.template create<proposal_object>([&](proposal_object &proposal) {
                    _proposed_trx.expiration = o.expiration_time;
                    proposal.proposed_transaction = _proposed_trx;
                    proposal.expiration_time = o.expiration_time;
                    proposal.proposal_id = o.proposal_id;
                    if (o.review_period_seconds) {
                        proposal.review_period_time = o.expiration_time - *o.review_period_seconds;
                    }

                    //Populate the required approval sets
                    flat_set<account_name_type> required_active;
                    flat_set<account_name_type> required_posting;
                    vector<authority> other;

                    // TODO: consider caching values from evaluate?
                    for (auto &op : _proposed_trx.operations) {
                        operation_get_required_authorities(op, required_active, proposal.required_owner_approvals, required_posting, other);
                    }

                    //All accounts which must provide both owner and active authority should be omitted from the active authority set;
                    //owner authority approval implies active authority approval.
                    std::set_difference(required_active.begin(), required_active.end(),
                            proposal.required_owner_approvals.begin(), proposal.required_owner_approvals.end(),
                            std::inserter(proposal.required_active_approvals, proposal.required_active_approvals.begin()));
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_update_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                _proposal = this->db.template find_proposal(o.owner, o.proposal_id);

                if (_proposal->review_period_time &&
                    this->db.template head_block_time() >= *_proposal->review_period_time)
                    FC_ASSERT(o.active_approvals_to_add.empty() &&
                              o.owner_approvals_to_add.empty(),
                            "This proposal is in its review period. No new approvals may be added.");

                for (account_name_type id : o.active_approvals_to_remove) {
                    FC_ASSERT(_proposal->available_active_approvals.find(id) !=
                              _proposal->available_active_approvals.end(),
                            "", ("id", id)("available", _proposal->available_active_approvals));
                }
                for (account_name_type id : o.owner_approvals_to_remove) {
                    FC_ASSERT(_proposal->available_owner_approvals.find(id) !=
                              _proposal->available_owner_approvals.end(),
                            "", ("id", id)("available", _proposal->available_owner_approvals));
                }

                // Potential optimization: if executed_proposal is true, we can skip the modify step and make push_proposal skip
                // signature checks. This isn't done now because I just wrote all the proposals code, and I'm not yet 100% sure the
                // required approvals are sufficient to authorize the transaction.
                this->db.template modify(*_proposal, [&](proposal_object &p) {
                    p.available_active_approvals.insert(o.active_approvals_to_add.begin(), o.active_approvals_to_add.end());
                    p.available_owner_approvals.insert(o.owner_approvals_to_add.begin(), o.owner_approvals_to_add.end());
                    for (account_name_type id : o.active_approvals_to_remove) {
                        p.available_active_approvals.erase(id);
                    }
                    for (account_name_type id : o.owner_approvals_to_remove) {
                        p.available_owner_approvals.erase(id);
                    }
                    for (const auto &id : o.key_approvals_to_add) {
                        p.available_key_approvals.insert(id);
                    }
                    for (const auto &id : o.key_approvals_to_remove) {
                        p.available_key_approvals.erase(id);
                    }
                });

                // If the proposal has a review period, don't bother attempting to authorize/execute it.
                // Proposals with a review period may never be executed except at their expiration.
                if (_proposal->review_period_time) {
                    return;
                }

                if (_proposal->is_authorized_to_execute(this->db)) {
                    // All required approvals are satisfied. Execute!
                    executed_proposal = true;
                    try {
                        this->db.template push_proposal(*_proposal);
                    } catch (fc::exception &e) {
                        wlog("Proposed transaction ${id} failed to apply once approved with exception:\n----\n${reason}\n----\nWill try again when it expires.",
                                ("id", o.proposal_id)("reason", e.to_detail_string()));
                        proposal_failed = true;
                    }
                }
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_delete_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                _proposal = this->db.template find_proposal(o.owner, o.proposal_id);

                auto required_approvals = o.using_owner_authority
                                          ? _proposal->required_owner_approvals
                                          : _proposal->required_active_approvals;
                FC_ASSERT(required_approvals.find(o.owner) !=
                          required_approvals.end(),
                        "Provided authority is not authoritative for this proposal.",
                        ("provided", o.owner)("required", required_approvals));

                        this->db.template remove(*_proposal);

            } FC_CAPTURE_AND_RETHROW((o))
        }
    }
} // golos::chain

#include <golos/chain/evaluators/proposal_evaluator.tpp>