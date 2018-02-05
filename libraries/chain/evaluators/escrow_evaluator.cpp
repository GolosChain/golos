#include <golos/chain/database.hpp>

#include <golos/chain/evaluators/escrow_evaluator.hpp>
#include <golos/chain/objects/steem_objects.hpp>

#include <golos/protocol/asset.hpp>

namespace golos {
    namespace chain {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_transfer_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                const auto &from_account = this->db.template get_account(o.from);
                this->db.template get_account(o.to);
                this->db.template get_account(o.agent);

                FC_ASSERT(o.ratification_deadline > this->db.template head_block_time(),
                          "The escrow ratification deadline must be after head block time.");
                FC_ASSERT(o.escrow_expiration > this->db.template head_block_time(),
                          "The escrow expiration must be after head block time.");

                protocol::asset<0, 17, 0> steem_spent = protocol::asset<0, 17, 0>(o.steem_amount.amount, o.steem_amount.symbol_name());
                protocol::asset<0, 17, 0> sbd_spent = protocol::asset<0, 17, 0>(o.sbd_amount.amount, o.sbd_amount.symbol_name());
                if (o.fee.symbol_name() == STEEM_SYMBOL_NAME) {
                    steem_spent += protocol::asset<0, 17, 0>(o.fee.amount, o.fee.symbol_name());
                } else {
                    sbd_spent += protocol::asset<0, 17, 0>(o.fee.amount, o.fee.symbol_name());
                }

                FC_ASSERT(this->db.template get_balance(from_account.name, STEEM_SYMBOL_NAME) >= steem_spent,
                          "Account cannot cover STEEM costs of escrow. Required: ${r} Available: ${a}",
                          ("r", steem_spent)("a", this->db.template get_balance(from_account.name, STEEM_SYMBOL_NAME)));

                FC_ASSERT(this->db.template get_balance(from_account.name, SBD_SYMBOL_NAME) >= sbd_spent,
                          "Account cannot cover SBD costs of escrow. Required: ${r} Available: ${a}",
                          ("r", sbd_spent)("a", this->db.template get_balance(from_account.name, SBD_SYMBOL_NAME)));

                this->db.template adjust_balance(from_account, -steem_spent);
                this->db.template adjust_balance(from_account, -sbd_spent);

                this->db.template create<escrow_object>([&](escrow_object &esc) {
                    esc.escrow_id = o.escrow_id;
                    esc.from = o.from;
                    esc.to = o.to;
                    esc.agent = o.agent;
                    esc.ratification_deadline = o.ratification_deadline;
                    esc.escrow_expiration = o.escrow_expiration;
                    esc.sbd_balance = protocol::asset<0, 17, 0>(o.sbd_amount.amount, o.sbd_amount.symbol_name());
                    esc.steem_balance = protocol::asset<0, 17, 0>(o.steem_amount.amount, o.steem_amount.symbol_name());
                    esc.pending_fee = protocol::asset<0, 17, 0>(o.fee.amount, o.fee.symbol_name());
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_approve_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {

                const auto &escrow = this->db.template get_escrow(o.from, o.escrow_id);

                FC_ASSERT(escrow.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).",
                          ("o", o.to)("e", escrow.to));
                FC_ASSERT(escrow.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).",
                          ("o", o.agent)("e", escrow.agent));
                FC_ASSERT(escrow.ratification_deadline >= this->db.template head_block_time(),
                          "The escrow ratification deadline has passed. Escrow can no longer be ratified.");

                bool reject_escrow = !o.approve;

                if (o.who == o.to) {
                    FC_ASSERT(!escrow.to_approved, "Account 'to' (${t}) has already approved the escrow.", ("t", o.to));

                    if (!reject_escrow) {
                        this->db.template modify(escrow, [&](escrow_object &esc) {
                            esc.to_approved = true;
                        });
                    }
                }
                if (o.who == o.agent) {
                    FC_ASSERT(!escrow.agent_approved, "Account 'agent' (${a}) has already approved the escrow.",
                              ("a", o.agent));

                    if (!reject_escrow) {
                        this->db.template modify(escrow, [&](escrow_object &esc) {
                            esc.agent_approved = true;
                        });
                    }
                }

                if (reject_escrow) {
                    const auto &from_account = this->db.template get_account(o.from);
                    this->db.template adjust_balance(from_account, escrow.steem_balance);
                    this->db.template adjust_balance(from_account, escrow.sbd_balance);
                    this->db.template adjust_balance(from_account, escrow.pending_fee);

                    this->db.template remove(escrow);
                } else if (escrow.to_approved && escrow.agent_approved) {
                    const auto &agent_account = this->db.template get_account(o.agent);
                    this->db.template adjust_balance(agent_account, escrow.pending_fee);

                    this->db.template modify(escrow, [&](escrow_object &esc) {
                        esc.pending_fee.amount = 0;
                    });
                }
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_dispute_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {

                this->db.template get_account(o.from); // Verify from account exists

                const auto &e = this->db.template get_escrow(o.from, o.escrow_id);
                FC_ASSERT(this->db.template head_block_time() < e.escrow_expiration,
                          "Disputing the escrow must happen before expiration.");
                FC_ASSERT(e.to_approved && e.agent_approved,
                          "The escrow must be approved by all parties before a dispute can be raised.");
                FC_ASSERT(!e.disputed, "The escrow is already under dispute.");
                FC_ASSERT(e.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).",
                          ("o", o.to)("e", e.to));
                FC_ASSERT(e.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).",
                          ("o", o.agent)("e", e.agent));

                this->db.template modify(e, [&](escrow_object &esc) {
                    esc.disputed = true;
                });
            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void escrow_release_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {

                this->db.template get_account(o.from); // Verify from account exists
                const auto &receiver_account = this->db.template get_account(o.receiver);

                const auto &e = this->db.template get_escrow(o.from, o.escrow_id);
                FC_ASSERT(e.steem_balance >= typename BOOST_IDENTITY_TYPE((protocol::asset<0, 17, 0>))(o.steem_amount.amount, o.steem_amount.symbol_name()),
                          "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}",
                          ("a", o.steem_amount)("b", e.steem_balance));
                FC_ASSERT(e.sbd_balance >= typename BOOST_IDENTITY_TYPE((protocol::asset<0, 17, 0>))(o.sbd_amount.amount, o.sbd_amount.symbol_name()),
                          "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}",
                          ("a", o.sbd_amount)("b", e.sbd_balance));
                FC_ASSERT(e.to == o.to, "Operation 'to' (${o}) does not match escrow 'to' (${e}).",
                          ("o", o.to)("e", e.to));
                FC_ASSERT(e.agent == o.agent, "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).",
                          ("o", o.agent)("e", e.agent));
                FC_ASSERT(o.receiver == e.from || o.receiver == e.to,
                          "Funds must be released to 'from' (${f}) or 'to' (${t})", ("f", e.from)("t", e.to));
                FC_ASSERT(e.to_approved && e.agent_approved, "Funds cannot be released prior to escrow approval.");

                // If there is a dispute regardless of expiration, the agent can release funds to either party
                if (e.disputed) {
                    FC_ASSERT(o.who == e.agent, "Only 'agent' (${a}) can release funds in a disputed escrow.",
                              ("a", e.agent));
                } else {
                    FC_ASSERT(o.who == e.from || o.who == e.to,
                              "Only 'from' (${f}) and 'to' (${t}) can release funds from a non-disputed escrow",
                              ("f", e.from)("t", e.to));

                    if (e.escrow_expiration > this->db.template head_block_time()) {
                        // If there is no dispute and escrow has not expired, either party can release funds to the other.
                        if (o.who == e.from) {
                            FC_ASSERT(o.receiver == e.to, "Only 'from' (${f}) can release funds to 'to' (${t}).",
                                      ("f", e.from)("t", e.to));
                        } else if (o.who == e.to) {
                            FC_ASSERT(o.receiver == e.from, "Only 'to' (${t}) can release funds to 'from' (${t}).",
                                      ("f", e.from)("t", e.to));
                        }
                    }
                }
                // If escrow expires and there is no dispute, either party can release funds to either party.

                this->db.template adjust_balance(receiver_account, protocol::asset<0, 17, 0>(o.steem_amount.amount, o.steem_amount.symbol_name()));
                this->db.template adjust_balance(receiver_account, protocol::asset<0, 17, 0>(o.sbd_amount.amount, o.sbd_amount.symbol_name()));

                this->db.template modify(e, [&](escrow_object &esc) {
                    esc.steem_balance -= protocol::asset<0, 17, 0>(o.steem_amount.amount, o.steem_amount.symbol_name());
                    esc.sbd_balance -= protocol::asset<0, 17, 0>(o.sbd_amount.amount, o.sbd_amount.symbol_name());
                });

                if (e.steem_balance.amount == 0 && e.sbd_balance.amount == 0) {
                    this->db.template remove(e);
                }
            } FC_CAPTURE_AND_RETHROW((o))
        }
    }
}

#include <golos/chain/evaluators/escrow_evaluator.tpp>