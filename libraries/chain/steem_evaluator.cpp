#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/block_summary_object.hpp>
#include <golos/chain/worker_objects.hpp>

#define GOLOS_CHECK_BANDWIDTH(NOW, NEXT, TYPE, MSG, ...) \
    GOLOS_ASSERT((NOW) > (NEXT), golos::bandwidth_exception, MSG, \
            ("bandwidth",TYPE)("now",NOW)("next",NEXT) __VA_ARGS__)


namespace golos { namespace chain {
        using fc::uint128_t;

        inline void validate_permlink_0_1(const string &permlink) {
            GOLOS_CHECK_VALUE(permlink.size() > STEEMIT_MIN_PERMLINK_LENGTH &&
                      permlink.size() < STEEMIT_MAX_PERMLINK_LENGTH, 
                      "Permlink is not a valid size. Permlink length should be more ${min} and less ${max}",
                      ("min", STEEMIT_MIN_PERMLINK_LENGTH)("max", STEEMIT_MAX_PERMLINK_LENGTH));

            for (auto c : permlink) {
                switch (c) {
                    case 'a':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                    case 'g':
                    case 'h':
                    case 'i':
                    case 'j':
                    case 'k':
                    case 'l':
                    case 'm':
                    case 'n':
                    case 'o':
                    case 'p':
                    case 'q':
                    case 'r':
                    case 's':
                    case 't':
                    case 'u':
                    case 'v':
                    case 'w':
                    case 'x':
                    case 'y':
                    case 'z':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case '-':
                        break;
                    default:
                        FC_THROW_EXCEPTION(invalid_value, "Invalid permlink character: ${s}",
                                ("s", std::string() + c));
                }
            }
        }

        struct strcmp_equal {
            bool operator()(const shared_string &a, const string &b) {
                return a.size() == b.size() ||
                       std::strcmp(a.c_str(), b.c_str()) == 0;
            }
        };

        void store_account_json_metadata(
            database& db, const account_name_type& account, const string& json_metadata, bool skip_empty = false
        ) {
            if (!db.store_metadata_for_account(account)) {
                auto meta = db.find<account_metadata_object, by_account>(account);
                if (meta != nullptr) {
                    db.remove(*meta);
                }
                return;
            }

            if (skip_empty && json_metadata.size() == 0)
                return;

            const auto& idx = db.get_index<account_metadata_index>().indices().get<by_account>();
            auto itr = idx.find(account);
            if (itr != idx.end()) {
                db.modify(*itr, [&](account_metadata_object& a) {
                    from_string(a.json_metadata, json_metadata);
                });
            } else {
                // Note: this branch should be executed only on account creation.
                db.create<account_metadata_object>([&](account_metadata_object& a) {
                    a.account = account;
                    from_string(a.json_metadata, json_metadata);
                });
            }
        }

        void account_create_evaluator::do_apply(const account_create_operation &o) {
            const auto& creator = _db.get_account(o.creator);

            GOLOS_CHECK_BALANCE(creator, MAIN_BALANCE, o.fee);

            if (_db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                const auto& median_props = _db.get_witness_schedule_object().median_props;
                auto min_fee = median_props.account_creation_fee;
                GOLOS_CHECK_OP_PARAM(o, fee,
                    GOLOS_CHECK_VALUE(o.fee >= min_fee,
                        "Insufficient Fee: ${f} required, ${p} provided.", ("f", min_fee)("p", o.fee));
                );
            }

            if (_db.is_producing() ||
                _db.has_hardfork(STEEMIT_HARDFORK_0_15__465)) {
                for (auto &a : o.owner.account_auths) {
                    _db.get_account(a.first);
                }

                for (auto &a : o.active.account_auths) {
                    _db.get_account(a.first);
                }

                for (auto &a : o.posting.account_auths) {
                    _db.get_account(a.first);
                }
            }

            _db.modify(creator, [&](account_object &c) {
                c.balance -= o.fee;
            });

            GOLOS_CHECK_OBJECT_MISSING(_db, account, o.new_account_name);

            const auto& props = _db.get_dynamic_global_properties();
            const auto& new_account = _db.create<account_object>([&](account_object& acc) {
                acc.name = o.new_account_name;
                acc.memo_key = o.memo_key;
                acc.created = props.time;
                acc.last_vote_time = props.time;
                acc.mined = false;

                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_11__169)) {
                    acc.recovery_account = STEEMIT_INIT_MINER_NAME;
                } else {
                    acc.recovery_account = o.creator;
                }
            });
            store_account_json_metadata(_db, o.new_account_name, o.json_metadata);

            _db.create<account_authority_object>([&](account_authority_object &auth) {
                auth.account = o.new_account_name;
                auth.owner = o.owner;
                auth.active = o.active;
                auth.posting = o.posting;
                auth.last_owner_update = fc::time_point_sec::min();
            });

            if (o.fee.amount > 0) {
                _db.create_vesting(new_account, o.fee);
            }
        }

        struct account_create_with_delegation_extension_visitor {
            account_create_with_delegation_extension_visitor(const account_object& a, database& db)
                    : _a(a), _db(db) {
            }

            using result_type = void;

            const account_object& _a;
            database& _db;

            void operator()(const account_referral_options& aro) const {
                ASSERT_REQ_HF(STEEMIT_HARDFORK_0_19__295, "account_referral_options");

                _db.get_account(aro.referrer);

                const auto& median_props = _db.get_witness_schedule_object().median_props;

                GOLOS_CHECK_LIMIT_PARAM(aro.interest_rate, median_props.max_referral_interest_rate);

                GOLOS_CHECK_PARAM(aro.end_date,
                    GOLOS_CHECK_VALUE(aro.end_date >= _db.head_block_time(), "End date should be in the future"));
                GOLOS_CHECK_LIMIT_PARAM(aro.end_date, _db.head_block_time() + median_props.max_referral_term_sec);

                GOLOS_CHECK_LIMIT_PARAM(aro.break_fee, median_props.max_referral_break_fee);

                _db.modify(_a, [&](account_object& a) {
                    a.referrer_account = aro.referrer;
                    a.referrer_interest_rate = aro.interest_rate;
                    a.referral_end_date = aro.end_date;
                    a.referral_break_fee = aro.break_fee;
                });
            }
        };

        void account_create_with_delegation_evaluator::do_apply(const account_create_with_delegation_operation& o) {
            const auto& creator = _db.get_account(o.creator);
            GOLOS_CHECK_BALANCE(creator, MAIN_BALANCE, o.fee);
            GOLOS_CHECK_BALANCE(creator, AVAILABLE_VESTING, o.delegation);

            const auto& v_share_price = _db.get_dynamic_global_properties().get_vesting_share_price();
            const auto& median_props = _db.get_witness_schedule_object().median_props;
            const auto target = median_props.create_account_min_golos_fee + median_props.create_account_min_delegation;
            auto target_delegation = target * v_share_price;
            auto min_fee = median_props.account_creation_fee.amount.value;
#ifdef STEEMIT_BUILD_TESTNET
            if (!min_fee)
                min_fee = 1;
#endif
            auto current_delegation = o.fee * target.amount.value / min_fee * v_share_price + o.delegation;

            GOLOS_CHECK_LOGIC(current_delegation >= target_delegation,
                logic_exception::not_enough_delegation,
                "Insufficient Delegation ${f} required, ${p} provided.",
                ("f", target_delegation)("p", current_delegation)("o.fee", o.fee) ("o.delegation", o.delegation));
            auto min_golos = median_props.create_account_min_golos_fee;
            GOLOS_CHECK_OP_PARAM(o, fee, {
                GOLOS_CHECK_VALUE(o.fee >= min_golos,
                    "Insufficient Fee: ${f} required, ${p} provided.", ("f", min_golos)("p", o.fee));
            });

            for (auto& a : o.owner.account_auths) {
                _db.get_account(a.first);
            }
            for (auto& a : o.active.account_auths) {
                _db.get_account(a.first);
            }
            for (auto& a : o.posting.account_auths) {
                _db.get_account(a.first);
            }

            const auto now = _db.head_block_time();

            _db.modify(creator, [&](account_object& c) {
                c.balance -= o.fee;
                c.delegated_vesting_shares += o.delegation;
            });
            const auto& new_account = _db.create<account_object>([&](account_object& acc) {
                acc.name = o.new_account_name;
                acc.memo_key = o.memo_key;
                acc.created = now;
                acc.last_vote_time = now;
                acc.mined = false;
                acc.recovery_account = o.creator;
                acc.received_vesting_shares = o.delegation;
            });
            store_account_json_metadata(_db, o.new_account_name, o.json_metadata);

            _db.create<account_authority_object>([&](account_authority_object& auth) {
                auth.account = o.new_account_name;
                auth.owner = o.owner;
                auth.active = o.active;
                auth.posting = o.posting;
                auth.last_owner_update = fc::time_point_sec::min();
            });
            if (o.delegation.amount > 0) {
                _db.create<vesting_delegation_object>([&](vesting_delegation_object& d) {
                    d.delegator = o.creator;
                    d.delegatee = o.new_account_name;
                    d.vesting_shares = o.delegation;
                    d.min_delegation_time = now + fc::seconds(median_props.create_account_delegation_time);
                });
            }
            if (o.fee.amount > 0) {
                _db.create_vesting(new_account, o.fee);
            }

            for (auto& e : o.extensions) {
                e.visit(account_create_with_delegation_extension_visitor(new_account, _db));
            }
        }

        void account_update_evaluator::do_apply(const account_update_operation &o) {
            if (_db.has_hardfork(STEEMIT_HARDFORK_0_1))
                GOLOS_CHECK_OP_PARAM(o, account,
                    GOLOS_CHECK_VALUE(o.account != STEEMIT_TEMP_ACCOUNT,
                          "Cannot update temp account."));

            if ((_db.has_hardfork(STEEMIT_HARDFORK_0_15__465) ||
                 _db.is_producing()) && o.posting) { // TODO: Add HF 15
                     o.posting->validate();
            }

            const auto &account = _db.get_account(o.account);
            const auto &account_auth = _db.get_authority(o.account);

            if (o.owner) {
#ifndef STEEMIT_BUILD_TESTNET
                if (_db.has_hardfork(STEEMIT_HARDFORK_0_11))
                    GOLOS_CHECK_BANDWIDTH(_db.head_block_time(),
                            account_auth.last_owner_update + STEEMIT_OWNER_UPDATE_LIMIT,
                            bandwidth_exception::change_owner_authority_bandwidth,
                            "Owner authority can only be updated once an hour.");
#endif

                if ((_db.has_hardfork(STEEMIT_HARDFORK_0_15__465) ||
                     _db.is_producing())) // TODO: Add HF 15
                {
                    for (auto a: o.owner->account_auths) {
                        _db.get_account(a.first);
                    }
                }


                _db.update_owner_authority(account, *o.owner);
            }

            if (o.active && (_db.has_hardfork(STEEMIT_HARDFORK_0_15__465) ||
                             _db.is_producing())) // TODO: Add HF 15
            {
                for (auto a: o.active->account_auths) {
                    _db.get_account(a.first);
                }
            }

            if (o.posting && (_db.has_hardfork(STEEMIT_HARDFORK_0_15__465) ||
                              _db.is_producing())) // TODO: Add HF 15
            {
                for (auto a: o.posting->account_auths) {
                    _db.get_account(a.first);
                }
            }

            _db.modify(account, [&](account_object &acc) {
                if (o.memo_key != public_key_type()) {
                    acc.memo_key = o.memo_key;
                }
                if ((o.active || o.owner) && acc.active_challenged) {
                    acc.active_challenged = false;
                    acc.last_active_proved = _db.head_block_time();
                }
                acc.last_account_update = _db.head_block_time();
            });
            store_account_json_metadata(_db, account.name, o.json_metadata, true);

            if (o.active || o.posting) {
                _db.modify(account_auth, [&](account_authority_object &auth) {
                    if (o.active) {
                        auth.active = *o.active;
                    }
                    if (o.posting) {
                        auth.posting = *o.posting;
                    }
                });
            }

        }

        void account_metadata_evaluator::do_apply(const account_metadata_operation& o) {
            const auto& account = _db.get_account(o.account);
            _db.modify(account, [&](account_object& a) {
                a.last_account_update = _db.head_block_time();
            });
            store_account_json_metadata(_db, o.account, o.json_metadata);
        }

/**
 *  Because net_rshares is 0 there is no need to update any pending payout calculations or parent posts.
 */
        void delete_comment_evaluator::do_apply(const delete_comment_operation &o) {
            if (_db.has_hardfork(STEEMIT_HARDFORK_0_10)) {
                const auto &auth = _db.get_account(o.author);
                GOLOS_CHECK_LOGIC(!(auth.owner_challenged || auth.active_challenged),
                        logic_exception::account_is_currently_challenged,
                        "Operation cannot be processed because account is currently challenged.");
            }

            const auto &comment = _db.get_comment(o.author, o.permlink);

            if (_db.has_hardfork(STEEMIT_HARDFORK_0_21__1013)
                && comment.parent_author == STEEMIT_ROOT_POST_PARENT) {

                const auto* wpo = _db.find_worker_proposal(comment.id);
                GOLOS_CHECK_LOGIC(!wpo,
                    logic_exception::cannot_delete_post_with_worker_proposal,
                    "Cannot delete a post with worker proposal.");

                const auto* wto = _db.find_worker_techspec(comment.id);
                GOLOS_CHECK_LOGIC(!wto,
                    logic_exception::cannot_delete_post_with_worker_techspec,
                    "Cannot delete a post with worker techspec.");

                const auto* wto_result = _db.find_worker_result(comment.id);
                GOLOS_CHECK_LOGIC(!wto_result,
                    logic_exception::cannot_delete_post_with_worker_result,
                    "Cannot delete a post with worker result.");
            }

            GOLOS_CHECK_LOGIC(comment.children == 0,
                    logic_exception::cannot_delete_comment_with_replies,
                    "Cannot delete a comment with replies.");

            if (_db.is_producing()) {
                GOLOS_CHECK_LOGIC(comment.net_rshares <= 0,
                        logic_exception::cannot_delete_comment_with_positive_votes,
                        "Cannot delete a comment with network positive votes.");
            }
            if (comment.net_rshares > 0) {
                return;
            }

            const auto &vote_idx = _db.get_index<comment_vote_index>().indices().get<by_comment_voter>();

            auto vote_itr = vote_idx.lower_bound(comment_id_type(comment.id));
            while (vote_itr != vote_idx.end() &&
                   vote_itr->comment == comment.id) {
                const auto &cur_vote = *vote_itr;
                ++vote_itr;
                _db.remove(cur_vote);
            }

            /// this loop can be skiped for validate-only nodes as it is merely gathering stats for indicies
            if (_db.has_hardfork(STEEMIT_HARDFORK_0_6__80) &&
                comment.parent_author != STEEMIT_ROOT_POST_PARENT) {
                auto parent = &_db.get_comment(comment.parent_author, comment.parent_permlink);
                while (parent) {
                    _db.modify(*parent, [&](comment_object &p) {
                        p.children--;
                    });
                    if (parent->parent_author != STEEMIT_ROOT_POST_PARENT) {
                        parent = &_db.get_comment(parent->parent_author, parent->parent_permlink);
                    } else
                    {
                        parent = nullptr;
                    }
                }
            }
            _db.remove(comment);
        }

        struct comment_options_extension_visitor {
            comment_options_extension_visitor(const comment_object& c, database& db)
                    : _c(c), _db(db), _a(db.get_account(_c.author)) {
            }

            using result_type = void;

            const comment_object& _c;
            database& _db;
            const account_object& _a;

            void operator()(const comment_payout_beneficiaries& cpb) const {
                if (_db.is_producing()) {
                    GOLOS_CHECK_LOGIC(cpb.beneficiaries.size() <= STEEMIT_MAX_COMMENT_BENEFICIARIES,
                        logic_exception::cannot_specify_more_beneficiaries,
                        "Cannot specify more than ${m} beneficiaries.", ("m", STEEMIT_MAX_COMMENT_BENEFICIARIES));
                }

                uint16_t total_weight = 0;

                if (_c.beneficiaries.size() == 1
                        && _c.beneficiaries.front().account == _a.referrer_account) {
                    total_weight += _c.beneficiaries[0].weight;
                    auto& referrer = _a.referrer_account;
                    const auto& itr = std::find_if(cpb.beneficiaries.begin(), cpb.beneficiaries.end(),
                            [&referrer](const beneficiary_route_type& benef) {
                        return benef.account == referrer;
                    });
                    GOLOS_CHECK_LOGIC(itr == cpb.beneficiaries.end(),
                        logic_exception::beneficiaries_should_be_unique,
                        "Comment already has '${referrer}' as a referrer-beneficiary.", ("referrer",referrer));
                } else {
                    GOLOS_CHECK_LOGIC(_c.beneficiaries.size() == 0,
                        logic_exception::comment_already_has_beneficiaries,
                        "Comment already has beneficiaries specified.");
                }

                GOLOS_CHECK_LOGIC(_c.abs_rshares == 0,
                    logic_exception::comment_must_not_have_been_voted,
                    "Comment must not have been voted on before specifying beneficiaries.");

                _db.modify(_c, [&](comment_object& c) {
                    for (auto& b : cpb.beneficiaries) {
                        _db.get_account(b.account);   // check beneficiary exists
                        c.beneficiaries.push_back(b);
                        total_weight += b.weight;
                    }
                });

                GOLOS_CHECK_PARAM("beneficiaries", {
                    GOLOS_CHECK_VALUE(total_weight <= STEEMIT_100_PERCENT,
                        "Cannot allocate more than 100% of rewards to a comment");
                });
            }

            void operator()( const comment_auction_window_reward_destination& cawrd ) const {
                ASSERT_REQ_HF(STEEMIT_HARDFORK_0_19__898, "auction window reward destination option");

                const auto& mprops = _db.get_witness_schedule_object().median_props;

                if (_db.has_hardfork(STEEMIT_HARDFORK_0_20__1075)) {
                    GOLOS_CHECK_LOGIC(_c.abs_rshares == 0,
                            logic_exception::comment_must_not_have_been_voted,
                            "Comment must not have been voted on changing auction window reward destination.");
                }

                GOLOS_CHECK_PARAM(cawrd.destination, {
                    GOLOS_CHECK_VALUE(cawrd.destination != to_reward_fund || mprops.allow_return_auction_reward_to_fund,
                        "Returning to reward fund is disallowed."
                    );
                });

                GOLOS_CHECK_PARAM(cawrd.destination, {
                    GOLOS_CHECK_VALUE(cawrd.destination != to_curators || mprops.allow_distribute_auction_reward,
                        "Distributing between curators is disallowed."
                    );
                });

                _db.modify(_c, [&](comment_object& c) {
                    c.auction_window_reward_destination = cawrd.destination;
                });
            }

            void operator()(const comment_curation_rewards_percent& ccrp) const {
                ASSERT_REQ_HF(STEEMIT_HARDFORK_0_19__324, "comment curation rewards percent option");

                const auto& mprops = _db.get_witness_schedule_object().median_props;

                auto percent = ccrp.percent; // Workaround for correct param name in GOLOS_CHECK_PARAM
                
                if (_db.has_hardfork(STEEMIT_HARDFORK_0_20__1075)) {
                    GOLOS_CHECK_LOGIC(_c.abs_rshares == 0,
                            logic_exception::comment_must_not_have_been_voted,
                            "Comment must not have been voted on changing curation rewards percent.");
                }

                GOLOS_CHECK_PARAM(percent, {
                    GOLOS_CHECK_VALUE(mprops.min_curation_percent <= ccrp.percent && ccrp.percent <= mprops.max_curation_percent,
                        "Curation rewards percent must be between ${min} and ${max}.",
                        ("min", mprops.min_curation_percent)("max", mprops.max_curation_percent));
                });

                _db.modify(_c, [&](comment_object& c) {
                    c.curation_rewards_percent = ccrp.percent;
                });
            }
        };

        void comment_options_evaluator::do_apply(const comment_options_operation &o) {
            database &_db = db();
            if (_db.has_hardfork(STEEMIT_HARDFORK_0_10)) {
                const auto &auth = _db.get_account(o.author);
                GOLOS_CHECK_LOGIC(!(auth.owner_challenged || auth.active_challenged),
                        logic_exception::account_is_currently_challenged,
                        "Operation cannot be processed because account is currently challenged.");
            }


            const auto &comment = _db.get_comment(o.author, o.permlink);
            if (!o.allow_curation_rewards || !o.allow_votes || o.max_accepted_payout < comment.max_accepted_payout) {
                GOLOS_CHECK_LOGIC(comment.abs_rshares == 0,
                        logic_exception::comment_options_requires_no_rshares,
                        "One of the included comment options requires the comment to have no rshares allocated to it.");
            }

            GOLOS_CHECK_LOGIC(comment.allow_curation_rewards >= o.allow_curation_rewards,
                    logic_exception::curation_rewards_cannot_be_reenabled,
                    "Curation rewards cannot be re-enabled.");
            GOLOS_CHECK_LOGIC(comment.allow_votes >= o.allow_votes,
                    logic_exception::voting_cannot_be_reenabled,
                    "Voting cannot be re-enabled.");
            GOLOS_CHECK_LOGIC(comment.max_accepted_payout >= o.max_accepted_payout,
                    logic_exception::comment_cannot_accept_greater_payout,
                    "A comment cannot accept a greater payout.");
            GOLOS_CHECK_LOGIC(comment.percent_steem_dollars >= o.percent_steem_dollars,
                    logic_exception::comment_cannot_accept_greater_percent_GBG,
                    "A comment cannot accept a greater percent SBD.");

            _db.modify(comment, [&](comment_object& c) {
                c.max_accepted_payout = o.max_accepted_payout;
                c.percent_steem_dollars = o.percent_steem_dollars;
                c.allow_votes = o.allow_votes;
                c.allow_curation_rewards = o.allow_curation_rewards;
            });

            for (auto& e : o.extensions) {
                e.visit(comment_options_extension_visitor(comment, _db));
            }
        }

        struct comment_bandwidth final {
            database& db;
            const time_point_sec& now;
            const comment_operation& op;
            const chain_properties& mprops;
            const account_object& auth;
            mutable const account_bandwidth_object* band = nullptr;
            mutable time_point_sec last_post_comment;

            uint16_t calc_reward_weight() const {
                band = db.find<account_bandwidth_object, by_account_bandwidth_type>(
                    std::make_tuple(op.author, bandwidth_type::post));

                if (nullptr == band) {
                    band = &db.create<account_bandwidth_object>([&](account_bandwidth_object &b) {
                        b.account = op.author;
                        b.type = bandwidth_type::post;
                    });
                }

                last_post_comment = std::max<time_point_sec>(auth.last_comment, auth.last_post);

                if (db.has_hardfork(STEEMIT_HARDFORK_0_21__1010)) {
                    hf21();
                } else if (db.has_hardfork(STEEMIT_HARDFORK_0_19__533_1002)) {
                    hf19();
                } else if (db.has_hardfork(STEEMIT_HARDFORK_0_12__176)) {
                    hf12();
                } else if (db.has_hardfork(STEEMIT_HARDFORK_0_6__113)) {
                    hf6();
                } else {
                    hf0();
                }

                return calculate();
            }

        private:
            uint16_t calculate() const {
                uint16_t reward_weight = STEEMIT_100_PERCENT;

                if (op.parent_author == STEEMIT_ROOT_POST_PARENT) {
                    auto pb = band->average_bandwidth;

                    if (db.has_hardfork(STEEMIT_HARDFORK_0_12__176)) {
                        auto post_delta_time = std::min(
                            now.sec_since_epoch() - band->last_bandwidth_update.sec_since_epoch(),
                            STEEMIT_POST_AVERAGE_WINDOW);

                        auto old_weight =
                            (pb * (STEEMIT_POST_AVERAGE_WINDOW - post_delta_time)) /
                            STEEMIT_POST_AVERAGE_WINDOW;

                        pb = (old_weight + STEEMIT_100_PERCENT);

                        reward_weight = uint16_t(std::min(
                            (STEEMIT_POST_WEIGHT_CONSTANT * STEEMIT_100_PERCENT) / (pb.value * pb.value),
                            uint64_t(STEEMIT_100_PERCENT)));
                    }

                    db.modify(*band, [&](account_bandwidth_object &b) {
                        b.last_bandwidth_update = now;
                        b.average_bandwidth = pb;
                    });
                }

                return reward_weight;
            }

            void hf21() const {
                if (op.parent_author == STEEMIT_ROOT_POST_PARENT) {
                    auto consumption = mprops.posts_window / mprops.posts_per_window;

                    auto elapsed_seconds = (now - auth.last_post).to_seconds();

                    auto regenerated_capacity = std::min(
                        uint32_t(mprops.posts_window),
                        uint32_t(elapsed_seconds));

                    auto current_capacity = std::min(
                        uint16_t(auth.posts_capacity + regenerated_capacity),
                        mprops.posts_window);

                    GOLOS_CHECK_BANDWIDTH(current_capacity + 1, consumption,
                        bandwidth_exception::post_bandwidth,
                        "You may only post ${posts_per_window} times in ${posts_window} seconds.",
                        ("posts_per_window", mprops.posts_per_window)
                        ("posts_window", mprops.posts_window));

                    db.modify(auth, [&](account_object& a) {
                        a.posts_capacity = current_capacity - consumption;
                    });
                } else {
                    auto consumption = mprops.comments_window / mprops.comments_per_window;

                    auto elapsed_seconds = (now - auth.last_comment).to_seconds();

                    auto regenerated_capacity = std::min(
                        uint32_t(mprops.comments_window),
                        uint32_t(elapsed_seconds));

                    auto current_capacity = std::min(
                        uint16_t(auth.comments_capacity + regenerated_capacity),
                        mprops.comments_window);

                    GOLOS_CHECK_BANDWIDTH(current_capacity + 1, consumption,
                        bandwidth_exception::comment_bandwidth,
                        "You may only comment ${comments_per_window} times in ${comments_window} seconds.",
                        ("comments_per_window", mprops.comments_per_window)
                        ("comments_window", mprops.comments_window));

                    db.modify(auth, [&](account_object& a) {
                        a.comments_capacity = current_capacity - consumption;
                    });
                }
            }

            void hf19() const {
                auto elapsed_seconds = (now - last_post_comment).to_seconds();

                if (op.parent_author == STEEMIT_ROOT_POST_PARENT) {
                    auto consumption = mprops.posts_window / mprops.posts_per_window;

                    auto regenerated_capacity = std::min(
                        uint32_t(mprops.posts_window),
                        uint32_t(elapsed_seconds));

                    auto current_capacity = std::min(
                        uint16_t(auth.posts_capacity + regenerated_capacity),
                        mprops.posts_window);

                    GOLOS_CHECK_BANDWIDTH(current_capacity + 1, consumption,
                        bandwidth_exception::post_bandwidth,
                        "You may only post ${posts_per_window} times in ${posts_window} seconds.",
                        ("posts_per_window", mprops.posts_per_window)
                        ("posts_window", mprops.posts_window));

                    db.modify(auth, [&](account_object& a) {
                        a.posts_capacity = current_capacity - consumption;
                    });
                } else {
                    auto consumption = mprops.comments_window / mprops.comments_per_window;

                    auto regenerated_capacity = std::min(
                        uint32_t(mprops.comments_window),
                        uint32_t(elapsed_seconds));

                    auto current_capacity = std::min(
                        uint16_t(auth.comments_capacity + regenerated_capacity),
                        mprops.comments_window);

                    GOLOS_CHECK_BANDWIDTH(current_capacity + 1, consumption,
                        bandwidth_exception::comment_bandwidth,
                        "You may only comment ${comments_per_window} times in ${comments_window} seconds.",
                        ("comments_per_window", mprops.comments_per_window)
                        ("comments_window", mprops.comments_window));

                    db.modify(auth, [&](account_object& a) {
                        a.comments_capacity = current_capacity - consumption;
                    });
                }
            }

            void hf12() const {
                if (op.parent_author == STEEMIT_ROOT_POST_PARENT) {
                    GOLOS_CHECK_BANDWIDTH(now, band->last_bandwidth_update + STEEMIT_MIN_ROOT_COMMENT_INTERVAL,
                        bandwidth_exception::post_bandwidth,
                        "You may only post once every 5 minutes.");
                } else {
                    GOLOS_CHECK_BANDWIDTH(now, last_post_comment + STEEMIT_MIN_REPLY_INTERVAL,
                        golos::bandwidth_exception::comment_bandwidth,
                        "You may only comment once every 20 seconds.");
                }
            }

            void hf6() const {
                if (op.parent_author == STEEMIT_ROOT_POST_PARENT) {
                    GOLOS_CHECK_BANDWIDTH(now, last_post_comment + STEEMIT_MIN_ROOT_COMMENT_INTERVAL,
                        bandwidth_exception::post_bandwidth,
                        "You may only post once every 5 minutes.");
                } else {
                    GOLOS_CHECK_BANDWIDTH(now, last_post_comment + STEEMIT_MIN_REPLY_INTERVAL,
                        bandwidth_exception::comment_bandwidth,
                        "You may only comment once every 20 seconds.");
                }
            }

            void hf0() const {
                GOLOS_CHECK_BANDWIDTH(now, last_post_comment + 60,
                    bandwidth_exception::post_bandwidth,
                    "You may only post once per minute.");
            }

        }; // struct comment_bandwidth

        void comment_evaluator::do_apply(const comment_operation &o) {
            try {
                if (_db.is_producing() ||
                    _db.has_hardfork(STEEMIT_HARDFORK_0_5__55))
                    GOLOS_CHECK_LOGIC(o.title.size() + o.body.size() + o.json_metadata.size(),
                            logic_exception::cannot_update_comment_because_nothing_changed,
                            "Cannot update comment because nothing appears to be changing.");

                const auto& mprops = _db.get_witness_schedule_object().median_props;

                const auto &by_permlink_idx = _db.get_index<comment_index>().indices().get<by_permlink>();
                auto itr = by_permlink_idx.find(boost::make_tuple(o.author, o.permlink));

                const auto &auth = _db.get_account(o.author); /// prove it exists

                if (_db.has_hardfork(STEEMIT_HARDFORK_0_10)) {
                    GOLOS_CHECK_LOGIC(!(auth.owner_challenged || auth.active_challenged),
                            logic_exception::account_is_currently_challenged,
                            "Operation cannot be processed because account is currently challenged.");
                }

                comment_id_type id;

                const comment_object *parent = nullptr;
                if (o.parent_author != STEEMIT_ROOT_POST_PARENT) {
                    parent = &_db.get_comment(o.parent_author, o.parent_permlink);
                    auto max_depth = STEEMIT_MAX_COMMENT_DEPTH;
                    if (!_db.has_hardfork(STEEMIT_HARDFORK_0_17__430)) {
                        max_depth = STEEMIT_MAX_COMMENT_DEPTH_PRE_HF17;
                    } else if (_db.is_producing()) {
                        max_depth = STEEMIT_SOFT_MAX_COMMENT_DEPTH;
                    }
                    GOLOS_CHECK_LOGIC(parent->depth < max_depth,
                            logic_exception::reached_comment_max_depth,
                            "Comment is nested ${x} posts deep, maximum depth is ${y}.",
                            ("x", parent->depth)("y", max_depth));
                }
                auto now = _db.head_block_time();

                if (itr == by_permlink_idx.end()) {
                    if (o.parent_author != STEEMIT_ROOT_POST_PARENT) {
                        GOLOS_CHECK_LOGIC(_db.get(parent->root_comment).allow_replies,
                                logic_exception::replies_are_not_allowed,
                                "The parent comment has disabled replies.");
                    }

                    uint16_t reward_weight = comment_bandwidth{_db, now, o, mprops, auth}.calc_reward_weight();

                    db().modify(auth, [&](account_object &a) {
                        if (o.parent_author != STEEMIT_ROOT_POST_PARENT) {
                            a.comment_count++;
                            a.last_comment = now;
                        } else {
                            a.post_count++;
                            a.last_post = now;
                        }
                    });

                    bool referrer_to_delete = false;

                    _db.create<comment_object>([&](comment_object &com) {
                        if (_db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                            GOLOS_CHECK_OP_PARAM(o, parent_permlink, validate_permlink_0_1(o.parent_permlink));
                            GOLOS_CHECK_OP_PARAM(o, permlink,        validate_permlink_0_1(o.permlink));
                        }

                        if (_db.has_hardfork(STEEMIT_HARDFORK_0_19__898)) {
                            if (mprops.allow_distribute_auction_reward) {
                                com.auction_window_reward_destination = protocol::to_curators;
                            } else {
                                com.auction_window_reward_destination = protocol::to_reward_fund;
                            }
                            com.auction_window_size = mprops.auction_window_size;
                        }

                        if (_db.has_hardfork(STEEMIT_HARDFORK_0_21__1009)) {
                            com.curation_rewards_percent = std::max(mprops.min_curation_percent,
                                std::min(uint16_t(STEEMIT_DEF_CURATION_PERCENT), mprops.max_curation_percent));
                        } else if (_db.has_hardfork(STEEMIT_HARDFORK_0_19__324)) {
                            com.curation_rewards_percent = mprops.min_curation_percent;
                        } else {
                            com.curation_rewards_percent = STEEMIT_DEF_CURATION_PERCENT;
                        }

                        com.author = o.author;
                        from_string(com.permlink, o.permlink);
                        com.created = _db.head_block_time();
                        com.last_payout = fc::time_point_sec::min();
                        com.max_cashout_time = fc::time_point_sec::maximum();
                        com.reward_weight = reward_weight;

                        if (o.parent_author == STEEMIT_ROOT_POST_PARENT) {
                            com.parent_author = "";
                            from_string(com.parent_permlink, o.parent_permlink);
                            com.root_comment = com.id;
                            com.cashout_time = _db.has_hardfork(STEEMIT_HARDFORK_0_12__177)
                                               ?
                                               _db.head_block_time() +
                                               STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17 :
                                               fc::time_point_sec::maximum();
                        } else {
                            com.parent_author = parent->author;
                            com.parent_permlink = parent->permlink;
                            com.depth = parent->depth + 1;
                            com.root_comment = parent->root_comment;
                            com.cashout_time = fc::time_point_sec::maximum();
                        }

                        if (_db.has_hardfork(STEEMIT_HARDFORK_0_17__431)) {
                            com.cashout_time = com.created + STEEMIT_CASHOUT_WINDOW_SECONDS;
                        }

                        if (auth.referrer_account != account_name_type()) {
                            if (_db.head_block_time() < auth.referral_end_date) {
                                com.beneficiaries.push_back(beneficiary_route_type(auth.referrer_account,
                                    auth.referrer_interest_rate));
                            } else {
                                referrer_to_delete = true;
                            }
                        }
                    });

                    if (referrer_to_delete) {
                        _db.modify(auth, [&](account_object& a) {
                            a.referrer_account = account_name_type();
                            a.referrer_interest_rate = 0;
                            a.referral_end_date = time_point_sec::min();
                            a.referral_break_fee.amount = 0;
                        });
                    }

                    while (parent) {
                        _db.modify(*parent, [&](comment_object& p) {
                            p.children++;
                        });
                        if (parent->parent_author != STEEMIT_ROOT_POST_PARENT) {
                            parent = &_db.get_comment(parent->parent_author, parent->parent_permlink);
                        } else
                        {
                            parent = nullptr;
                        }
                    }

                } else {
                    // start edit case
                    const auto& comment = *itr;
                    _db.modify(comment, [&](comment_object& com) {
                        strcmp_equal equal;

                        GOLOS_CHECK_LOGIC(com.parent_author == (parent ? o.parent_author : account_name_type()),
                                logic_exception::parent_of_comment_cannot_change,
                                "The parent of a comment cannot change.");
                        GOLOS_CHECK_LOGIC(equal(com.parent_permlink, o.parent_permlink),
                                logic_exception::parent_perlink_of_comment_cannot_change,
                                "The parent permlink of a comment cannot change.");

                    });
                } // end EDIT case

            } FC_CAPTURE_AND_RETHROW((o))
        }

        void escrow_transfer_evaluator::do_apply(const escrow_transfer_operation& o) {
            try {
                const auto& from_account = _db.get_account(o.from);
                _db.get_account(o.to);
                _db.get_account(o.agent);

                GOLOS_CHECK_LOGIC(o.ratification_deadline > _db.head_block_time(),
                    logic_exception::escrow_time_in_past,
                    "The escrow ratification deadline must be after head block time.");
                GOLOS_CHECK_LOGIC(o.escrow_expiration > _db.head_block_time(),
                    logic_exception::escrow_time_in_past,
                    "The escrow expiration must be after head block time.");

                asset steem_spent = o.steem_amount;
                asset sbd_spent = o.sbd_amount;
                if (o.fee.symbol == STEEM_SYMBOL) {
                    steem_spent += o.fee;
                } else {
                    sbd_spent += o.fee;
                }
                GOLOS_CHECK_BALANCE(from_account, MAIN_BALANCE, steem_spent);
                GOLOS_CHECK_BALANCE(from_account, MAIN_BALANCE, sbd_spent);
                _db.adjust_balance(from_account, -steem_spent);
                _db.adjust_balance(from_account, -sbd_spent);

                _db.create<escrow_object>([&](escrow_object& esc) {
                    esc.escrow_id = o.escrow_id;
                    esc.from = o.from;
                    esc.to = o.to;
                    esc.agent = o.agent;
                    esc.ratification_deadline = o.ratification_deadline;
                    esc.escrow_expiration = o.escrow_expiration;
                    esc.sbd_balance = o.sbd_amount;
                    esc.steem_balance = o.steem_amount;
                    esc.pending_fee = o.fee;
                });
            }
            FC_CAPTURE_AND_RETHROW((o))
        }

        void escrow_approve_evaluator::do_apply(const escrow_approve_operation& o) {
            try {
                const auto& escrow = _db.get_escrow(o.from, o.escrow_id);
                GOLOS_CHECK_LOGIC(escrow.to == o.to,
                    logic_exception::escrow_bad_to,
                    "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o",o.to)("e",escrow.to));
                GOLOS_CHECK_LOGIC(escrow.agent == o.agent,
                    logic_exception::escrow_bad_agent,
                    "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o",o.agent)("e",escrow.agent));
                GOLOS_CHECK_LOGIC(escrow.ratification_deadline >= _db.head_block_time(),
                    logic_exception::ratification_deadline_passed,
                    "The escrow ratification deadline has passed. Escrow can no longer be ratified.");

                bool reject_escrow = !o.approve;
                if (o.who == o.to) {
                    GOLOS_CHECK_LOGIC(!escrow.to_approved,
                        logic_exception::account_already_approved_escrow,
                        "Account 'to' (${t}) has already approved the escrow.", ("t",o.to));
                    if (!reject_escrow) {
                        _db.modify(escrow, [&](escrow_object& esc) {
                            esc.to_approved = true;
                        });
                    }
                }
                if (o.who == o.agent) {
                    GOLOS_CHECK_LOGIC(!escrow.agent_approved,
                        logic_exception::account_already_approved_escrow,
                        "Account 'agent' (${a}) has already approved the escrow.", ("a",o.agent));
                    if (!reject_escrow) {
                        _db.modify(escrow, [&](escrow_object& esc) {
                            esc.agent_approved = true;
                        });
                    }
                }

                if (reject_escrow) {
                    const auto &from_account = _db.get_account(o.from);
                    _db.adjust_balance(from_account, escrow.steem_balance);
                    _db.adjust_balance(from_account, escrow.sbd_balance);
                    _db.adjust_balance(from_account, escrow.pending_fee);
                    _db.remove(escrow);
                } else if (escrow.to_approved && escrow.agent_approved) {
                    const auto &agent_account = _db.get_account(o.agent);
                    _db.adjust_balance(agent_account, escrow.pending_fee);
                    _db.modify(escrow, [&](escrow_object& esc) {
                        esc.pending_fee.amount = 0;
                    });
                }
            }
            FC_CAPTURE_AND_RETHROW((o))
        }

        void escrow_dispute_evaluator::do_apply(const escrow_dispute_operation& o) {
            try {
                _db.get_account(o.from); // Verify from account exists
                const auto& e = _db.get_escrow(o.from, o.escrow_id);
                GOLOS_CHECK_LOGIC(_db.head_block_time() < e.escrow_expiration,
                    logic_exception::cannot_dispute_expired_escrow,
                    "Disputing the escrow must happen before expiration.");
                GOLOS_CHECK_LOGIC(e.to_approved && e.agent_approved,
                    logic_exception::escrow_must_be_approved_first,
                    "The escrow must be approved by all parties before a dispute can be raised.");
                GOLOS_CHECK_LOGIC(!e.disputed,
                    logic_exception::escrow_already_disputed,
                    "The escrow is already under dispute.");
                GOLOS_CHECK_LOGIC(e.to == o.to,
                    logic_exception::escrow_bad_to,
                    "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o",o.to)("e",e.to));
                GOLOS_CHECK_LOGIC(e.agent == o.agent,
                    logic_exception::escrow_bad_agent,
                    "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o",o.agent)("e",e.agent));

                _db.modify(e, [&](escrow_object& esc) {
                    esc.disputed = true;
                });
            }
            FC_CAPTURE_AND_RETHROW((o))
        }

        void escrow_release_evaluator::do_apply(const escrow_release_operation& o) {
            try {
                _db.get_account(o.from); // Verify from account exists
                const auto& receiver_account = _db.get_account(o.receiver);

                const auto& e = _db.get_escrow(o.from, o.escrow_id);
                GOLOS_CHECK_LOGIC(e.steem_balance >= o.steem_amount,
                    logic_exception::release_amount_exceeds_escrow_balance,
                    "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}",
                    ("a",o.steem_amount)("b",e.steem_balance));
                GOLOS_CHECK_LOGIC(e.sbd_balance >= o.sbd_amount,
                    logic_exception::release_amount_exceeds_escrow_balance,
                    "Release amount exceeds escrow balance. Amount: ${a}, Balance: ${b}",
                    ("a",o.sbd_amount)("b",e.sbd_balance));
                GOLOS_CHECK_LOGIC(e.to == o.to,
                    logic_exception::escrow_bad_to,
                    "Operation 'to' (${o}) does not match escrow 'to' (${e}).", ("o",o.to)("e",e.to));
                GOLOS_CHECK_LOGIC(e.agent == o.agent,
                    logic_exception::escrow_bad_agent,
                    "Operation 'agent' (${a}) does not match escrow 'agent' (${e}).", ("o",o.agent)("e",e.agent));
                GOLOS_CHECK_LOGIC(o.receiver == e.from || o.receiver == e.to,
                    logic_exception::escrow_bad_receiver,
                    "Funds must be released to 'from' (${f}) or 'to' (${t})", ("f",e.from)("t",e.to));
                GOLOS_CHECK_LOGIC(e.to_approved && e.agent_approved,
                    logic_exception::escrow_must_be_approved_first,
                    "Funds cannot be released prior to escrow approval.");

                // If there is a dispute regardless of expiration, the agent can release funds to either party
                if (e.disputed) {
                    GOLOS_CHECK_LOGIC(o.who == e.agent,
                        logic_exception::only_agent_can_release_disputed,
                        "Only 'agent' (${a}) can release funds in a disputed escrow.", ("a",e.agent));
                } else {
                    GOLOS_CHECK_LOGIC(o.who == e.from || o.who == e.to,
                        logic_exception::only_from_to_can_release_non_disputed,
                        "Only 'from' (${f}) and 'to' (${t}) can release funds from a non-disputed escrow",
                        ("f",e.from)("t",e.to));

                    if (e.escrow_expiration > _db.head_block_time()) {
                        // If there is no dispute and escrow has not expired, either party can release funds to the other.
                        if (o.who == e.from) {
                            GOLOS_CHECK_LOGIC(o.receiver == e.to,
                                logic_exception::from_can_release_only_to_to,
                                "Only 'from' (${f}) can release funds to 'to' (${t}).", ("f",e.from)("t",e.to));
                        } else if (o.who == e.to) {
                            GOLOS_CHECK_LOGIC(o.receiver == e.from,
                                logic_exception::to_can_release_only_to_from,
                                "Only 'to' (${t}) can release funds to 'from' (${t}).", ("f",e.from)("t",e.to));
                        }
                    }
                }
                // If escrow expires and there is no dispute, either party can release funds to either party.

                _db.adjust_balance(receiver_account, o.steem_amount);
                _db.adjust_balance(receiver_account, o.sbd_amount);

                _db.modify(e, [&](escrow_object& esc) {
                    esc.steem_balance -= o.steem_amount;
                    esc.sbd_balance -= o.sbd_amount;
                });

                if (e.steem_balance.amount == 0 && e.sbd_balance.amount == 0) {
                    _db.remove(e);
                }
            }
            FC_CAPTURE_AND_RETHROW((o))
        }


        void transfer_evaluator::do_apply(const transfer_operation &o) {
            const auto &from_account = _db.get_account(o.from);
            const auto &to_account = _db.get_account(o.to);

            if (from_account.active_challenged) {
                _db.modify(from_account, [&](account_object &a) {
                    a.active_challenged = false;
                    a.last_active_proved = _db.head_block_time();
                });
            }

            GOLOS_CHECK_OP_PARAM(o, amount, {
                GOLOS_CHECK_BALANCE(from_account, MAIN_BALANCE, o.amount);
                _db.adjust_balance(from_account, -o.amount);
                _db.adjust_balance(to_account, o.amount);
            });
        }

        void transfer_to_vesting_evaluator::do_apply(const transfer_to_vesting_operation &o) {
            const auto &from_account = _db.get_account(o.from);
            const auto &to_account = o.to.size() ? _db.get_account(o.to)
                                                 : from_account;

            GOLOS_CHECK_OP_PARAM(o, amount, {
                GOLOS_CHECK_BALANCE(from_account, MAIN_BALANCE, o.amount);
                _db.adjust_balance(from_account, -o.amount);
                _db.create_vesting(to_account, o.amount);
            });
        }

        void withdraw_vesting_evaluator::do_apply(const withdraw_vesting_operation &o) {
            const auto &account = _db.get_account(o.account);

            GOLOS_CHECK_BALANCE(account, VESTING, asset(0, VESTS_SYMBOL));
            GOLOS_CHECK_BALANCE(account, HAVING_VESTING, o.vesting_shares);

            if (!account.mined && _db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                const auto &props = _db.get_dynamic_global_properties();
                const witness_schedule_object &wso = _db.get_witness_schedule_object();

                asset min_vests = wso.median_props.account_creation_fee *
                                  props.get_vesting_share_price();
                min_vests.amount.value *= 10;

                GOLOS_CHECK_LOGIC(account.vesting_shares.amount > min_vests.amount ||
                          (_db.has_hardfork(STEEMIT_HARDFORK_0_16__562) &&
                           o.vesting_shares.amount == 0),
                           logic_exception::insufficient_fee_for_powerdown_registered_account,
                        "Account registered by another account requires 10x account creation fee worth of Golos Power before it can be powered down.");
            }

            if (o.vesting_shares.amount == 0) {
                if (_db.is_producing() ||
                    _db.has_hardfork(STEEMIT_HARDFORK_0_5__57))
                    GOLOS_CHECK_LOGIC(account.vesting_withdraw_rate.amount != 0,
                            logic_exception::operation_would_not_change_vesting_withdraw_rate,
                            "This operation would not change the vesting withdraw rate.");

                _db.modify(account, [&](account_object &a) {
                    a.vesting_withdraw_rate = asset(0, VESTS_SYMBOL);
                    a.next_vesting_withdrawal = time_point_sec::maximum();
                    a.to_withdraw = 0;
                    a.withdrawn = 0;
                });
            } else {
                int vesting_withdraw_intervals = STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_16;
                if (_db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                    vesting_withdraw_intervals = STEEMIT_VESTING_WITHDRAW_INTERVALS;
                } /// 13 weeks = 1 quarter of a year

                _db.modify(account, [&](account_object &a) {
                    auto new_vesting_withdraw_rate = asset(
                            o.vesting_shares.amount /
                            vesting_withdraw_intervals, VESTS_SYMBOL);

                    if (new_vesting_withdraw_rate.amount == 0)
                        new_vesting_withdraw_rate.amount = 1;

                    if (_db.is_producing() ||
                        _db.has_hardfork(STEEMIT_HARDFORK_0_5__57))
                        GOLOS_CHECK_LOGIC(account.vesting_withdraw_rate != new_vesting_withdraw_rate,
                                logic_exception::operation_would_not_change_vesting_withdraw_rate,
                                "This operation would not change the vesting withdraw rate.");

                    a.vesting_withdraw_rate = new_vesting_withdraw_rate;
                    a.next_vesting_withdrawal = _db.head_block_time() +
                                                fc::seconds(STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS);
                    a.to_withdraw = o.vesting_shares.amount;
                    a.withdrawn = 0;
                });
            }
        }

        void set_withdraw_vesting_route_evaluator::do_apply(const set_withdraw_vesting_route_operation &o) {
            try {
                const auto &from_account = _db.get_account(o.from_account);
                const auto &to_account = _db.get_account(o.to_account);
                const auto &wd_idx = _db.get_index<withdraw_vesting_route_index>().indices().get<by_withdraw_route>();
                auto itr = wd_idx.find(boost::make_tuple(from_account.id, to_account.id));

                if (itr == wd_idx.end()) {
                    GOLOS_CHECK_LOGIC(o.percent != 0,
                            logic_exception::cannot_create_zero_percent_destination,
                            "Cannot create a 0% destination.");
                    GOLOS_CHECK_LOGIC(from_account.withdraw_routes < STEEMIT_MAX_WITHDRAW_ROUTES,
                            logic_exception::reached_maxumum_number_of_routes,
                            "Account already has the maximum number of routes (${max}).",
                            ("max",STEEMIT_MAX_WITHDRAW_ROUTES));

                    _db.create<withdraw_vesting_route_object>([&](withdraw_vesting_route_object &wvdo) {
                        wvdo.from_account = from_account.id;
                        wvdo.to_account = to_account.id;
                        wvdo.percent = o.percent;
                        wvdo.auto_vest = o.auto_vest;
                    });

                    _db.modify(from_account, [&](account_object &a) {
                        a.withdraw_routes++;
                    });
                } else if (o.percent == 0) {
                    _db.remove(*itr);

                    _db.modify(from_account, [&](account_object &a) {
                        a.withdraw_routes--;
                    });
                } else {
                    _db.modify(*itr, [&](withdraw_vesting_route_object &wvdo) {
                        wvdo.from_account = from_account.id;
                        wvdo.to_account = to_account.id;
                        wvdo.percent = o.percent;
                        wvdo.auto_vest = o.auto_vest;
                    });
                }

                itr = wd_idx.upper_bound(boost::make_tuple(from_account.id, account_id_type()));
                fc::safe<uint32_t> total_percent = 0;

                while (itr->from_account == from_account.id &&
                       itr != wd_idx.end()) {
                    total_percent += itr->percent;
                    ++itr;
                }

                GOLOS_CHECK_LOGIC(total_percent <= STEEMIT_100_PERCENT,
                        logic_exception::more_100percent_allocated_to_destinations,
                        "More than 100% of vesting withdrawals allocated to destinations.");
            }
            FC_CAPTURE_AND_RETHROW()
        }

        void account_witness_proxy_evaluator::do_apply(const account_witness_proxy_operation &o) {
            const auto &account = _db.get_account(o.account);
            GOLOS_CHECK_LOGIC(account.proxy != o.proxy,
                    logic_exception::proxy_must_change,
                    "Proxy must change.");

            GOLOS_CHECK_LOGIC(account.can_vote,
                    logic_exception::voter_declined_voting_rights,
                    "Account has declined the ability to vote and cannot proxy votes.");

            /// remove all current votes
            std::array<share_type, STEEMIT_MAX_PROXY_RECURSION_DEPTH + 1> delta;
            delta[0] = -account.vesting_shares.amount;
            for (int i = 0; i < STEEMIT_MAX_PROXY_RECURSION_DEPTH; ++i) {
                delta[i + 1] = -account.proxied_vsf_votes[i];
            }
            _db.adjust_proxied_witness_votes(account, delta);

            if (o.proxy.size()) {
                const auto &new_proxy = _db.get_account(o.proxy);
                flat_set<account_id_type> proxy_chain({account.id, new_proxy.id
                });
                proxy_chain.reserve(STEEMIT_MAX_PROXY_RECURSION_DEPTH + 1);

                /// check for proxy loops and fail to update the proxy if it would create a loop
                auto cprox = &new_proxy;
                while (cprox->proxy.size() != 0) {
                    const auto next_proxy = _db.get_account(cprox->proxy);
                    GOLOS_CHECK_LOGIC(proxy_chain.insert(next_proxy.id).second,
                            logic_exception::proxy_would_create_loop,
                            "This proxy would create a proxy loop.");
                    cprox = &next_proxy;
                    GOLOS_CHECK_LOGIC(proxy_chain.size() <= STEEMIT_MAX_PROXY_RECURSION_DEPTH,
                            logic_exception::proxy_chain_is_too_long,
                            "Proxy chain is too long.");
                }

                /// clear all individual vote records
                _db.clear_witness_votes(account);

                _db.modify(account, [&](account_object &a) {
                    a.proxy = o.proxy;
                });

                /// add all new votes
                for (int i = 0; i <= STEEMIT_MAX_PROXY_RECURSION_DEPTH; ++i) {
                    delta[i] = -delta[i];
                }
                _db.adjust_proxied_witness_votes(account, delta);
            } else { /// we are clearing the proxy which means we simply update the account
                _db.modify(account, [&](account_object &a) {
                    a.proxy = o.proxy;
                });
            }
        }


        void account_witness_vote_evaluator::do_apply(const account_witness_vote_operation &o) {
            const auto &voter = _db.get_account(o.account);
            GOLOS_CHECK_LOGIC(voter.proxy.size() == 0,
                    logic_exception::cannot_vote_when_route_are_set,
                    "A proxy is currently set, please clear the proxy before voting for a witness.");

            if (o.approve)
                GOLOS_CHECK_LOGIC(voter.can_vote,
                        logic_exception::voter_declined_voting_rights,
                        "Account has declined its voting rights.");

            const auto &witness = _db.get_witness(o.witness);

            const auto &by_account_witness_idx = _db.get_index<witness_vote_index>().indices().get<by_account_witness>();
            auto itr = by_account_witness_idx.find(boost::make_tuple(voter.id, witness.id));

            if (itr == by_account_witness_idx.end()) {
                GOLOS_CHECK_LOGIC(o.approve,
                        logic_exception::witness_vote_does_not_exist,
                        "Vote doesn't exist, user must indicate a desire to approve witness.");

                if (_db.has_hardfork(STEEMIT_HARDFORK_0_2)) {
                    GOLOS_CHECK_LOGIC(voter.witnesses_voted_for < STEEMIT_MAX_ACCOUNT_WITNESS_VOTES,
                            logic_exception::account_has_too_many_witness_votes,
                            "Account has voted for too many witnesses.",
                            ("max_votes", STEEMIT_MAX_ACCOUNT_WITNESS_VOTES)); // TODO: Remove after hardfork 2

                    _db.create<witness_vote_object>([&](witness_vote_object &v) {
                        v.witness = witness.id;
                        v.account = voter.id;
                    });

                    if (_db.has_hardfork(STEEMIT_HARDFORK_0_3)) {
                        _db.adjust_witness_vote(witness, voter.witness_vote_weight());
                    } else {
                        _db.adjust_proxied_witness_votes(voter, voter.witness_vote_weight());
                    }

                } else {

                    _db.create<witness_vote_object>([&](witness_vote_object &v) {
                        v.witness = witness.id;
                        v.account = voter.id;
                    });
                    _db.modify(witness, [&](witness_object &w) {
                        w.votes += voter.witness_vote_weight();
                    });

                }
                _db.modify(voter, [&](account_object &a) {
                    a.witnesses_voted_for++;
                });

            } else {
                GOLOS_CHECK_LOGIC(!o.approve,
                        logic_exception::witness_vote_already_exist,
                        "Vote currently exists, user must indicate a desire to reject witness.");

                if (_db.has_hardfork(STEEMIT_HARDFORK_0_2)) {
                    if (_db.has_hardfork(STEEMIT_HARDFORK_0_3)) {
                        _db.adjust_witness_vote(witness, -voter.witness_vote_weight());
                    } else {
                        _db.adjust_proxied_witness_votes(voter, -voter.witness_vote_weight());
                    }
                } else {
                    _db.modify(witness, [&](witness_object &w) {
                        w.votes -= voter.witness_vote_weight();
                    });
                }
                _db.modify(voter, [&](account_object &a) {
                    a.witnesses_voted_for--;
                });
                _db.remove(*itr);
            }
        }

        void vote_evaluator::do_apply(const vote_operation& o) {
            try {
                const auto& comment = _db.get_comment(o.author, o.permlink);
                const auto& voter = _db.get_account(o.voter);

                const auto& mprops = _db.get_witness_schedule_object().median_props;

                GOLOS_CHECK_LOGIC(!(voter.owner_challenged || voter.active_challenged),
                    logic_exception::account_is_currently_challenged,
                    "Account \"${account}\" is currently challenged", ("account", voter.name));

                GOLOS_CHECK_LOGIC(voter.can_vote, logic_exception::voter_declined_voting_rights,
                    "Voter has declined their voting rights");

                if (o.weight > 0) {
                    GOLOS_CHECK_LOGIC(comment.allow_votes, logic_exception::votes_are_not_allowed,
                        "Votes are not allowed on the comment.");
                }

                if (_db.calculate_discussion_payout_time(comment) == fc::time_point_sec::maximum()) {
                    // non-consensus vote (after cashout)
                    const auto& comment_vote_idx = _db.get_index<comment_vote_index>().indices().get<by_comment_voter>();
                    auto itr = comment_vote_idx.find(std::make_tuple(comment.id, voter.id));
                    if (itr == comment_vote_idx.end()) {
                        _db.modify(comment, [&](comment_object &c) {
                            ++c.total_votes;
                        });

                        _db.create<comment_vote_object>([&](comment_vote_object& cvo) {
                            cvo.voter = voter.id;
                            cvo.comment = comment.id;
                            cvo.vote_percent = o.weight;
                            cvo.last_update = _db.head_block_time();
                            cvo.num_changes = -2;           // mark vote that it's ready to be removed (archived comment)
                        });
                    } else {
                        _db.modify(*itr, [&](comment_vote_object& cvo) {
                            cvo.vote_percent = o.weight;
                            cvo.last_update = _db.head_block_time();
                        });
                    }
                    return;
                }

                const auto& comment_vote_idx = _db.get_index<comment_vote_index>().indices().get<by_comment_voter>();
                auto itr = comment_vote_idx.find(std::make_tuple(comment.id, voter.id));

                auto elapsed_seconds = (_db.head_block_time() - voter.last_vote_time).to_seconds();

                if (_db.has_hardfork(STEEMIT_HARDFORK_0_19__533_1002)) {
                    auto consumption = mprops.votes_window / mprops.votes_per_window;

                    auto regenerated_capacity = std::min(uint32_t(mprops.votes_window), uint32_t(elapsed_seconds));
                    auto current_capacity = std::min(uint16_t(voter.voting_capacity + regenerated_capacity), mprops.votes_window);

                    GOLOS_CHECK_BANDWIDTH(current_capacity + 1, consumption,
                        bandwidth_exception::vote_bandwidth,
                        "Can only vote ${votes_per_window} times in ${votes_window} seconds.",
                        ("votes_per_window", mprops.votes_per_window)("votes_window", mprops.votes_window));

                    _db.modify(voter, [&](account_object &a) {
                        a.voting_capacity = current_capacity - consumption;
                    });
                } else {
                    GOLOS_CHECK_BANDWIDTH(_db.head_block_time(), voter.last_vote_time + STEEMIT_MIN_VOTE_INTERVAL_SEC-1,
                        bandwidth_exception::vote_bandwidth, "Can only vote once every 3 seconds.");
                }

                int64_t regenerated_power =
                        (STEEMIT_100_PERCENT * elapsed_seconds) /
                        STEEMIT_VOTE_REGENERATION_SECONDS;
                int64_t current_power = std::min(
                    int64_t(voter.voting_power + regenerated_power),
                    int64_t(STEEMIT_100_PERCENT));
                GOLOS_CHECK_LOGIC(current_power > 0, logic_exception::does_not_have_voting_power,
                        "Account currently does not have voting power.");

                int64_t abs_weight = abs(o.weight);
                int64_t used_power = (current_power * abs_weight) / STEEMIT_100_PERCENT;

                const dynamic_global_property_object &dgpo = _db.get_dynamic_global_properties();

                // used_power = (current_power * abs_weight / STEEMIT_100_PERCENT) * (reserve / max_vote_denom)
                // The second multiplication is rounded up as of HF 259
                int64_t max_vote_denom = dgpo.vote_regeneration_per_day *
                    STEEMIT_VOTE_REGENERATION_SECONDS / (60 * 60 * 24);
                GOLOS_ASSERT(max_vote_denom > 0, golos::internal_error, "max_vote_denom is too small");
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_14__259)) {
                    used_power = (used_power / max_vote_denom) + 1;
                } else {
                    used_power = (used_power + max_vote_denom - 1) / max_vote_denom;
                }
                GOLOS_CHECK_LOGIC(used_power <= current_power, logic_exception::does_not_have_voting_power,
                        "Account does not have enough power to vote.");

                int64_t abs_rshares = (
                    (uint128_t(voter.effective_vesting_shares().amount.value) * used_power) /
                    (STEEMIT_100_PERCENT)).to_uint64();
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_14__259) && abs_rshares == 0) {
                    abs_rshares = 1;
                }

                if (_db.has_hardfork(STEEMIT_HARDFORK_0_14__259)) {
                    GOLOS_CHECK_LOGIC(abs_rshares > 30000000 || o.weight == 0,
                            logic_exception::voting_weight_is_too_small,
                            "Voting weight is too small, please accumulate more voting power or steem power.");
                } else if (_db.has_hardfork(STEEMIT_HARDFORK_0_13__248)) {
                    GOLOS_CHECK_LOGIC(abs_rshares > 30000000 || abs_rshares == 1,
                            logic_exception::voting_weight_is_too_small,
                            "Voting weight is too small, please accumulate more voting power or steem power.");
                }


                if (itr == comment_vote_idx.end()) {
                    std::vector<delegator_vote_interest_rate> delegator_vote_interest_rates;
                    delegator_vote_interest_rates.reserve(100);
                    if (_db.has_hardfork(STEEMIT_HARDFORK_0_19__756) && voter.received_vesting_shares > asset(0, VESTS_SYMBOL)) {
                        const auto& vdo_idx = _db.get_index<vesting_delegation_index>().indices().get<by_received>();
                        auto vdo_itr = vdo_idx.lower_bound(voter.name);
                        for (; vdo_itr != vdo_idx.end() && vdo_itr->delegatee == voter.name; ++vdo_itr) {
                            delegator_vote_interest_rate dvir;
                            dvir.account = vdo_itr->delegator;
                            if (_db.head_block_num() < GOLOS_BUG1074_BLOCK && !_db.has_hardfork(STEEMIT_HARDFORK_0_20__1074)) {
                                dvir.interest_rate = vdo_itr->vesting_shares.amount.value * vdo_itr->interest_rate /
                                    voter.effective_vesting_shares().amount.value;
                            } else {
                                dvir.interest_rate = (uint128_t(vdo_itr->vesting_shares.amount.value) *
                                    vdo_itr->interest_rate / voter.effective_vesting_shares().amount.value).to_uint64();
                            }
                            dvir.payout_strategy = vdo_itr->payout_strategy;
                            if (dvir.interest_rate > 0) {
                                delegator_vote_interest_rates.emplace_back(std::move(dvir));
                            }
                        }
                    }

                    GOLOS_CHECK_OP_PARAM(o, weight, GOLOS_CHECK_VALUE(o.weight != 0, "Vote weight cannot be 0"));
                    /// this is the rshares voting for or against the post
                    int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;
                    if (rshares > 0) {
                        GOLOS_CHECK_LOGIC(_db.head_block_time() <
                            _db.calculate_discussion_payout_time(comment) - STEEMIT_UPVOTE_LOCKOUT,
                            logic_exception::cannot_vote_within_last_minute_before_payout,
                            "Cannot increase reward of post within the last minute before payout.");
                    }

                    //used_power /= (50*7); /// a 100% vote means use .28% of voting power which should force users to spread their votes around over 50+ posts day for a week
                    //if( used_power == 0 ) used_power = 1;

                    _db.modify(voter, [&](account_object &a) {
                        a.voting_power = current_power - used_power;
                        a.last_vote_time = _db.head_block_time();
                    });

                    /// if the current net_rshares is less than 0, the post is getting 0 rewards so it is not factored into total rshares^2
                    fc::uint128_t old_rshares = std::max(comment.net_rshares.value, int64_t(0));
                    const auto &root = _db.get(comment.root_comment);
                    auto old_root_abs_rshares = root.children_abs_rshares.value;

                    fc::uint128_t avg_cashout_sec = 0;

                    if (!_db.has_hardfork(STEEMIT_HARDFORK_0_17__431)) {
                        fc::uint128_t cur_cashout_time_sec = _db.calculate_discussion_payout_time(comment).sec_since_epoch();
                        fc::uint128_t new_cashout_time_sec = _db.head_block_time().sec_since_epoch();

                        if (_db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                            !_db.has_hardfork(STEEMIT_HARDFORK_0_13__257)
                        ) {
                            new_cashout_time_sec += STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17;
                        } else {
                            new_cashout_time_sec += STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF12;
                        }
                        avg_cashout_sec =
                                (cur_cashout_time_sec * old_root_abs_rshares + new_cashout_time_sec * abs_rshares ) /
                                (old_root_abs_rshares + abs_rshares);
                    }


                    GOLOS_CHECK_LOGIC(abs_rshares > 0, logic_exception::cannot_vote_with_zero_rshares,
                            "Cannot vote with 0 rshares.");

                    _db.modify(comment, [&](comment_object &c) {
                        c.net_rshares += rshares;
                        c.abs_rshares += abs_rshares;
                        ++c.total_votes;

                        if (rshares > 0) {
                            c.vote_rshares += rshares;
                        }
                        if (rshares > 0) {
                            c.net_votes++;
                        } else {
                            c.net_votes--;
                        }
                        if (!_db.has_hardfork(STEEMIT_HARDFORK_0_6__114) && c.net_rshares == -c.abs_rshares)
                            GOLOS_ASSERT(c.net_votes < 0, golos::internal_error, "Comment has negative network votes?");
                    });

                    _db.modify(root, [&](comment_object &c) {
                        c.children_abs_rshares += abs_rshares;
                        if (!_db.has_hardfork( STEEMIT_HARDFORK_0_17__431)) {
                            if (_db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                                c.last_payout > fc::time_point_sec::min()
                            ) {
                                c.cashout_time = c.last_payout + STEEMIT_SECOND_CASHOUT_WINDOW;
                            } else {
                                c.cashout_time = fc::time_point_sec(
                                        std::min(uint32_t(avg_cashout_sec.to_uint64()),
                                                 c.max_cashout_time.sec_since_epoch()));
                            }

                            if (c.max_cashout_time == fc::time_point_sec::maximum()) {
                                c.max_cashout_time =
                                        _db.head_block_time() + fc::seconds(STEEMIT_MAX_CASHOUT_WINDOW_SECONDS);
                            }
                        }
                    });

                    fc::uint128_t new_rshares = std::max(comment.net_rshares.value, int64_t(0));

                    /// calculate rshares2 value
                    new_rshares = _db.calculate_vshares(new_rshares);
                    old_rshares = _db.calculate_vshares(old_rshares);

                    _db.create<comment_vote_object>([&](comment_vote_object &cv) {
                        cv.voter = voter.id;
                        cv.comment = comment.id;
                        cv.rshares = rshares;
                        cv.vote_percent = o.weight;
                        cv.last_update = _db.head_block_time();

                        if (rshares > 0 && (comment.last_payout == fc::time_point_sec()) && comment.allow_curation_rewards) {
                            cv.orig_rshares = rshares;

                            if (_db.head_block_time() > fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME)) {
                                /// start enforcing this prior to the hardfork

                                /// discount weight by time
                                uint64_t delta_t = std::min(
                                    uint64_t((cv.last_update - comment.created).to_seconds()),
                                    uint64_t(comment.auction_window_size));

                                if (_db.has_hardfork(STEEMIT_HARDFORK_0_19__898)) {
                                    if (voter.name == comment.author) { // self upvote
                                        cv.auction_time = comment.auction_window_size;
                                    } else {
                                        cv.auction_time = static_cast<uint16_t>(delta_t);
                                    }
                                } else {
                                    cv.auction_time = static_cast<uint16_t>(delta_t);
                                }
                            }

                            if (!delegator_vote_interest_rates.empty()) {
                                for (auto& dvir : delegator_vote_interest_rates) {
                                    cv.delegator_vote_interest_rates.push_back(dvir);
                                }
                            }
                        }
                    });

                    _db.adjust_rshares2(comment, old_rshares, new_rshares);
                } else {
                    GOLOS_CHECK_LOGIC(itr->num_changes < STEEMIT_MAX_VOTE_CHANGES,
                            logic_exception::voter_has_used_maximum_vote_changes,
                            "Voter has used the maximum number of vote changes on this comment.");
                    GOLOS_CHECK_LOGIC(itr->vote_percent != o.weight,
                            logic_exception::already_voted_in_similar_way,
                            "You have already voted in a similar way.");

                    /// this is the rshares voting for or against the post
                    int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;

                    if (itr->rshares < rshares) {
                        GOLOS_CHECK_LOGIC(_db.head_block_time() <
                            _db.calculate_discussion_payout_time(comment) - STEEMIT_UPVOTE_LOCKOUT,
                            logic_exception::cannot_vote_within_last_minute_before_payout,
                            "Cannot increase reward of post within the last minute before payout.");
                    }

                    _db.modify(voter, [&](account_object& a) {
                        a.voting_power = current_power - used_power;
                        a.last_vote_time = _db.head_block_time();
                    });

                    /// if the current net_rshares is less than 0, the post is getting 0 rewards so it is not factored into total rshares^2
                    fc::uint128_t old_rshares = std::max(comment.net_rshares.value, int64_t(0));
                    const auto &root = _db.get(comment.root_comment);
                    auto old_root_abs_rshares = root.children_abs_rshares.value;

                    fc::uint128_t avg_cashout_sec = 0;

                    if (!_db.has_hardfork(STEEMIT_HARDFORK_0_17__431)) {
                        fc::uint128_t cur_cashout_time_sec = _db.calculate_discussion_payout_time(comment).sec_since_epoch();
                        fc::uint128_t new_cashout_time_sec = _db.head_block_time().sec_since_epoch();

                        if (_db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                            !_db.has_hardfork(STEEMIT_HARDFORK_0_13__257)
                        ) {
                            new_cashout_time_sec += STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17;
                        } else {
                            new_cashout_time_sec += STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF12;
                        }

                        if (_db.has_hardfork(STEEMIT_HARDFORK_0_14__259) && abs_rshares == 0) {
                            avg_cashout_sec = cur_cashout_time_sec;
                        } else {
                            avg_cashout_sec =
                                    (cur_cashout_time_sec * old_root_abs_rshares + new_cashout_time_sec * abs_rshares) /
                                    (old_root_abs_rshares + abs_rshares);
                        }
                    }

                    _db.modify(comment, [&](comment_object &c) {
                        c.net_rshares -= itr->rshares;
                        c.net_rshares += rshares;
                        c.abs_rshares += abs_rshares;

                        /// TODO: figure out how to handle remove a vote (rshares == 0 )
                        if (rshares > 0 && itr->rshares < 0) {
                            c.net_votes += 2;
                        } else if (rshares > 0 && itr->rshares == 0) {
                            c.net_votes += 1;
                        } else if (rshares == 0 && itr->rshares < 0) {
                            c.net_votes += 1;
                        } else if (rshares == 0 && itr->rshares > 0) {
                            c.net_votes -= 1;
                        } else if (rshares < 0 && itr->rshares == 0) {
                            c.net_votes -= 1;
                        } else if (rshares < 0 && itr->rshares > 0) {
                            c.net_votes -= 2;
                        }
                    });

                    _db.modify(root, [&](comment_object &c) {
                        c.children_abs_rshares += abs_rshares;
                        if (!_db.has_hardfork(STEEMIT_HARDFORK_0_17__431)) {
                            if (_db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                                c.last_payout > fc::time_point_sec::min()
                            ) {
                                c.cashout_time = c.last_payout + STEEMIT_SECOND_CASHOUT_WINDOW;
                            } else {
                                c.cashout_time = fc::time_point_sec(std::min(
                                    uint32_t(avg_cashout_sec.to_uint64()), c.max_cashout_time.sec_since_epoch()));
                            }

                            if (c.max_cashout_time == fc::time_point_sec::maximum()) {
                                c.max_cashout_time =
                                    _db.head_block_time() + fc::seconds(STEEMIT_MAX_CASHOUT_WINDOW_SECONDS);
                            }
                        }
                    });

                    fc::uint128_t new_rshares = std::max(comment.net_rshares.value, int64_t(0));

                    /// calculate rshares2 value
                    new_rshares = _db.calculate_vshares(new_rshares);
                    old_rshares = _db.calculate_vshares(old_rshares);

                    _db.modify(*itr, [&](comment_vote_object &cv) {
                        cv.rshares = rshares;
                        cv.vote_percent = o.weight;
                        cv.last_update = _db.head_block_time();
                        cv.num_changes += 1;

                        cv.delegator_vote_interest_rates.clear();
                    });

                    _db.adjust_rshares2(comment, old_rshares, new_rshares);
                }
            } FC_CAPTURE_AND_RETHROW((o))
        }

        void custom_evaluator::do_apply(const custom_operation &o) {
        }

        void custom_json_evaluator::do_apply(const custom_json_operation &o) {
            database &d = db();
            std::shared_ptr<custom_operation_interpreter> eval = d.get_custom_json_evaluator(o.id);
            if (!eval) {
                return;
            }

            try {
                eval->apply(o);
            }
            catch (const fc::exception &e) {
                if (d.is_producing()) {
                    throw;
                }
            }
            catch (...) {
                elog("Unexpected exception applying custom json evaluator.");
            }
        }


        void custom_binary_evaluator::do_apply(const custom_binary_operation &o) {
            database &d = db();

            std::shared_ptr<custom_operation_interpreter> eval = d.get_custom_json_evaluator(o.id);
            if (!eval) {
                return;
            }

            try {
                eval->apply(o);
            }
            catch (const fc::exception &e) {
                if (d.is_producing()) {
                    throw;
                }
            }
            catch (...) {
                elog("Unexpected exception applying custom json evaluator.");
            }
        }


        template<typename Operation>
        void pow_apply(database &db, Operation o) {
            const auto &dgp = db.get_dynamic_global_properties();

            if (db.is_producing() ||
                db.has_hardfork(STEEMIT_HARDFORK_0_5__59)) {
                const auto &witness_by_work = db.get_index<witness_index>().indices().get<by_work>();
                auto work_itr = witness_by_work.find(o.work.work);
                GOLOS_CHECK_LOGIC(work_itr == witness_by_work.end(),
                        logic_exception::duplicate_work_discovered,
                        "Duplicate work discovered (${work} ${witness})", ("work", o)("witness", *work_itr));
            }

            const auto& name = o.get_worker_account();
            const auto& accounts_by_name = db.get_index<account_index>().indices().get<by_name>();
            auto itr = accounts_by_name.find(name);
            if (itr == accounts_by_name.end()) {
                db.create<account_object>([&](account_object &acc) {
                    acc.name = name;
                    acc.memo_key = o.work.worker;
                    acc.created = dgp.time;
                    acc.last_vote_time = dgp.time;

                    if (!db.has_hardfork(STEEMIT_HARDFORK_0_11__169)) {
                        acc.recovery_account = STEEMIT_INIT_MINER_NAME;
                    } else {
                        acc.recovery_account = "";
                    } /// highest voted witness at time of recovery
                });
                store_account_json_metadata(db, name, "");

                db.create<account_authority_object>([&](account_authority_object &auth) {
                    auth.account = name;
                    auth.owner = authority(1, o.work.worker, 1);
                    auth.active = auth.owner;
                    auth.posting = auth.owner;
                });
            }

            const auto &worker_account = db.get_account(name); // verify it exists
            const auto &worker_auth = db.get_authority(name);
            GOLOS_CHECK_LOGIC(worker_auth.active.num_auths() == 1,
                    logic_exception::miners_can_only_have_one_key_authority,
                    "Miners can only have one key authority. ${a}", ("a", worker_auth.active));
            GOLOS_CHECK_LOGIC(worker_auth.active.key_auths.size() == 1,
                    logic_exception::miners_can_only_have_one_key_authority,
                    "Miners may only have one key authority.");
            GOLOS_CHECK_LOGIC(worker_auth.active.key_auths.begin()->first == o.work.worker,
                    logic_exception::work_must_be_performed_by_signed_key,
                    "Work must be performed by key that signed the work.");
            GOLOS_CHECK_LOGIC(o.block_id == db.head_block_id(),
                    logic_exception::work_not_for_last_block,
                    "pow not for last block");

            if (db.has_hardfork(STEEMIT_HARDFORK_0_13__256))
                GOLOS_CHECK_LOGIC(worker_account.last_account_update < db.head_block_time(),
                        logic_exception::account_must_not_be_updated_in_this_block,
                        "Worker account must not have updated their account this block.");

            fc::sha256 target = db.get_pow_target();

            GOLOS_CHECK_LOGIC(o.work.work < target,
                    logic_exception::insufficient_work_difficalty,
                    "Work lacks sufficient difficulty.");

            db.modify(dgp, [&](dynamic_global_property_object &p) {
                p.total_pow++; // make sure this doesn't break anything...
                p.num_pow_witnesses++;
            });


            const witness_object *cur_witness = db.find_witness(worker_account.name);
            if (cur_witness) {
                GOLOS_CHECK_LOGIC(cur_witness->pow_worker == 0,
                        logic_exception::account_already_scheduled_for_work,
                        "This account is already scheduled for pow block production.");
                db.modify(*cur_witness, [&](witness_object &w) {
                    w.props = o.props;
                    w.pow_worker = dgp.total_pow;
                    w.last_work = o.work.work;
                });
            } else {
                db.create<witness_object>([&](witness_object &w) {
                    w.owner = name;
                    w.props = o.props;
                    w.signing_key = o.work.worker;
                    w.pow_worker = dgp.total_pow;
                    w.last_work = o.work.work;
                });
            }
            /// POW reward depends upon whether we are before or after MINER_VOTING kicks in
            asset pow_reward = db.get_pow_reward();
            if (db.head_block_num() < STEEMIT_START_MINER_VOTING_BLOCK) {
                pow_reward.amount *= STEEMIT_MAX_WITNESSES;
            }
            db.adjust_supply(pow_reward, true);

            /// pay the witness that includes this POW
            const auto &inc_witness = db.get_account(dgp.current_witness);
            if (db.head_block_num() < STEEMIT_START_MINER_VOTING_BLOCK) {
                db.adjust_balance(inc_witness, pow_reward);
            } else {
                db.create_vesting(inc_witness, pow_reward);
            }
        }

        void pow_evaluator::do_apply(const pow_operation &o) {
            if (db().has_hardfork(STEEMIT_HARDFORK_0_13__256)) {
                FC_THROW_EXCEPTION(golos::unsupported_operation, "pow is deprecated. Use pow2 instead");
            }
            pow_apply(db(), o);
        }


        void pow2_evaluator::do_apply(const pow2_operation &o) {
            database &db = this->db();
            const auto &dgp = db.get_dynamic_global_properties();
            uint32_t target_pow = db.get_pow_summary_target();
            account_name_type worker_account;

            if (db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                const auto &work = o.work.get<equihash_pow>();
                GOLOS_CHECK_LOGIC(work.prev_block == db.head_block_id(),
                        logic_exception::work_not_for_last_block,
                        "Equihash pow op not for last block");
                auto recent_block_num = protocol::block_header::num_from_id(work.input.prev_block);
                GOLOS_CHECK_LOGIC(recent_block_num > dgp.last_irreversible_block_num,
                        logic_exception::work_for_block_older_last_irreversible_block,
                        "Equihash pow done for block older than last irreversible block num");
                GOLOS_CHECK_LOGIC(work.pow_summary < target_pow,
                        logic_exception::insufficient_work_difficalty,
                        "Insufficient work difficulty. Work: ${w}, Target: ${t}", ("w", work.pow_summary)("t", target_pow));
                worker_account = work.input.worker_account;
            } else {
                const auto &work = o.work.get<pow2>();
                GOLOS_CHECK_LOGIC(work.input.prev_block == db.head_block_id(),
                        logic_exception::work_not_for_last_block,
                        "Work not for last block");
                GOLOS_CHECK_LOGIC(work.pow_summary < target_pow,
                        logic_exception::insufficient_work_difficalty,
                        "Insufficient work difficulty. Work: ${w}, Target: ${t}", ("w", work.pow_summary)("t", target_pow));
                worker_account = work.input.worker_account;
            }

            GOLOS_CHECK_OP_PARAM(o, props.maximum_block_size,
                GOLOS_CHECK_VALUE(o.props.maximum_block_size >= STEEMIT_MIN_BLOCK_SIZE_LIMIT * 2,
                    "Voted maximum block size is too small. Must be more then ${max} bytes.",
                    ("max", STEEMIT_MIN_BLOCK_SIZE_LIMIT * 2)));

            db.modify(dgp, [&](dynamic_global_property_object &p) {
                p.total_pow++;
                p.num_pow_witnesses++;
            });

            const auto &accounts_by_name = db.get_index<account_index>().indices().get<by_name>();
            auto itr = accounts_by_name.find(worker_account);
            if (itr == accounts_by_name.end()) {
                GOLOS_CHECK_OP_PARAM(o, new_owner_key,
                    GOLOS_CHECK_VALUE(o.new_owner_key.valid(), "Key is not valid."));
                db.create<account_object>([&](account_object &acc) {
                    acc.name = worker_account;
                    acc.memo_key = *o.new_owner_key;
                    acc.created = dgp.time;
                    acc.last_vote_time = dgp.time;
                    acc.recovery_account = ""; /// highest voted witness at time of recovery
                });
                store_account_json_metadata(db, worker_account, "");

                db.create<account_authority_object>([&](account_authority_object &auth) {
                    auth.account = worker_account;
                    auth.owner = authority(1, *o.new_owner_key, 1);
                    auth.active = auth.owner;
                    auth.posting = auth.owner;
                });

                db.create<witness_object>([&](witness_object &w) {
                    w.owner = worker_account;
                    w.props = o.props;
                    w.signing_key = *o.new_owner_key;
                    w.pow_worker = dgp.total_pow;
                });
            } else {
                GOLOS_CHECK_LOGIC(!o.new_owner_key.valid(),
                        logic_exception::cannot_specify_owner_key_unless_creating_account,
                        "Cannot specify an owner key unless creating account.");
                const witness_object *cur_witness = db.find_witness(worker_account);
                GOLOS_CHECK_LOGIC(cur_witness,
                        logic_exception::witness_must_be_created_before_minning,
                        "Witness must be created for existing account before mining.");
                GOLOS_CHECK_LOGIC(cur_witness->pow_worker == 0,
                        logic_exception::account_already_scheduled_for_work,
                        "This account is already scheduled for pow block production.");
                db.modify(*cur_witness, [&](witness_object &w) {
                    w.props = o.props;
                    w.pow_worker = dgp.total_pow;
                });
            }

            if (!db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                /// pay the witness that includes this POW
                asset inc_reward = db.get_pow_reward();
                db.adjust_supply(inc_reward, true);

                const auto &inc_witness = db.get_account(dgp.current_witness);
                db.create_vesting(inc_witness, inc_reward);
            }
        }

        void feed_publish_evaluator::do_apply(const feed_publish_operation &o) {
            const auto &witness = _db.get_witness(o.publisher);
            _db.modify(witness, [&](witness_object &w) {
                w.sbd_exchange_rate = o.exchange_rate;
                w.last_sbd_exchange_update = _db.head_block_time();
            });
        }

        void convert_evaluator::do_apply(const convert_operation &o) {
            const auto &owner = _db.get_account(o.owner);
            GOLOS_CHECK_BALANCE(owner, MAIN_BALANCE, o.amount);

            _db.adjust_balance(owner, -o.amount);

            const auto &fhistory = _db.get_feed_history();
            GOLOS_CHECK_LOGIC(!fhistory.current_median_history.is_null(),
                    logic_exception::no_price_feed_yet,
                    "Cannot convert SBD because there is no price feed.");

            auto steem_conversion_delay = STEEMIT_CONVERSION_DELAY_PRE_HF_16;
            if (_db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                steem_conversion_delay = STEEMIT_CONVERSION_DELAY;
            }

            GOLOS_CHECK_OBJECT_MISSING(_db, convert_request, o.owner, o.requestid);

            _db.create<convert_request_object>([&](convert_request_object &obj) {
                obj.owner = o.owner;
                obj.requestid = o.requestid;
                obj.amount = o.amount;
                obj.conversion_date =
                        _db.head_block_time() + steem_conversion_delay;
            });

        }

        void limit_order_create_evaluator::do_apply(const limit_order_create_operation& o) {
            GOLOS_CHECK_OP_PARAM(o, expiration, {
                GOLOS_CHECK_VALUE(o.expiration > _db.head_block_time(),
                        "Limit order has to expire after head block time.");
            });

            const auto &owner = _db.get_account(o.owner);

            GOLOS_CHECK_BALANCE(owner, MAIN_BALANCE, o.amount_to_sell);
            _db.adjust_balance(owner, -o.amount_to_sell);

            GOLOS_CHECK_OBJECT_MISSING(_db, limit_order, o.owner, o.orderid);

            const auto &order = _db.create<limit_order_object>([&](limit_order_object &obj) {
                obj.created = _db.head_block_time();
                obj.seller = o.owner;
                obj.orderid = o.orderid;
                obj.for_sale = o.amount_to_sell.amount;
                obj.sell_price = o.get_price();
                obj.expiration = o.expiration;
            });

            bool filled = _db.apply_order(order);

            if (o.fill_or_kill)
                GOLOS_CHECK_LOGIC(filled, logic_exception::cancelling_not_filled_order,
                        "Cancelling order because it was not filled.");
        }

        void limit_order_create2_evaluator::do_apply(const limit_order_create2_operation& o) {
            GOLOS_CHECK_OP_PARAM(o, expiration, {
                GOLOS_CHECK_VALUE(o.expiration > _db.head_block_time(),
                        "Limit order has to expire after head block time.");
            });

            const auto &owner = _db.get_account(o.owner);

            GOLOS_CHECK_BALANCE(owner, MAIN_BALANCE, o.amount_to_sell);
            _db.adjust_balance(owner, -o.amount_to_sell);

            GOLOS_CHECK_OBJECT_MISSING(_db, limit_order, o.owner, o.orderid);

            const auto &order = _db.create<limit_order_object>([&](limit_order_object &obj) {
                obj.created = _db.head_block_time();
                obj.seller = o.owner;
                obj.orderid = o.orderid;
                obj.for_sale = o.amount_to_sell.amount;
                obj.sell_price = o.exchange_rate;
                obj.expiration = o.expiration;
            });

            bool filled = _db.apply_order(order);

            if (o.fill_or_kill)
                GOLOS_CHECK_LOGIC(filled, logic_exception::cancelling_not_filled_order,
                        "Cancelling order because it was not filled.");
        }

        void limit_order_cancel_evaluator::do_apply(const limit_order_cancel_operation &o) {
            _db.cancel_order(_db.get_limit_order(o.owner, o.orderid));
        }

        void report_over_production_evaluator::do_apply(const report_over_production_operation &o) {
            if (_db.has_hardfork(STEEMIT_HARDFORK_0_4)) {
                FC_THROW_EXCEPTION(golos::unsupported_operation, "report_over_production_operation is disabled");
            }
        }

        void challenge_authority_evaluator::do_apply(const challenge_authority_operation& o) {
            if (_db.has_hardfork(STEEMIT_HARDFORK_0_14__307))
                GOLOS_ASSERT(false, golos::unsupported_operation, "Challenge authority operation is currently disabled.");
            // TODO: update error handling if enable this operation

            const auto& challenged = _db.get_account(o.challenged);
            const auto& challenger = _db.get_account(o.challenger);

            if (o.require_owner) {
                FC_ASSERT(challenged.reset_account == o.challenger,
                    "Owner authority can only be challenged by its reset account.");
                FC_ASSERT(challenger.balance >= STEEMIT_OWNER_CHALLENGE_FEE);
                FC_ASSERT(!challenged.owner_challenged);
                FC_ASSERT(_db.head_block_time() - challenged.last_owner_proved > STEEMIT_OWNER_CHALLENGE_COOLDOWN);

                _db.adjust_balance(challenger, -STEEMIT_OWNER_CHALLENGE_FEE);
                _db.create_vesting(_db.get_account(o.challenged), STEEMIT_OWNER_CHALLENGE_FEE);

                _db.modify(challenged, [&](account_object &a) {
                    a.owner_challenged = true;
                });
            } else {
                FC_ASSERT(challenger.balance >= STEEMIT_ACTIVE_CHALLENGE_FEE,
                    "Account does not have sufficient funds to pay challenge fee.");
                FC_ASSERT(!(challenged.owner_challenged || challenged.active_challenged),
                    "Account is already challenged.");
                FC_ASSERT(_db.head_block_time() - challenged.last_active_proved > STEEMIT_ACTIVE_CHALLENGE_COOLDOWN,
                    "Account cannot be challenged because it was recently challenged.");

                _db.adjust_balance(challenger, -STEEMIT_ACTIVE_CHALLENGE_FEE);
                _db.create_vesting(_db.get_account(o.challenged), STEEMIT_ACTIVE_CHALLENGE_FEE);

                _db.modify(challenged, [&](account_object& a) {
                    a.active_challenged = true;
                });
            }
        }

        void prove_authority_evaluator::do_apply(const prove_authority_operation& o) {
            const auto& challenged = _db.get_account(o.challenged);
            GOLOS_CHECK_LOGIC(challenged.owner_challenged || challenged.active_challenged,
                logic_exception::account_is_not_challeneged,
                "Account is not challeneged. No need to prove authority.");

            _db.modify(challenged, [&](account_object& a) {
                a.active_challenged = false;
                a.last_active_proved = _db.head_block_time();   // TODO: if enable `challenge_authority` then check, is it ok to set active always
                if (o.require_owner) {
                    a.owner_challenged = false;
                    a.last_owner_proved = _db.head_block_time();
                }
            });
        }

        void request_account_recovery_evaluator::do_apply(const request_account_recovery_operation& o) {
            const auto& account_to_recover = _db.get_account(o.account_to_recover);
            if (account_to_recover.recovery_account.length()) {
                // Make sure recovery matches expected recovery account
                GOLOS_CHECK_LOGIC(account_to_recover.recovery_account == o.recovery_account,
                    logic_exception::cannot_recover_if_not_partner,
                    "Cannot recover an account that does not have you as there recovery partner.");
            } else {
                // Empty string recovery account defaults to top witness
                GOLOS_CHECK_LOGIC(
                    _db.get_index<witness_index>().indices().get<by_vote_name>().begin()->owner == o.recovery_account,
                    logic_exception::must_be_recovered_by_top_witness,
                    "Top witness must recover an account with no recovery partner.");
            }

            const auto& recovery_request_idx =
                _db.get_index<account_recovery_request_index>().indices().get<by_account>();
            auto request = recovery_request_idx.find(o.account_to_recover);

            if (request == recovery_request_idx.end()) {
                // New Request
                GOLOS_CHECK_OP_PARAM(o, new_owner_authority, {
                    GOLOS_CHECK_VALUE(!o.new_owner_authority.is_impossible(),
                        "Cannot recover using an impossible authority.");
                    GOLOS_CHECK_VALUE(o.new_owner_authority.weight_threshold,
                        "Cannot recover using an open authority.");
                });
                // Check accounts in the new authority exist
                if (_db.has_hardfork(STEEMIT_HARDFORK_0_15__465)) {
                    for (auto& a : o.new_owner_authority.account_auths) {
                        _db.get_account(a.first);
                    }
                }
                _db.create<account_recovery_request_object>([&](account_recovery_request_object& req) {
                    req.account_to_recover = o.account_to_recover;
                    req.new_owner_authority = o.new_owner_authority;
                    req.expires = _db.head_block_time() + STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD;
                });
            } else if (o.new_owner_authority.weight_threshold == 0) {
                // Cancel Request if authority is open
                _db.remove(*request);
            } else {
                // Change Request
                GOLOS_CHECK_OP_PARAM(o, new_owner_authority, {
                    GOLOS_CHECK_VALUE(!o.new_owner_authority.is_impossible(),
                    "Cannot recover using an impossible authority.");
                });
                // Check accounts in the new authority exist
                if (_db.has_hardfork(STEEMIT_HARDFORK_0_15__465)) {
                    for (auto& a : o.new_owner_authority.account_auths) {
                        _db.get_account(a.first);
                    }
                }
                _db.modify(*request, [&](account_recovery_request_object& req) {
                    req.new_owner_authority = o.new_owner_authority;
                    req.expires = _db.head_block_time() + STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD;
                });
            }
        }

namespace {

        void stop_withdraw(database& db, const golos::chain::account_object& account) {
            db.modify(account, [&](account_object& a) {
                a.vesting_withdraw_rate.amount = 0;
                a.next_vesting_withdrawal = fc::time_point_sec::maximum();
                a.withdrawn = 0;
                a.to_withdraw = 0;
            });
        }

        void remove_vesting_routes(database& db, const golos::chain::account_object& account) {
            const auto & withdraw_idx = db.get_index<withdraw_vesting_route_index>().indices().get<by_withdraw_route>();
            auto withdraw_it = withdraw_idx.upper_bound(account.id);

            while (withdraw_it != withdraw_idx.end() && withdraw_it->from_account == account.id) {
                const auto& val = *withdraw_it;
                ++withdraw_it;
                db.remove(val);
            }
        }

        void reset_vesting_withdraw(database& db, const account_object& account) {
            stop_withdraw(db, account);
            remove_vesting_routes(db, account);
        }
}

        void recover_account_evaluator::do_apply(const recover_account_operation& o) {
            const auto& account = _db.get_account(o.account_to_recover);
            const auto now = _db.head_block_time();

            if (_db.has_hardfork(STEEMIT_HARDFORK_0_12)) {
                GOLOS_CHECK_BANDWIDTH(now, account.last_account_recovery + STEEMIT_OWNER_UPDATE_LIMIT,
                    bandwidth_exception::change_owner_authority_bandwidth,
                    "Owner authority can only be updated once an hour.");
            }

            const auto& recovery_request_idx = _db.get_index<account_recovery_request_index>().indices().get<by_account>();
            auto request = recovery_request_idx.find(o.account_to_recover);

            GOLOS_CHECK_LOGIC(request != recovery_request_idx.end(),
                logic_exception::no_active_recovery_request,
                "There are no active recovery requests for this account.");
            GOLOS_CHECK_LOGIC(request->new_owner_authority == o.new_owner_authority,
                logic_exception::authority_does_not_match_request,
                "New owner authority does not match recovery request.");

            const auto& recent_auth_idx = _db.get_index<owner_authority_history_index>().indices().get<by_account>();
            auto hist = recent_auth_idx.lower_bound(o.account_to_recover);
            bool found = false;

            while (hist != recent_auth_idx.end() && hist->account == o.account_to_recover && !found) {
                found = hist->previous_owner_authority == o.recent_owner_authority;
                if (found) {
                    break;
                }
                ++hist;
            }
            GOLOS_CHECK_LOGIC(found, logic_exception::no_recent_authority_in_history,
                "Recent authority not found in authority history.");

            _db.remove(*request); // Remove first, update_owner_authority may invalidate iterator
            _db.update_owner_authority(account, o.new_owner_authority);
            _db.modify(account, [&](account_object& a) {
                a.last_account_recovery = now;
            });

            if (_db.has_hardfork(STEEMIT_HARDFORK_0_19__971)) {
                reset_vesting_withdraw(_db, account);
            }
        }

        void change_recovery_account_evaluator::do_apply(const change_recovery_account_operation& o) {
            _db.get_account(o.new_recovery_account); // Simply validate account exists
            const auto& account_to_recover = _db.get_account(o.account_to_recover);
            const auto& change_recovery_idx =
                _db.get_index<change_recovery_account_request_index>().indices().get<by_account>();
            auto request = change_recovery_idx.find(o.account_to_recover);

            if (request == change_recovery_idx.end()) {
                // New request
                _db.create<change_recovery_account_request_object>([&](change_recovery_account_request_object& req) {
                    req.account_to_recover = o.account_to_recover;
                    req.recovery_account = o.new_recovery_account;
                    req.effective_on = _db.head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD;
                });
            } else if (account_to_recover.recovery_account != o.new_recovery_account) {
                // Change existing request
                _db.modify(*request, [&](change_recovery_account_request_object& req) {
                    req.recovery_account = o.new_recovery_account;
                    req.effective_on = _db.head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD;
                });
            } else {
                // Request exists and changing back to current recovery account
                _db.remove(*request);
            }
        }

        void transfer_to_savings_evaluator::do_apply(const transfer_to_savings_operation& op) {
            const auto& from = _db.get_account(op.from);
            const auto& to = _db.get_account(op.to);

            GOLOS_CHECK_BALANCE(from, MAIN_BALANCE, op.amount);

            _db.adjust_balance(from, -op.amount);
            _db.adjust_savings_balance(to, op.amount);
        }

        void transfer_from_savings_evaluator::do_apply(const transfer_from_savings_operation& op) {
            const auto& from = _db.get_account(op.from);
            _db.get_account(op.to); // Verify `to` account exists

            GOLOS_CHECK_LOGIC(from.savings_withdraw_requests < STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT,
                golos::logic_exception::reached_limit_for_pending_withdraw_requests,
                "Account has reached limit for pending withdraw requests.",
                ("limit",STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT));

            GOLOS_CHECK_BALANCE(from, SAVINGS, op.amount);
            _db.adjust_savings_balance(from, -op.amount);

            GOLOS_CHECK_OBJECT_MISSING(_db, savings_withdraw, op.from, op.request_id);

            _db.create<savings_withdraw_object>([&](savings_withdraw_object& s) {
                s.from = op.from;
                s.to = op.to;
                s.amount = op.amount;
                if (_db.store_memo_in_savings_withdraws())  {
                    from_string(s.memo, op.memo);
                }
                s.request_id = op.request_id;
                s.complete = _db.head_block_time() + STEEMIT_SAVINGS_WITHDRAW_TIME;
            });
            _db.modify(from, [&](account_object& a) {
                a.savings_withdraw_requests++;
            });
        }

        void cancel_transfer_from_savings_evaluator::do_apply(const cancel_transfer_from_savings_operation& op) {
            const auto& name = op.from;
            const auto& from = _db.get_account(name);
            const auto& swo = _db.get_savings_withdraw(name, op.request_id);
            _db.adjust_savings_balance(from, swo.amount);
            _db.remove(swo);
            _db.modify(from, [&](account_object& a) {
                a.savings_withdraw_requests--;
            });
        }

        void decline_voting_rights_evaluator::do_apply(const decline_voting_rights_operation& o) {
            const auto& account = _db.get_account(o.account);
            const auto& request_idx = _db.get_index<decline_voting_rights_request_index>().indices().get<by_account>();
            auto itr = request_idx.find(account.id);
            auto exist = itr != request_idx.end();

            if (o.decline) {
                if (exist) {
                    GOLOS_THROW_OBJECT_ALREADY_EXIST("decline_voting_rights_request", o.account);
                }
                _db.create<decline_voting_rights_request_object>([&](decline_voting_rights_request_object& req) {
                    req.account = account.id;
                    req.effective_date = _db.head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD;
                });
            } else {
                if (!exist) {
                    GOLOS_THROW_MISSING_OBJECT("decline_voting_rights_request", o.account);
                }
                _db.remove(*itr);
            }
        }

        void reset_account_evaluator::do_apply(const reset_account_operation& op) {
            GOLOS_ASSERT(false,  golos::unsupported_operation, "Reset Account Operation is currently disabled.");
/*          const auto& acnt = _db.get_account(op.account_to_reset);
            auto band = _db.find<account_bandwidth_object, by_account_bandwidth_type>(std::make_tuple(op.account_to_reset, bandwidth_type::old_forum));
            if (band != nullptr)
                FC_ASSERT((_db.head_block_time() - band->last_bandwidth_update) > fc::days(60),
                    "Account must be inactive for 60 days to be eligible for reset");
            FC_ASSERT(acnt.reset_account == op.reset_account, "Reset account does not match reset account on account.");
            _db.update_owner_authority(acnt, op.new_owner_authority);
*/
        }

        void set_reset_account_evaluator::do_apply(const set_reset_account_operation& op) {
            GOLOS_ASSERT(false, golos::unsupported_operation, "Set Reset Account Operation is currently disabled.");
/*          const auto& acnt = _db.get_account(op.account);
            _db.get_account(op.reset_account);

            FC_ASSERT(acnt.reset_account == op.current_reset_account,
                "Current reset account does not match reset account on account.");
            FC_ASSERT(acnt.reset_account != op.reset_account, "Reset account must change");

            _db.modify(acnt, [&](account_object& a) {
                a.reset_account = op.reset_account;
            });
*/
        }

template <typename CreateVdo, typename ValidateWithVdo, typename Operation>
void delegate_vesting_shares(
    database& _db, const chain_properties& median_props, const Operation& op,
    CreateVdo&& create_vdo, ValidateWithVdo&& validate_with_vdo
) {
    const auto& delegator = _db.get_account(op.delegator);
    const auto& delegatee = _db.get_account(op.delegatee);
    auto delegation = _db.find<vesting_delegation_object, by_delegation>(std::make_tuple(op.delegator, op.delegatee));

    if (delegation) {
        validate_with_vdo(*delegation);
    }

    const auto v_share_price = _db.get_dynamic_global_properties().get_vesting_share_price();
    auto min_delegation = median_props.min_delegation * v_share_price;
    auto min_update = median_props.create_account_min_golos_fee * v_share_price;

    auto now = _db.head_block_time();
    auto delta = delegation ?
        op.vesting_shares - delegation->vesting_shares :
        op.vesting_shares;
    auto increasing = delta.amount > 0;

    GOLOS_CHECK_OP_PARAM(op, vesting_shares, {
        GOLOS_CHECK_LOGIC((increasing ? delta : -delta) >= min_update,
            logic_exception::delegation_difference_too_low,
            "Delegation difference is not enough. min_update: ${min}", ("min", min_update));
#ifdef STEEMIT_BUILD_TESTNET
        // min_update depends on account_creation_fee, which can be 0 on testnet
        GOLOS_CHECK_LOGIC(delta.amount != 0,
            logic_exception::delegation_difference_too_low,
            "Delegation difference can't be 0");
#endif
    });

    if (increasing) {
        auto delegated = delegator.delegated_vesting_shares;
        GOLOS_CHECK_BALANCE(delegator, AVAILABLE_VESTING, delta);
        auto elapsed_seconds = (now - delegator.last_vote_time).to_seconds();
        auto regenerated_power = (STEEMIT_100_PERCENT * elapsed_seconds) / STEEMIT_VOTE_REGENERATION_SECONDS;
        auto current_power = std::min<int64_t>(delegator.voting_power + regenerated_power, STEEMIT_100_PERCENT);
        auto max_allowed = (uint128_t(delegator.vesting_shares.amount) * current_power / STEEMIT_100_PERCENT).to_uint64();
        GOLOS_CHECK_LOGIC(delegated + delta <= asset(max_allowed, VESTS_SYMBOL),
            logic_exception::delegation_limited_by_voting_power,
            "Account allowed to delegate a maximum of ${v} with current voting power = ${p}",
            ("v",max_allowed)("p",current_power)("delegated",delegated)("delta",delta));

        if (!delegation) {
            GOLOS_CHECK_OP_PARAM(op, vesting_shares, {
                GOLOS_CHECK_LOGIC(op.vesting_shares >= min_delegation,
                    logic_exception::cannot_delegate_below_minimum,
                    "Account must delegate a minimum of ${v}",
                    ("v",min_delegation)("vesting_shares",op.vesting_shares));
            });
            _db.create<vesting_delegation_object>([&](vesting_delegation_object& o) {
                o.delegator = op.delegator;
                o.delegatee = op.delegatee;
                o.vesting_shares = op.vesting_shares;
                o.min_delegation_time = now;
                create_vdo(o);
            });
        }
        _db.modify(delegator, [&](account_object& a) {
            a.delegated_vesting_shares += delta;
        });
    } else {
        GOLOS_CHECK_OP_PARAM(op, vesting_shares, {
            GOLOS_CHECK_LOGIC(op.vesting_shares.amount == 0 || op.vesting_shares >= min_delegation,
                logic_exception::cannot_delegate_below_minimum,
                "Delegation must be removed or leave minimum delegation amount of ${v}",
                ("v",min_delegation)("vesting_shares",op.vesting_shares));
        });
        _db.create<vesting_delegation_expiration_object>([&](vesting_delegation_expiration_object& o) {
            o.delegator = op.delegator;
            o.vesting_shares = -delta;
            o.expiration = std::max(now + STEEMIT_CASHOUT_WINDOW_SECONDS, delegation->min_delegation_time);
        });
    }

    _db.modify(delegatee, [&](account_object& a) {
        a.received_vesting_shares += delta;
    });
    if (delegation) {
        if (op.vesting_shares.amount > 0) {
            _db.modify(*delegation, [&](vesting_delegation_object& o) {
                o.vesting_shares = op.vesting_shares;
            });
        } else {
            _db.remove(*delegation);
        }
    }
}

        void delegate_vesting_shares_evaluator::do_apply(const delegate_vesting_shares_operation& op) {
            const auto& median_props = _db.get_witness_schedule_object().median_props;

            delegate_vesting_shares(_db, median_props, op, [&](auto&){}, [&](auto&){});
        }

        void break_free_referral_evaluator::do_apply(const break_free_referral_operation& op) {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_19__295, "break_free_referral_operation");

            const auto& referral = _db.get_account(op.referral);
            const auto& referrer = _db.get_account(referral.referrer_account);

            GOLOS_CHECK_LOGIC(referral.referral_break_fee.amount != 0,
                logic_exception::no_right_to_break_referral,
                "This referral account has no right to break referral");

            GOLOS_CHECK_BALANCE(referral, MAIN_BALANCE, referral.referral_break_fee);
            _db.adjust_balance(referral, -referral.referral_break_fee);
            _db.adjust_balance(referrer, referral.referral_break_fee);

            _db.modify(referral, [&](account_object& a) {
                a.referrer_account = account_name_type();
                a.referrer_interest_rate = 0;
                a.referral_end_date = time_point_sec::min();
                a.referral_break_fee.amount = 0;
            });
        }

        void delegate_vesting_shares_with_interest_evaluator::do_apply(const delegate_vesting_shares_with_interest_operation& op) {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_19__756, "delegate_vesting_shares_with_interest_operation");

            const auto& median_props = _db.get_witness_schedule_object().median_props;

            GOLOS_CHECK_LIMIT_PARAM(op.interest_rate, median_props.max_delegated_vesting_interest_rate);

            delegate_vesting_shares(_db, median_props, op, [&](auto& o) {
                o.interest_rate = op.interest_rate;
                o.payout_strategy = op.payout_strategy;
            }, [&](auto& o) {
                GOLOS_CHECK_LOGIC(o.interest_rate == op.interest_rate,
                    logic_exception::cannot_change_delegator_interest_rate,
                    "Cannot change interest rate of already created delegation");
                GOLOS_CHECK_LOGIC(o.payout_strategy == op.payout_strategy,
                    logic_exception::cannot_change_delegator_payout_strategy,
                    "Cannot change payout strategy of already created delegation");
            });
        }

        void reject_vesting_shares_delegation_evaluator::do_apply(const reject_vesting_shares_delegation_operation& op) {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_19__756, "reject_vesting_shares_delegation_operation");

            const auto& delegatee = _db.get_account(op.delegatee);

            auto delegation = _db.find<vesting_delegation_object, by_delegation>(std::make_tuple(op.delegator, op.delegatee));

            if (delegation == nullptr) {
                GOLOS_THROW_MISSING_OBJECT("vesting_delegation_object", fc::mutable_variant_object()("delegator",op.delegator)("delegatee",op.delegatee));
            }

            auto now = _db.head_block_time();

            _db.modify(delegatee, [&](account_object& a) {
                a.received_vesting_shares -= delegation->vesting_shares;
            });

            _db.create<vesting_delegation_expiration_object>([&](vesting_delegation_expiration_object& o) {
                o.delegator = op.delegator;
                o.vesting_shares = delegation->vesting_shares;
                o.expiration = std::max(now + STEEMIT_CASHOUT_WINDOW_SECONDS, delegation->min_delegation_time);
            });

            _db.remove(*delegation);
        }

} } // golos::chain
