#include <golos/chain/evaluators/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/database_exceptions.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>
#include <golos/chain/objects/steem_objects.hpp>
#include <golos/chain/objects/block_summary_object.hpp>

#include <golos/chain/utilities/reward.hpp>

#ifndef STEEMIT_BUILD_LOW_MEMORY

#include <diff_match_patch.h>
#include <boost/locale/encoding_utf.hpp>

using boost::locale::conv::utf_to_utf;

std::wstring utf8_to_wstring(const std::string &str) {
    return utf_to_utf<wchar_t>(str.c_str(), str.c_str() + str.size());
}

std::string wstring_to_utf8(const std::wstring &str) {
    return utf_to_utf<char>(str.c_str(), str.c_str() + str.size());
}

#endif

namespace golos {
    namespace chain {
        using fc::uint128_t;

        inline void validate_permlink_0_1(const string &permlink) {
            FC_ASSERT(permlink.size() > STEEMIT_MIN_PERMLINK_LENGTH && permlink.size() < STEEMIT_MAX_PERMLINK_LENGTH,
                      "Permlink is not a valid size.");

            FC_ASSERT(std::find_if_not(permlink.begin(), permlink.end(), [&](std::string::value_type c) -> bool {
                return std::isdigit(c) || std::islower(c) || c == '-';
            }) == permlink.end(), "Invalid permlink character");
        }

        struct strcmp_equal {
            bool operator()(const shared_string &a, const string &b) {
                return a.size() == b.size() || std::strcmp(a.c_str(), b.c_str()) == 0;
            }
        };

        /**
         *  Because net_rshares is 0 there is no need to update any pending payout calculations or parent posts.
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void delete_comment_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_10)) {
                const auto &auth = this->db.get_account(o.author);
                FC_ASSERT(!(auth.owner_challenged || auth.active_challenged),
                          "Operation cannot be processed because account is currently challenged.");
            }

            const auto &comment = this->db.get_comment(o.author, o.permlink);
            FC_ASSERT(comment.children == 0, "Cannot delete a comment with replies.");

            if (this->db.is_producing()) {
                FC_ASSERT(comment.net_rshares <= 0, "Cannot delete a comment with net positive votes.");
            }
            if (comment.net_rshares > 0) {
                return;
            }

            const auto &vote_idx = this->db.template get_index<comment_vote_index>().indices().
                    template get<by_comment_voter>();

            auto vote_itr = vote_idx.lower_bound(comment_object::id_type(comment.id));
            while (vote_itr != vote_idx.end() && vote_itr->comment == comment.id) {
                const auto &cur_vote = *vote_itr;
                ++vote_itr;
                this->db.remove(cur_vote);
            }

            /// this loop can be skiped for validate-only nodes as it is merely gathering stats for indicies
            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_6__80) && comment.parent_author != STEEMIT_ROOT_POST_PARENT) {
                auto parent = &this->db.get_comment(comment.parent_author, comment.parent_permlink);
                auto now = this->db.head_block_time();
                while (parent) {
                    this->db.modify(*parent, [&](comment_object &p) {
                        p.children--;
                        p.active = now;
                    });
#ifndef STEEMIT_BUILD_LOW_MEMORY
                    if (parent->parent_author != STEEMIT_ROOT_POST_PARENT) {
                        parent = &this->db.get_comment(parent->parent_author, parent->parent_permlink);
                    } else
#endif
                    {
                        parent = nullptr;
                    }
                }
            }

            /** TODO move category behavior to a plugin, this is not part of consensus */
            const category_object *cat = this->db.find_category(comment.category);
            this->db.modify(*cat, [&](category_object &c) {
                c.discussions--;
                c.last_update = this->db.head_block_time();
            });

            this->db.remove(comment);
        }

        struct comment_options_extension_visitor {
            comment_options_extension_visitor(const comment_object &c, database &input_db) : _c(c), db(input_db) {
            }

            typedef void result_type;

            const comment_object &_c;
            database &db;

            void operator()(const comment_payout_beneficiaries &cpb) const {
                if (this->db.is_producing()) {
                    FC_ASSERT(cpb.beneficiaries.size() <= 8, "Cannot specify more than 8 beneficiaries.");
                }

                FC_ASSERT(_c.beneficiaries.size() == 0, "Comment already has beneficiaries specified.");
                FC_ASSERT(_c.abs_rshares == 0, "Comment must not have been voted on before specifying beneficiaries.");

                this->db.modify(_c, [&](comment_object &c) {
                    for (auto &b : cpb.beneficiaries) {
                        auto acc = this->db.template find<account_object, by_name>(b.account);
                        FC_ASSERT(acc != nullptr, "Beneficiary \"${a}\" must exist.", ("a", b.account));
                        c.beneficiaries.push_back(b);
                    }
                });
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void comment_options_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_10)) {
                const auto &auth = this->db.get_account(o.author);
                FC_ASSERT(!(auth.owner_challenged || auth.active_challenged),
                          "Operation cannot be processed because account is currently challenged.");
            }

            const auto &comment = this->db.get_comment(o.author, o.permlink);
            if (!o.allow_curation_rewards || !o.allow_votes || o.max_accepted_payout < comment.max_accepted_payout) {
                FC_ASSERT(comment.abs_rshares == 0,
                          "One of the included comment options requires the comment to have no rshares allocated to it.");
            }

            if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__102)) { // TODO: Remove after hardfork 17
                FC_ASSERT(o.extensions.size() == 0,
                          "Operation extensions for the comment_options_operation are not currently supported.");
            }

            FC_ASSERT(comment.allow_curation_rewards >= o.allow_curation_rewards,
                      "Curation rewards cannot be re-enabled.");
            FC_ASSERT(comment.allow_votes >= o.allow_votes, "Voting cannot be re-enabled.");
            FC_ASSERT(comment.max_accepted_payout >= o.max_accepted_payout,
                      "A comment cannot accept a greater payout.");
            FC_ASSERT(comment.percent_steem_dollars >= o.percent_steem_dollars,
                      "A comment cannot accept a greater percent SBD.");

            this->db.modify(comment, [&](comment_object &c) {
                c.max_accepted_payout = asset<0, 17, 0>(o.max_accepted_payout.amount,
                                                        o.max_accepted_payout.symbol_name());
                c.percent_steem_dollars = o.percent_steem_dollars;
                c.allow_votes = o.allow_votes;
                c.allow_curation_rewards = o.allow_curation_rewards;
            });

            for (auto &e : o.extensions) {
                e.visit(comment_options_extension_visitor(comment, this->db));
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void comment_payout_extension_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            const account_object &from_account = this->db.get_account(o.payer);

            if (from_account.active_challenged) {
                this->db.modify(from_account, [&](account_object &a) {
                    a.active_challenged = false;
                    a.last_active_proved = this->db.head_block_time();
                });
            }

            const comment_object &comment = this->db.get_comment(o.author, o.permlink);

            if (o.amount) {
                FC_ASSERT(this->db.get_balance(from_account, o.amount->symbol) >= *o.amount,
                          "Account does not have sufficient funds for transfer.");

                this->db.pay_fee(from_account, *o.amount);

                this->db.modify(comment, [&](comment_object &c) {
                    c.cashout_time = this->db.get_payout_extension_time(comment, *o.amount);
                });
            } else if (o.extension_time) {
                asset<0, 17, 0> amount = this->db.get_payout_extension_cost(comment, *o.extension_time);

                FC_ASSERT(this->db.get_balance(from_account, o.amount->symbol) >= amount,
                          "Account does not have sufficient funds for transfer.");

                this->db.pay_fee(from_account, amount);

                this->db.modify(comment, [&](comment_object &c) {
                    c.cashout_time = *o.extension_time;
                });
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void comment_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                if (this->db.is_producing() || this->db.has_hardfork(STEEMIT_HARDFORK_0_5__55)) {
                    FC_ASSERT(o.title.size() + o.body.size() + o.json_metadata.size(),
                              "Cannot update comment because nothing appears to be changing.");
                }

                const auto &by_permlink_idx = this->db.template get_index<comment_index>().indices().
                        template get<by_permlink>();
                auto itr = by_permlink_idx.find(boost::make_tuple(o.author, o.permlink));

                const auto &auth = this->db.get_account(o.author); /// prove it exists

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_10)) {
                    FC_ASSERT(!(auth.owner_challenged || auth.active_challenged),
                              "Operation cannot be processed because account is currently challenged.");
                }

                comment_object::id_type id;

                const comment_object *parent = nullptr;
                if (o.parent_author != STEEMIT_ROOT_POST_PARENT) {
                    parent = &this->db.get_comment(o.parent_author, o.parent_permlink);
                    if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__84)) {
                        FC_ASSERT(parent->depth < STEEMIT_MAX_COMMENT_DEPTH_PRE_HF17,
                                  "Comment is nested ${x} posts deep, maximum depth is ${y}.",
                                  ("x", parent->depth)("y", STEEMIT_MAX_COMMENT_DEPTH_PRE_HF17));
                    } else if (this->db.is_producing()) {
                        FC_ASSERT(parent->depth < STEEMIT_SOFT_MAX_COMMENT_DEPTH,
                                  "Comment is nested ${x} posts deep, maximum depth is ${y}.",
                                  ("x", parent->depth)("y", STEEMIT_SOFT_MAX_COMMENT_DEPTH));
                    } else {
                        FC_ASSERT(parent->depth < STEEMIT_MAX_COMMENT_DEPTH,
                                  "Comment is nested ${x} posts deep, maximum depth is ${y}.",
                                  ("x", parent->depth)("y", STEEMIT_MAX_COMMENT_DEPTH));
                    }

                }

                if ((this->db.is_producing() || this->db.has_hardfork(STEEMIT_HARDFORK_0_17)) &&
                    o.json_metadata.size()) {
                    FC_ASSERT(fc::is_utf8(o.json_metadata), "JSON Metadata must be UTF-8");
                }

                auto now = this->db.head_block_time();

                if (itr == by_permlink_idx.end()) {
                    if (o.parent_author != STEEMIT_ROOT_POST_PARENT) {
                        FC_ASSERT(this->db.get(parent->root_comment).allow_replies,
                                  "The parent comment has disabled replies.");
                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                            !this->db.has_hardfork(STEEMIT_HARDFORK_0_17__97)) {
                            FC_ASSERT(
                                    this->db.calculate_discussion_payout_time(*parent) != fc::time_point_sec::maximum(),
                                    "Discussion is frozen.");
                        }
                    }

                    auto band = this->db.template find<account_bandwidth_object, by_account_bandwidth_type>(
                            boost::make_tuple(o.author, bandwidth_type::post));

                    if (band == nullptr) {
                        band = &this->db.template create<account_bandwidth_object>([&](account_bandwidth_object &b) {
                            b.account = o.author;
                            b.type = bandwidth_type::post;
                        });
                    }

                    if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__176)) {
                        if (o.parent_author == STEEMIT_ROOT_POST_PARENT) {
                            FC_ASSERT((now - band->last_bandwidth_update) > STEEMIT_MIN_ROOT_COMMENT_INTERVAL,
                                      "You may only post once every 5 minutes.",
                                      ("now", now)("last_root_post", band->last_bandwidth_update));
                        } else {
                            FC_ASSERT((now - auth.last_post) > STEEMIT_MIN_REPLY_INTERVAL,
                                      "You may only comment once every 20 seconds.",
                                      ("now", now)("auth.last_post", auth.last_post));
                        }
                    } else if (this->db.has_hardfork(STEEMIT_HARDFORK_0_6__113)) {
                        if (o.parent_author == STEEMIT_ROOT_POST_PARENT) {
                            FC_ASSERT((now - auth.last_post) > STEEMIT_MIN_ROOT_COMMENT_INTERVAL,
                                      "You may only post once every 5 minutes.",
                                      ("now", now)("auth.last_post", auth.last_post));
                        } else {
                            FC_ASSERT((now - auth.last_post) > STEEMIT_MIN_REPLY_INTERVAL,
                                      "You may only comment once every 20 seconds.",
                                      ("now", now)("auth.last_post", auth.last_post));
                        }
                    } else {
                        FC_ASSERT((now - auth.last_post) > fc::seconds(60), "You may only post once per minute.",
                                  ("now", now)("auth.last_post", auth.last_post));
                    }

                    uint16_t reward_weight = STEEMIT_100_PERCENT;

                    if (o.parent_author == STEEMIT_ROOT_POST_PARENT) {
                        auto post_bandwidth = band->average_bandwidth;

                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_17__78)) {
                            auto post_delta_time = std::min(
                                    now.sec_since_epoch() - band->last_bandwidth_update.sec_since_epoch(),
                                    STEEMIT_POST_AVERAGE_WINDOW);
                            auto old_weight = (post_bandwidth * (STEEMIT_POST_AVERAGE_WINDOW - post_delta_time)) /
                                              STEEMIT_POST_AVERAGE_WINDOW;
                            post_bandwidth = (old_weight + STEEMIT_100_PERCENT);
                            reward_weight = uint16_t(std::min((STEEMIT_POST_WEIGHT_CONSTANT * STEEMIT_100_PERCENT) /
                                                              (post_bandwidth.value * post_bandwidth.value),
                                                              uint64_t(STEEMIT_100_PERCENT)));
                        } else if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__176)) {
                            auto post_delta_time = std::min(
                                    now.sec_since_epoch() - band->last_bandwidth_update.sec_since_epoch(),
                                    STEEMIT_POST_AVERAGE_WINDOW);
                            auto old_weight = (post_bandwidth * (STEEMIT_POST_AVERAGE_WINDOW - post_delta_time)) /
                                              STEEMIT_POST_AVERAGE_WINDOW;
                            post_bandwidth = (old_weight + STEEMIT_100_PERCENT);
                            reward_weight = uint16_t(std::min(
                                    (STEEMIT_POST_WEIGHT_CONSTANT_PRE_HF17 * STEEMIT_100_PERCENT) /
                                    (post_bandwidth.value * post_bandwidth.value), uint64_t(STEEMIT_100_PERCENT)));
                        }

                        this->db.modify(*band, [&](account_bandwidth_object &b) {
                            b.last_bandwidth_update = now;
                            b.average_bandwidth = post_bandwidth;
                        });
                    }

                    this->db.modify(auth, [&](account_object &a) {
                        a.last_post = now;
                        a.post_count++;
                    });

                    const auto &new_comment = this->db.template create<comment_object>([&](comment_object &com) {
                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                            validate_permlink_0_1(o.parent_permlink);
                            validate_permlink_0_1(o.permlink);
                        }

                        com.author = o.author;
                        from_string(com.permlink, o.permlink);
                        com.last_update = this->db.head_block_time();
                        com.created = com.last_update;
                        com.active = com.last_update;
                        com.last_payout = fc::time_point_sec::min();
                        com.max_cashout_time = fc::time_point_sec::maximum();
                        com.reward_weight = reward_weight;

                        if (o.parent_author == STEEMIT_ROOT_POST_PARENT) {
                            com.parent_author = "";
                            from_string(com.parent_permlink, o.parent_permlink);
                            from_string(com.category, o.parent_permlink);
                            com.root_comment = com.id;
                            com.cashout_time = this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177) ?
                                               this->db.head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17
                                                                                                 : fc::time_point_sec::maximum();
                        } else {
                            com.parent_author = parent->author;
                            com.parent_permlink = parent->permlink;
                            com.depth = parent->depth + 1;
                            com.category = parent->category;
                            com.root_comment = parent->root_comment;
                            com.cashout_time = fc::time_point_sec::maximum();
                        }

                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_17__91)) {
                            com.cashout_time = com.created + STEEMIT_CASHOUT_WINDOW_SECONDS;
                        }

#ifndef STEEMIT_BUILD_LOW_MEMORY
                        from_string(com.title, o.title);
                        if (o.body.size() < 1024 * 1024 * 128) {
                            from_string(com.body, o.body);
                        }
                        if (fc::is_utf8(o.json_metadata)) {
                            from_string(com.json_metadata, o.json_metadata);
                        } else {
                            wlog("Comment ${a}/${p} contains invalid UTF-8 metadata", ("a", o.author)("p", o.permlink));
                        }
#endif
                    });

                    /** TODO move category behavior to a plugin, this is not part of consensus */
                    const category_object *cat = this->db.find_category(new_comment.category);
                    if (!cat) {
                        cat = &this->db.template create<category_object>([&](category_object &c) {
                            c.name = new_comment.category;
                            c.discussions = 1;
                            c.last_update = this->db.head_block_time();
                        });
                    } else {
                        this->db.modify(*cat, [&](category_object &c) {
                            c.discussions++;
                            c.last_update = this->db.head_block_time();
                        });
                    }

                    id = new_comment.id;

                    /// this loop can be skiped for validate-only nodes as it is merely gathering stats for indicies
                    auto now = this->db.head_block_time();
                    while (parent) {
                        this->db.modify(*parent, [&](comment_object &p) {
                            p.children++;
                            p.active = now;
                        });
#ifndef STEEMIT_BUILD_LOW_MEMORY
                        if (parent->parent_author != STEEMIT_ROOT_POST_PARENT) {
                            parent = &this->db.get_comment(parent->parent_author, parent->parent_permlink);
                        } else
#endif
                        {
                            parent = nullptr;
                        }
                    }

                } else // start edit case
                {
                    const auto &comment = *itr;

                    if (this->db.has_hardfork(STEEMIT_HARDFORK_0_17__85)) {
                        // This will be moved to the witness plugin in a later release
                        if (this->db.is_producing()) {
                            // For now, use the same editting rules, but implement it as a soft fork.
                            FC_ASSERT(
                                    this->db.calculate_discussion_payout_time(comment) != fc::time_point_sec::maximum(),
                                    "The comment is archived.");
                        }
                    } else if (this->db.has_hardfork(STEEMIT_HARDFORK_0_14__306)) {
                        FC_ASSERT(this->db.calculate_discussion_payout_time(comment) != fc::time_point_sec::maximum(),
                                  "The comment is archived.");
                    } else if (this->db.has_hardfork(STEEMIT_HARDFORK_0_10)) {
                        FC_ASSERT(comment.last_payout == fc::time_point_sec::min(),
                                  "Can only edit during the first 24 hours.");
                    }

                    this->db.modify(comment, [&](comment_object &com) {
                        com.last_update = this->db.head_block_time();
                        com.active = com.last_update;
                        strcmp_equal equal;

                        if (!parent) {
                            FC_ASSERT(com.parent_author == account_name_type(),
                                      "The parent of a comment cannot change.");
                            FC_ASSERT(equal(com.parent_permlink, o.parent_permlink),
                                      "The permlink of a comment cannot change.");
                        } else {
                            FC_ASSERT(com.parent_author == o.parent_author, "The parent of a comment cannot change.");
                            FC_ASSERT(equal(com.parent_permlink, o.parent_permlink),
                                      "The permlink of a comment cannot change.");
                        }

#ifndef STEEMIT_BUILD_LOW_MEMORY
                        if (o.title.size()) {
                            from_string(com.title, o.title);
                        }
                        if (o.json_metadata.size()) {
                            if (fc::is_utf8(o.json_metadata)) {
                                from_string(com.json_metadata, o.json_metadata);
                            } else {
                                wlog("Comment ${a}/${p} contains invalid UTF-8 metadata",
                                     ("a", o.author)("p", o.permlink));
                            }
                        }

                        if (o.body.size()) {
                            try {
                                diff_match_patch<std::wstring> dmp;
                                auto patch = dmp.patch_fromText(utf8_to_wstring(o.body));
                                if (patch.size()) {
                                    auto result = dmp.patch_apply(patch, utf8_to_wstring(to_string(com.body)));
                                    auto patched_body = wstring_to_utf8(result.first);
                                    if (!fc::is_utf8(patched_body)) {
                                        idump(("invalid utf8")(patched_body));
                                        from_string(com.body, fc::prune_invalid_utf8(patched_body));
                                    } else {
                                        from_string(com.body, patched_body);
                                    }
                                } else { // replace
                                    from_string(com.body, o.body);
                                }
                            } catch (...) {
                                from_string(com.body, o.body);
                            }
                        }
#endif
                    });

                } // end EDIT case

            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void withdraw_vesting_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            const auto &account = this->db.get_account(o.account);

            asset<Major, Hardfork, Release> default_vests(0, VESTS_SYMBOL);

            FC_ASSERT(account.vesting_shares >= default_vests,
                      "Account does not have sufficient Golos Power for withdraw.");
            FC_ASSERT(account.vesting_shares - account.delegated_vesting_shares >= o.vesting_shares,
                      "Account does not have sufficient Golos Power for withdraw. Vesting amount: ${a}. Vesting required: ${r}",
                      ("a", account.vesting_shares - account.delegated_vesting_shares)("r", o.vesting_shares));

            if (!account.mined && this->db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                const auto &props = this->db.get_dynamic_global_properties();
                const witness_schedule_object &wso = this->db.get_witness_schedule_object();

                asset<0, 17, 0> min_vests = wso.median_props.account_creation_fee * props.get_vesting_share_price();
                min_vests.amount.value *= 10;

                FC_ASSERT(account.vesting_shares > min_vests ||
                          (this->db.has_hardfork(STEEMIT_HARDFORK_0_16__562) && o.vesting_shares.amount == 0),
                          "Account registered by another account requires 10x account creation fee worth of Golos Power before it can be powered down.");
            }

            if (o.vesting_shares.amount == 0) {
                if (this->db.is_producing() || this->db.has_hardfork(STEEMIT_HARDFORK_0_5__57)) {
                    FC_ASSERT(account.vesting_withdraw_rate.amount != 0,
                              "This operation would not change the vesting withdraw rate.");
                }

                this->db.modify(account, [&](account_object &a) {
                    a.vesting_withdraw_rate = asset<0, 17, 0>(0, VESTS_SYMBOL);
                    a.next_vesting_withdrawal = time_point_sec::maximum();
                    a.to_withdraw = 0;
                    a.withdrawn = 0;
                });
            } else {
                int vesting_withdraw_intervals = 0;

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_17__103)) {
                    vesting_withdraw_intervals = STEEMIT_VESTING_WITHDRAW_INTERVALS;
                } else if (this->db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                    vesting_withdraw_intervals = STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF17;
                } else {
                    vesting_withdraw_intervals = STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF16;
                }

                this->db.modify(account, [&](account_object &a) {
                    asset<0, 17, 0> new_vesting_withdraw_rate = asset<0, 17, 0>(
                            o.vesting_shares.amount / vesting_withdraw_intervals, VESTS_SYMBOL);

                    if (new_vesting_withdraw_rate.amount == 0) {
                        new_vesting_withdraw_rate.amount = 1;
                    }

                    if (this->db.is_producing() || this->db.has_hardfork(STEEMIT_HARDFORK_0_5__57)) {
                        FC_ASSERT(account.vesting_withdraw_rate != new_vesting_withdraw_rate,
                                  "This operation would not change the vesting withdraw rate.");
                    }

                    a.vesting_withdraw_rate = new_vesting_withdraw_rate;
                    a.next_vesting_withdrawal =
                            this->db.head_block_time() + fc::seconds(STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS);
                    a.to_withdraw = o.vesting_shares.amount;
                    a.withdrawn = 0;
                });
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void set_withdraw_vesting_route_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {

                const auto &from_account = this->db.get_account(o.from_account);
                const auto &to_account = this->db.get_account(o.to_account);
                const auto &wd_idx = this->db.template get_index<withdraw_vesting_route_index>().indices().
                        template get<by_withdraw_route>();
                auto itr = wd_idx.find(boost::make_tuple(from_account.id, to_account.id));

                if (itr == wd_idx.end()) {
                    FC_ASSERT(o.percent != 0, "Cannot create a 0% destination.");
                    FC_ASSERT(from_account.withdraw_routes < STEEMIT_MAX_WITHDRAW_ROUTES,
                              "Account already has the maximum number of routes.");

                    this->db.template create<withdraw_vesting_route_object>([&](withdraw_vesting_route_object &wvdo) {
                        wvdo.from_account = from_account.id;
                        wvdo.to_account = to_account.id;
                        wvdo.percent = o.percent;
                        wvdo.auto_vest = o.auto_vest;
                    });

                    this->db.modify(from_account, [&](account_object &a) {
                        a.withdraw_routes++;
                    });
                } else if (o.percent == 0) {
                    this->db.remove(*itr);

                    this->db.modify(from_account, [&](account_object &a) {
                        a.withdraw_routes--;
                    });
                } else {
                    this->db.modify(*itr, [&](withdraw_vesting_route_object &wvdo) {
                        wvdo.from_account = from_account.id;
                        wvdo.to_account = to_account.id;
                        wvdo.percent = o.percent;
                        wvdo.auto_vest = o.auto_vest;
                    });
                }

                itr = wd_idx.upper_bound(boost::make_tuple(from_account.id, account_object::id_type()));
                uint16_t total_percent = 0;

                while (itr->from_account == from_account.id && itr != wd_idx.end()) {
                    total_percent += itr->percent;
                    ++itr;
                }

                FC_ASSERT(total_percent <= STEEMIT_100_PERCENT,
                          "More than 100% of vesting withdrawals allocated to destinations.");
            } FC_CAPTURE_AND_RETHROW()
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void vote_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            try {
                const auto &comment = this->db.get_comment(o.author, o.permlink);
                const auto &voter = this->db.get_account(o.voter);

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_10))
                    FC_ASSERT(!(voter.owner_challenged || voter.active_challenged),
                              "Operation cannot be processed because the account is currently challenged.");

                FC_ASSERT(voter.can_vote, "Voter has declined their voting rights.");

                if (o.weight > 0)
                    FC_ASSERT(comment.allow_votes, "Votes are not allowed on the comment.");

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                    this->db.calculate_discussion_payout_time(comment) == fc::time_point_sec::maximum()) {
#ifndef CLEAR_VOTES
                    const auto &comment_vote_idx = this->db.template get_index<comment_vote_index>().indices().
                            template get<by_comment_voter>();
                    auto itr = comment_vote_idx.find(std::make_tuple(comment.id, voter.id));

                    if (itr == comment_vote_idx.end()) {
                        this->db.template create<comment_vote_object>([&](comment_vote_object &cvo) {
                            cvo.voter = voter.id;
                            cvo.comment = comment.id;
                            cvo.vote_percent = o.weight;
                            cvo.last_update = this->db.head_block_time();
                        });
                    } else {
                        this->db.modify(*itr, [&](comment_vote_object &cvo) {
                            cvo.vote_percent = o.weight;
                            cvo.last_update = this->db.head_block_time();
                        });
                    }
#endif
                    return;
                }

                const auto &comment_vote_idx = this->db.template get_index<comment_vote_index>().indices().
                        template get<by_comment_voter>();
                auto itr = comment_vote_idx.find(std::make_tuple(comment.id, voter.id));

                int64_t elapsed_seconds = (this->db.head_block_time() - voter.last_vote_time).to_seconds();

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_11))
                    FC_ASSERT(elapsed_seconds >= STEEMIT_MIN_VOTE_INTERVAL_SEC, "Can only vote once every 3 seconds.");

                int64_t regenerated_power = (STEEMIT_100_PERCENT * elapsed_seconds) / STEEMIT_VOTE_REGENERATION_SECONDS;
                int64_t current_power = std::min(int64_t(voter.voting_power + regenerated_power),
                                                 int64_t(STEEMIT_100_PERCENT));
                FC_ASSERT(current_power > 0, "Account currently does not have voting power.");

                int64_t abs_weight = abs(o.weight);
                int64_t used_power = (current_power * abs_weight) / STEEMIT_100_PERCENT;

                const dynamic_global_property_object &dgpo = this->db.get_dynamic_global_properties();

                // used_power = (current_power * abs_weight / STEEMIT_100_PERCENT) * (reserve / max_vote_denom)
                // The second multiplication is rounded up as of HF 259
                int64_t max_vote_denom =
                        dgpo.vote_regeneration_per_day * STEEMIT_VOTE_REGENERATION_SECONDS / (60 * 60 * 24);
                FC_ASSERT(max_vote_denom > 0);

                if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_14__259)) {
                    FC_ASSERT(max_vote_denom == 200);   // TODO: Remove this assert
                    used_power = (used_power / max_vote_denom) + 1;
                } else {
                    used_power = (used_power + max_vote_denom - 1) / max_vote_denom;
                }
                FC_ASSERT(used_power <= current_power, "Account does not have enough power to vote.");

                int64_t abs_rshares = ((uint128_t(voter.effective_vesting_shares().amount.value) * used_power) /
                                       (STEEMIT_100_PERCENT)).to_uint64();
                if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_14__259) && abs_rshares == 0) {
                    abs_rshares = 1;
                }

                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_14__259)) {
                    FC_ASSERT(abs_rshares > STEEMIT_VOTE_DUST_THRESHOLD || o.weight == 0,
                              "Voting weight is too small, please accumulate more voting power or steem power. Rshares: ${r}",
                              ("r", abs_rshares));
                } else if (this->db.has_hardfork(STEEMIT_HARDFORK_0_13__248)) {
                    FC_ASSERT(abs_rshares > STEEMIT_VOTE_DUST_THRESHOLD || abs_rshares == 1,
                              "Voting weight is too small, please accumulate more voting power or steem power. Rshares: ${r}",
                              ("r", abs_rshares));
                }

                // Lazily delete vote
                if (itr != comment_vote_idx.end() && itr->num_changes == -1) {
                    FC_ASSERT(!(this->db.is_producing() || this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177)),
                              "Cannot vote again on a comment after payout.");

                    this->db.remove(*itr);
                    itr = comment_vote_idx.end();
                }

                if (itr == comment_vote_idx.end()) {
                    FC_ASSERT(o.weight != 0, "Vote weight cannot be 0.");
                    /// this is the rshares voting for or against the post
                    int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;

                    if (rshares > 0 && this->db.has_hardfork(STEEMIT_HARDFORK_0_7)) {
                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_17__91))
                            FC_ASSERT(this->db.head_block_time() < comment.cashout_time - STEEMIT_UPVOTE_LOCKOUT,
                                      "Cannot increase reward of post within the last minute before payout.");
                        else
                            FC_ASSERT(this->db.head_block_time() <
                                      this->db.calculate_discussion_payout_time(comment) - STEEMIT_UPVOTE_LOCKOUT,
                                      "Cannot increase reward of post within the last minute before payout.");
                    }

                    //used_power /= (50*7); /// a 100% vote means use .28% of voting power which should force users to spread their votes around over 50+ posts day for a week
                    //if( used_power == 0 ) used_power = 1;

                    this->db.modify(voter, [&](account_object &a) {
                        a.voting_power = current_power - used_power;
                        a.last_vote_time = this->db.head_block_time();
                    });

                    /// if the current net_rshares is less than 0, the post is getting 0 rewards so it is not factored into total rshares^2
                    fc::uint128_t old_rshares = std::max(comment.net_rshares.value, int64_t(0));
                    const auto &root = this->db.get(comment.root_comment);
                    auto old_root_abs_rshares = root.children_abs_rshares.value;

                    fc::uint128_t avg_cashout_sec;

                    if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__91)) {
                        fc::uint128_t cur_cashout_time_sec = this->db.calculate_discussion_payout_time(
                                comment).sec_since_epoch();
                        fc::uint128_t new_cashout_time_sec;

                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                            !this->db.has_hardfork(STEEMIT_HARDFORK_0_13__257)) {
                            new_cashout_time_sec = this->db.head_block_time().sec_since_epoch() +
                                                   STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17;
                        } else {
                            new_cashout_time_sec = this->db.head_block_time().sec_since_epoch() +
                                                   STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF12;
                        }

                        avg_cashout_sec =
                                (cur_cashout_time_sec * old_root_abs_rshares + new_cashout_time_sec * abs_rshares) /
                                (old_root_abs_rshares + abs_rshares);
                    }

                    FC_ASSERT(abs_rshares > 0, "Cannot vote with 0 rshares.");

                    auto old_vote_rshares = comment.vote_rshares;

                    this->db.modify(comment, [&](comment_object &c) {
                        c.net_rshares += rshares;
                        c.abs_rshares += abs_rshares;
                        if (rshares > 0) {
                            c.vote_rshares += rshares;
                        }
                        if (rshares > 0) {
                            c.net_votes++;
                        } else {
                            c.net_votes--;
                        }
                        if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_6__114) && c.net_rshares == -c.abs_rshares)
                            FC_ASSERT(c.net_votes < 0, "Comment has negative net votes?");
                    });

                    this->db.modify(root, [&](comment_object &c) {
                        c.children_abs_rshares += abs_rshares;

                        if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__91)) {
                            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                                c.last_payout > fc::time_point_sec::min()) {
                                c.cashout_time = c.last_payout + STEEMIT_SECOND_CASHOUT_WINDOW;
                            } else {
                                c.cashout_time = fc::time_point_sec(std::min(uint32_t(avg_cashout_sec.to_uint64()),
                                                                             c.max_cashout_time.sec_since_epoch()));
                            }

                            if (c.max_cashout_time == fc::time_point_sec::maximum()) {
                                c.max_cashout_time =
                                        this->db.head_block_time() + fc::seconds(STEEMIT_MAX_CASHOUT_WINDOW_SECONDS);
                            }
                        }
                    });

                    fc::uint128_t new_rshares = std::max(comment.net_rshares.value, int64_t(0));

                    /// calculate rshares2 value
                    new_rshares = utilities::calculate_claims(new_rshares);
                    old_rshares = utilities::calculate_claims(old_rshares);

                    const auto &cat = this->db.get_category(comment.category);
                    this->db.modify(cat, [&](category_object &c) {
                        c.abs_rshares += abs_rshares;
                        c.last_update = this->db.head_block_time();
                    });

                    uint64_t max_vote_weight = 0;

                    /** this verifies uniqueness of voter
                     *
                     *  cv.weight / c.total_vote_weight ==> % of rshares increase that is accounted for by the vote
                     *
                     *  W(R) = B * R / ( R + 2S )
                     *  W(R) is bounded above by B. B is fixed at 2^64 - 1, so all weights fit in a 64 bit integer.
                     *
                     *  The equation for an individual vote is:
                     *    W(R_N) - W(R_N-1), which is the delta increase of proportional weight
                     *
                     *  c.total_vote_weight =
                     *    W(R_1) - W(R_0) +
                     *    W(R_2) - W(R_1) + ...
                     *    W(R_N) - W(R_N-1) = W(R_N) - W(R_0)
                     *
                     *  Since W(R_0) = 0, c.total_vote_weight is also bounded above by B and will always fit in a 64 bit integer.
                     *
                     */
                    this->db.template create<comment_vote_object>([&](comment_vote_object &cv) {
                        cv.voter = voter.id;
                        cv.comment = comment.id;
                        cv.rshares = rshares;
                        cv.vote_percent = o.weight;
                        cv.last_update = this->db.head_block_time();

                        bool curation_reward_eligible = rshares > 0 && (comment.last_payout == fc::time_point_sec()) &&
                                                        comment.allow_curation_rewards;

                        if (curation_reward_eligible && this->db.has_hardfork(STEEMIT_HARDFORK_0_17__86)) {
                            curation_reward_eligible = this->db.get_curation_rewards_percent(comment) > 0;
                        }

                        if (curation_reward_eligible) {
                            if (comment.created < fc::time_point_sec(STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME)) {
                                boost::multiprecision::uint512_t rshares3(rshares);
                                boost::multiprecision::uint256_t total2(comment.abs_rshares.value);

                                if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                                    rshares3 *= 10000;
                                    total2 *= 10000;
                                }

                                rshares3 = rshares3 * rshares3 * rshares3;

                                total2 *= total2;
                                cv.weight = static_cast<uint64_t>( rshares3 / total2 );
                            } else {// cv.weight = W(R_1) - W(R_0)
                                const uint128_t two_s = 2 * utilities::get_content_constant_s();
                                if (this->db.has_hardfork(STEEMIT_HARDFORK_0_17__86)) {
                                    const auto &reward_fund = this->db.get_reward_fund(comment);
                                    uint64_t old_weight = utilities::get_vote_weight(old_vote_rshares.value,
                                                                                     reward_fund);
                                    uint64_t new_weight = utilities::get_vote_weight(comment.vote_rshares.value,
                                                                                     reward_fund);
                                    cv.weight = new_weight - old_weight;
                                } else if (this->db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
                                    uint64_t old_weight = ((std::numeric_limits<uint64_t>::max() *
                                                            fc::uint128_t(old_vote_rshares.value)) /
                                                           (two_s + old_vote_rshares.value)).to_uint64();
                                    uint64_t new_weight = ((std::numeric_limits<uint64_t>::max() *
                                                            fc::uint128_t(comment.vote_rshares.value)) /
                                                           (two_s + comment.vote_rshares.value)).to_uint64();
                                    cv.weight = new_weight - old_weight;
                                } else {
                                    uint64_t old_weight = ((std::numeric_limits<uint64_t>::max() *
                                                            fc::uint128_t(10000 * old_vote_rshares.value)) /
                                                           (two_s + (10000 * old_vote_rshares.value))).to_uint64();
                                    uint64_t new_weight = ((std::numeric_limits<uint64_t>::max() *
                                                            fc::uint128_t(10000 * comment.vote_rshares.value)) /
                                                           (two_s + (10000 * comment.vote_rshares.value))).to_uint64();
                                    cv.weight = new_weight - old_weight;
                                }
                            }

                            max_vote_weight = cv.weight;

                            if (this->db.head_block_time() > fc::time_point_sec(
                                    STEEMIT_HARDFORK_0_6_REVERSE_AUCTION_TIME))  /// start enforcing this prior to the hardfork
                            {
                                /// discount weight by time
                                uint128_t w(max_vote_weight);
                                uint64_t delta_t = std::min(uint64_t((cv.last_update - comment.created).to_seconds()),
                                                            uint64_t(STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS));

                                w *= delta_t;
                                w /= STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS;
                                cv.weight = w.to_uint64();
                            }
                        } else {
                            cv.weight = 0;
                        }
                    });

                    if (max_vote_weight) {
                        this->db.modify(comment, [&](comment_object &c) {
                            c.total_vote_weight += max_vote_weight;
                        });
                    }

                    if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__86)) {
                        this->db.adjust_rshares2(comment, old_rshares, new_rshares);
                    }
                } else {
                    FC_ASSERT(itr->num_changes < STEEMIT_MAX_VOTE_CHANGES,
                              "Voter has used the maximum number of vote changes on this comment.");

                    if (this->db.is_producing() || this->db.has_hardfork(STEEMIT_HARDFORK_0_6__112))
                        FC_ASSERT(itr->vote_percent != o.weight, "You have already voted in a similar way.");

                    /// this is the rshares voting for or against the post
                    int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;

                    if (itr->rshares < rshares && this->db.has_hardfork(STEEMIT_HARDFORK_0_7))
                        FC_ASSERT(this->db.head_block_time() <
                                  this->db.calculate_discussion_payout_time(comment) - STEEMIT_UPVOTE_LOCKOUT,
                                  "Cannot increase payout within last minute before payout.");

                    this->db.modify(voter, [&](account_object &a) {
                        a.voting_power = current_power - used_power;
                        a.last_vote_time = this->db.head_block_time();
                    });

                    /// if the current net_rshares is less than 0, the post is getting 0 rewards so it is not factored into total rshares^2
                    fc::uint128_t old_rshares = std::max(comment.net_rshares.value, int64_t(0));
                    const auto &root = this->db.get(comment.root_comment);
                    auto old_root_abs_rshares = root.children_abs_rshares.value;

                    fc::uint128_t avg_cashout_sec;

                    if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__91)) {
                        fc::uint128_t cur_cashout_time_sec = this->db.calculate_discussion_payout_time(
                                comment).sec_since_epoch();
                        fc::uint128_t new_cashout_time_sec;

                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                            !this->db.has_hardfork(STEEMIT_HARDFORK_0_13__257)) {
                            new_cashout_time_sec = this->db.head_block_time().sec_since_epoch() +
                                                   STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17;
                        } else {
                            new_cashout_time_sec = this->db.head_block_time().sec_since_epoch() +
                                                   STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF12;
                        }

                        if (this->db.has_hardfork(STEEMIT_HARDFORK_0_14__259) && abs_rshares == 0) {
                            avg_cashout_sec = cur_cashout_time_sec;
                        } else {
                            avg_cashout_sec =
                                    (cur_cashout_time_sec * old_root_abs_rshares + new_cashout_time_sec * abs_rshares) /
                                    (old_root_abs_rshares + abs_rshares);
                        }
                    }

                    this->db.modify(comment, [&](comment_object &c) {
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

                    this->db.modify(root, [&](comment_object &c) {
                        c.children_abs_rshares += abs_rshares;
                        if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__91)) {
                            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12__177) &&
                                c.last_payout > fc::time_point_sec::min()) {
                                c.cashout_time = c.last_payout + STEEMIT_SECOND_CASHOUT_WINDOW;
                            } else {
                                c.cashout_time = fc::time_point_sec(std::min(uint32_t(avg_cashout_sec.to_uint64()),
                                                                             c.max_cashout_time.sec_since_epoch()));
                            }

                            if (c.max_cashout_time == fc::time_point_sec::maximum()) {
                                c.max_cashout_time =
                                        this->db.head_block_time() + fc::seconds(STEEMIT_MAX_CASHOUT_WINDOW_SECONDS);
                            }
                        }
                    });

                    fc::uint128_t new_rshares = std::max(comment.net_rshares.value, int64_t(0));

                    /// calculate rshares2 value
                    new_rshares = utilities::calculate_claims(new_rshares);
                    old_rshares = utilities::calculate_claims(old_rshares);

                    this->db.modify(comment, [&](comment_object &c) {
                        c.total_vote_weight -= itr->weight;
                    });

                    this->db.modify(*itr, [&](comment_vote_object &cv) {
                        cv.rshares = rshares;
                        cv.vote_percent = o.weight;
                        cv.last_update = this->db.head_block_time();
                        cv.weight = 0;
                        cv.num_changes += 1;
                    });

                    if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_17__86)) {
                        this->db.adjust_rshares2(comment, old_rshares, new_rshares);
                    }
                }

            } FC_CAPTURE_AND_RETHROW((o))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release, typename Operation>
        void pow_apply(database &db, Operation o) {
            const auto &dgp = db.get_dynamic_global_properties();

            if (db.is_producing() || db.has_hardfork(STEEMIT_HARDFORK_0_5__59)) {
                const auto &witness_by_work = db.template get_index<witness_index>().indices().
                        template get<by_work>();
                auto work_itr = witness_by_work.find(o.work.work);
                if (work_itr != witness_by_work.end()) {
                    FC_ASSERT(!"DUPLICATE WORK DISCOVERED", "${w}  ${witness}", ("w", o)("wit", *work_itr));
                }
            }

            const auto &accounts_by_name = db.template get_index<account_index>().indices().
                    template get<by_name>();

            auto itr = accounts_by_name.find(o.get_worker_account());
            if (itr == accounts_by_name.end()) {
                db.template create<account_object>([&](account_object &acc) {
                    acc.name = o.get_worker_account();
                    acc.memo_key = o.work.worker;
                    acc.created = dgp.time;
                    acc.last_vote_time = dgp.time;

                    if (!db.has_hardfork(STEEMIT_HARDFORK_0_11__169)) {
                        acc.recovery_account = STEEMIT_INIT_MINER_NAME;
                    } else {
                        acc.recovery_account = "";
                    } /// highest voted witness at time of recovery
                });

                db.template create<account_authority_object>([&](account_authority_object &auth) {
                    auth.account = o.get_worker_account();
                    auth.owner = authority(1, o.work.worker, 1);
                    auth.active = auth.owner;
                    auth.posting = auth.owner;
                });

                db.template create<account_balance_object>([&](account_balance_object &b) {
                    b.owner = o.get_worker_account();
                    b.asset_name = STEEM_SYMBOL_NAME;
                    b.balance = 0;
                });

                db.template create<account_balance_object>([&](account_balance_object &b) {
                    b.owner = o.get_worker_account();
                    b.asset_name = SBD_SYMBOL_NAME;
                    b.balance = 0;
                });

                db.template create<account_statistics_object>([&](account_statistics_object &s) {
                    s.owner = o.get_worker_account();
                });
            }

            const auto &worker_account = db.get_account(o.get_worker_account()); // verify it exists
            const auto &worker_auth = db.get<account_authority_object, by_account>(o.get_worker_account());
            FC_ASSERT(worker_auth.active.num_auths() == 1, "Miners can only have one key authority. ${a}",
                      ("a", worker_auth.active));
            FC_ASSERT(worker_auth.active.key_auths.size() == 1, "Miners may only have one key authority.");
            FC_ASSERT(worker_auth.active.key_auths.begin()->first == o.work.worker,
                      "Work must be performed by key that signed the work.");
            FC_ASSERT(o.block_id == db.head_block_id(), "pow not for last block");
            if (db.has_hardfork(STEEMIT_HARDFORK_0_13__256)) {
                FC_ASSERT(worker_account.last_account_update < db.head_block_time(),
                          "Worker account must not have updated their account this block.");
            }

            fc::sha256 target = db.get_pow_target();

            FC_ASSERT(o.work.work < target, "Work lacks sufficient difficulty.");

            db.modify(dgp, [&](dynamic_global_property_object &p) {
                p.total_pow++; // make sure this doesn't break anything...
                p.num_pow_witnesses++;
            });


            const witness_object *cur_witness = db.find_witness(worker_account.name);
            if (cur_witness) {
                FC_ASSERT(cur_witness->pow_worker == 0, "This account is already scheduled for pow block production.");
                db.modify(*cur_witness, [&](witness_object &w) {
                    w.props = {asset<0, 17, 0>(o.props.account_creation_fee.amount,
                                               o.props.account_creation_fee.symbol_name()), o.props.maximum_block_size,
                               o.props.sbd_interest_rate};
                    w.pow_worker = dgp.total_pow;
                    w.last_work = o.work.work;
                });
            } else {
                db.template create<witness_object>([&](witness_object &w) {
                    w.owner = o.get_worker_account();
                    w.props = {asset<0, 17, 0>(o.props.account_creation_fee.amount,
                                               o.props.account_creation_fee.symbol_name()), o.props.maximum_block_size,
                               o.props.sbd_interest_rate};
                    w.signing_key = o.work.worker;
                    w.pow_worker = dgp.total_pow;
                    w.last_work = o.work.work;
                });
            }
            /// POW reward depends upon whether we are before or after MINER_VOTING kicks in
            asset<0, 17, 0> pow_reward = db.get_pow_reward();
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

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void pow_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            FC_ASSERT(!this->db.has_hardfork(STEEMIT_HARDFORK_0_13__256), "pow is deprecated. Use pow2 instead");
            pow_apply<Major, Hardfork, Release, operation_type>(this->db, o);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void pow2_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            const auto &dgp = this->db.get_dynamic_global_properties();
            uint32_t target_pow = this->db.get_pow_summary_target();
            account_name_type worker_account;

            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                const auto &work = o.work.template get<equihash_pow>();
                FC_ASSERT(work.prev_block == this->db.head_block_id(), "Equihash pow op not for last block");
                auto recent_block_num = protocol::block_header::num_from_id(work.input.prev_block);
                FC_ASSERT(recent_block_num > dgp.last_irreversible_block_num,
                          "Equihash pow done for block older than last irreversible block num. Recent block: ${l}, Irreversible block: ${r}",
                          ("l", recent_block_num)("r", dgp.last_irreversible_block_num));
                FC_ASSERT(work.pow_summary < target_pow, "Insufficient work difficulty. Work: ${w}, Target: ${t}",
                          ("w", work.pow_summary)("t", target_pow));
                worker_account = work.input.worker_account;
            } else {
                const auto &work = o.work.template get<pow2>();
                FC_ASSERT(work.input.prev_block == this->db.head_block_id(), "Work not for last block");
                FC_ASSERT(work.pow_summary < target_pow, "Insufficient work difficulty. Work: ${w}, Target: ${t}",
                          ("w", work.pow_summary)("t", target_pow));
                worker_account = work.input.worker_account;
            }

            FC_ASSERT(o.props.maximum_block_size >= STEEMIT_MIN_BLOCK_SIZE_LIMIT * 2,
                      "Voted maximum block size is too small.");

            this->db.modify(dgp, [&](dynamic_global_property_object &p) {
                p.total_pow++;
                p.num_pow_witnesses++;
            });

            const auto &accounts_by_name = this->db.template get_index<account_index>().indices().
                    template get<by_name>();
            auto itr = accounts_by_name.find(worker_account);
            if (itr == accounts_by_name.end()) {
                FC_ASSERT(o.new_owner_key.valid(), "New owner key is not valid.");
                this->db.template create<account_object>([&](account_object &acc) {
                    acc.name = worker_account;
                    acc.memo_key = *o.new_owner_key;
                    acc.created = dgp.time;
                    acc.last_vote_time = dgp.time;
                    acc.recovery_account = ""; /// highest voted witness at time of recovery
                });

                this->db.template create<account_authority_object>([&](account_authority_object &auth) {
                    auth.account = worker_account;
                    auth.owner = authority(1, *o.new_owner_key, 1);
                    auth.active = auth.owner;
                    auth.posting = auth.owner;
                });

                this->db.template create<account_balance_object>([&](account_balance_object &b) {
                    b.owner = worker_account;
                    b.asset_name = STEEM_SYMBOL_NAME;
                    b.balance = 0;
                });

                this->db.template create<account_balance_object>([&](account_balance_object &b) {
                    b.owner = worker_account;
                    b.asset_name = SBD_SYMBOL_NAME;
                    b.balance = 0;
                });

                this->db.template create<account_statistics_object>([&](account_statistics_object &s) {
                    s.owner = worker_account;
                });

                this->db.template create<witness_object>([&](witness_object &w) {
                    w.owner = worker_account;
                    w.props = {asset<0, 17, 0>(o.props.account_creation_fee.amount,
                                               o.props.account_creation_fee.symbol_name()), o.props.maximum_block_size,
                               o.props.sbd_interest_rate};
                    w.signing_key = *o.new_owner_key;
                    w.pow_worker = dgp.total_pow;
                });
            } else {
                FC_ASSERT(!o.new_owner_key.valid(), "Cannot specify an owner key unless creating account.");
                const witness_object *cur_witness = this->db.find_witness(worker_account);
                FC_ASSERT(cur_witness, "Witness must be created for existing account before mining.");
                FC_ASSERT(cur_witness->pow_worker == 0, "This account is already scheduled for pow block production.");
                this->db.modify(*cur_witness, [&](witness_object &w) {
                    w.props = {asset<0, 17, 0>(o.props.account_creation_fee.amount,
                                               o.props.account_creation_fee.symbol_name()), o.props.maximum_block_size,
                               o.props.sbd_interest_rate};
                    w.pow_worker = dgp.total_pow;
                });
            }

            if (!this->db.has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                /// pay the witness that includes this POW
                asset<0, 17, 0> inc_reward = this->db.get_pow_reward();
                this->db.adjust_supply(inc_reward, true);

                const auto &inc_witness = this->db.get_account(dgp.current_witness);
                this->db.template create_vesting(inc_witness, inc_reward);
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void feed_publish_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            const auto &witness = this->db.get_witness(o.publisher);
            this->db.modify(witness, [&](witness_object &w) {
                w.sbd_exchange_rate = protocol::price<0, 17, 0>(
                        protocol::asset<0, 17, 0>(o.exchange_rate.base.amount, o.exchange_rate.base.symbol_name()),
                        protocol::asset<0, 17, 0>(o.exchange_rate.quote.amount, o.exchange_rate.quote.symbol_name()));
                w.last_sbd_exchange_update = this->db.head_block_time();
            });
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void report_over_production_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            FC_ASSERT(!this->db.has_hardfork(STEEMIT_HARDFORK_0_4), "report_over_production_operation is disabled.");
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void challenge_authority_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_14__307)) {
                FC_ASSERT(false, "Challenge authority operation is currently disabled.");
            }
            const auto &challenged = this->db.get_account(o.challenged);
            const auto &challenger = this->db.get_account(o.challenger);

            if (o.require_owner) {
                FC_ASSERT(challenged.reset_account == o.challenger,
                          "Owner authority can only be challenged by its reset account.");
                FC_ASSERT(this->db.get_balance(challenger, STEEM_SYMBOL_NAME) >= STEEMIT_OWNER_CHALLENGE_FEE);
                FC_ASSERT(!challenged.owner_challenged);
                FC_ASSERT(this->db.head_block_time() - challenged.last_owner_proved > STEEMIT_OWNER_CHALLENGE_COOLDOWN);

                this->db.adjust_balance(challenger, -STEEMIT_OWNER_CHALLENGE_FEE);
                this->db.create_vesting(this->db.get_account(o.challenged), STEEMIT_OWNER_CHALLENGE_FEE);

                this->db.modify(challenged, [&](account_object &a) {
                    a.owner_challenged = true;
                });
            } else {
                FC_ASSERT(this->db.get_balance(challenger, STEEM_SYMBOL_NAME) >= STEEMIT_ACTIVE_CHALLENGE_FEE,
                          "Account does not have sufficient funds to pay challenge fee.");
                FC_ASSERT(!(challenged.owner_challenged || challenged.active_challenged),
                          "Account is already challenged.");
                FC_ASSERT(
                        this->db.head_block_time() - challenged.last_active_proved > STEEMIT_ACTIVE_CHALLENGE_COOLDOWN,
                        "Account cannot be challenged because it was recently challenged.");

                this->db.adjust_balance(challenger, -STEEMIT_ACTIVE_CHALLENGE_FEE);
                this->db.create_vesting(this->db.get_account(o.challenged), STEEMIT_ACTIVE_CHALLENGE_FEE);

                this->db.modify(challenged, [&](account_object &a) {
                    a.active_challenged = true;
                });
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void prove_authority_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            const auto &challenged = this->db.get_account(o.challenged);
            FC_ASSERT(challenged.owner_challenged || challenged.active_challenged,
                      "Account is not challeneged. No need to prove authority.");

            this->db.modify(challenged, [&](account_object &a) {
                a.active_challenged = false;
                a.last_active_proved = this->db.head_block_time();
                if (o.require_owner) {
                    a.owner_challenged = false;
                    a.last_owner_proved = this->db.head_block_time();
                }
            });
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void request_account_recovery_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {

            const auto &account_to_recover = this->db.get_account(o.account_to_recover);

            if (account_to_recover.recovery_account.length()) {   // Make sure recovery matches expected recovery account
                FC_ASSERT(account_to_recover.recovery_account == o.recovery_account,
                          "Cannot recover an account that does not have you as there recovery partner.");
            } else {                                                  // Empty string recovery account defaults to top witness
                FC_ASSERT(this->db.template get_index<witness_index>().indices().
                        template get<by_vote_name>().begin()->owner == o.recovery_account,
                          "Top witness must recover an account with no recovery partner.");
            }

            const auto &recovery_request_idx = this->db.template get_index<account_recovery_request_index>().indices().
                    template get<by_account>();
            auto request = recovery_request_idx.find(o.account_to_recover);

            if (request == recovery_request_idx.end()) // New Request
            {
                FC_ASSERT(!o.new_owner_authority.is_impossible(), "Cannot recover using an impossible authority.");
                FC_ASSERT(o.new_owner_authority.weight_threshold, "Cannot recover using an open authority.");

                // Check accounts in the new authority exist
                if ((this->db.has_hardfork(STEEMIT_HARDFORK_0_15__465) || this->db.is_producing())) {
                    for (auto &a : o.new_owner_authority.account_auths) {
                        this->db.get_account(a.first);
                    }
                }

                this->db.template create<account_recovery_request_object>([&](account_recovery_request_object &req) {
                    req.account_to_recover = o.account_to_recover;
                    req.new_owner_authority = o.new_owner_authority;
                    req.expires = this->db.head_block_time() + STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD;
                });
            } else if (o.new_owner_authority.weight_threshold == 0) // Cancel Request if authority is open
            {
                this->db.remove(*request);
            } else // Change Request
            {
                FC_ASSERT(!o.new_owner_authority.is_impossible(), "Cannot recover using an impossible authority.");

                // Check accounts in the new authority exist
                if ((this->db.has_hardfork(STEEMIT_HARDFORK_0_15__465) || this->db.is_producing())) {
                    for (auto &a : o.new_owner_authority.account_auths) {
                        this->db.get_account(a.first);
                    }
                }

                this->db.modify(*request, [&](account_recovery_request_object &req) {
                    req.new_owner_authority = o.new_owner_authority;
                    req.expires = this->db.head_block_time() + STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD;
                });
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void recover_account_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            const auto &account = this->db.get_account(o.account_to_recover);

            if (this->db.has_hardfork(STEEMIT_HARDFORK_0_12)) {
                FC_ASSERT(this->db.head_block_time() - account.last_account_recovery > STEEMIT_OWNER_UPDATE_LIMIT,
                          "Owner authority can only be updated once an hour.");
            }

            const auto &recovery_request_idx = this->db.template get_index<account_recovery_request_index>().indices().
                    template get<by_account>();
            auto request = recovery_request_idx.find(o.account_to_recover);

            FC_ASSERT(request != recovery_request_idx.end(), "There are no active recovery requests for this account.");
            FC_ASSERT(request->new_owner_authority == o.new_owner_authority,
                      "New owner authority does not match recovery request.");

            const auto &recent_auth_idx = this->db.template get_index<owner_authority_history_index>().indices().
                    template get<by_account>();
            auto hist = recent_auth_idx.lower_bound(o.account_to_recover);
            bool found = false;

            while (hist != recent_auth_idx.end() && hist->account == o.account_to_recover && !found) {
                found = hist->previous_owner_authority == o.recent_owner_authority;
                if (found) {
                    break;
                }
                ++hist;
            }

            FC_ASSERT(found, "Recent authority not found in authority history.");

            this->db.remove(*request); // Remove first, update_owner_authority may invalidate iterator
            this->db.update_owner_authority(account, o.new_owner_authority);
            this->db.modify(account, [&](account_object &a) {
                a.last_account_recovery = this->db.head_block_time();
            });
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void change_recovery_account_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            this->db.get_account(o.new_recovery_account); // Simply validate account exists
            const auto &account_to_recover = this->db.get_account(o.account_to_recover);

            const auto &change_recovery_idx = this->db.template get_index<
                    change_recovery_account_request_index>().indices().
                    template get<by_account>();
            auto request = change_recovery_idx.find(o.account_to_recover);

            if (request == change_recovery_idx.end()) // New request
            {
                this->db.template create<change_recovery_account_request_object>(
                        [&](change_recovery_account_request_object &req) {
                            req.account_to_recover = o.account_to_recover;
                            req.recovery_account = o.new_recovery_account;
                            req.effective_on = this->db.head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD;
                        });
            } else if (account_to_recover.recovery_account != o.new_recovery_account) // Change existing request
            {
                this->db.modify(*request, [&](change_recovery_account_request_object &req) {
                    req.recovery_account = o.new_recovery_account;
                    req.effective_on = this->db.head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD;
                });
            } else // Request exists and changing back to current recovery account
            {
                this->db.remove(*request);
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void decline_voting_rights_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &o) {
            FC_ASSERT(this->db.has_hardfork(STEEMIT_HARDFORK_0_14__324));

            const auto &account = this->db.get_account(o.account);
            const auto &request_idx = this->db.template get_index<decline_voting_rights_request_index>().indices().
                    template get<by_account>();
            auto itr = request_idx.find(account.id);

            if (o.decline) {
                FC_ASSERT(itr == request_idx.end(), "Cannot create new request because one already exists.");

                this->db.template create<decline_voting_rights_request_object>(
                        [&](decline_voting_rights_request_object &req) {
                            req.account = account.id;
                            req.effective_date = this->db.head_block_time() + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD;
                        });
            } else {
                FC_ASSERT(itr != request_idx.end(), "Cannot cancel the request because it does not exist.");
                this->db.remove(*itr);
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void reset_account_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &op) {
            FC_ASSERT(false, "Reset Account Operation is currently disabled.");

            const auto &acnt = this->db.get_account(op.account_to_reset);
            auto band = this->db.template find<account_bandwidth_object, by_account_bandwidth_type>(
                    boost::make_tuple(op.account_to_reset, bandwidth_type::old_forum));
            if (band != nullptr) {
                FC_ASSERT((this->db.head_block_time() - band->last_bandwidth_update) > fc::days(60),
                          "Account must be inactive for 60 days to be eligible for reset");
            }
            FC_ASSERT(acnt.reset_account == op.reset_account, "Reset account does not match reset account on account.");

            this->db.update_owner_authority(acnt, op.new_owner_authority);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void set_reset_account_evaluator<Major, Hardfork, Release>::do_apply(const operation_type &op) {

            FC_ASSERT(false, "Set Reset Account Operation is currently disabled.");

            const auto &acnt = this->db.get_account(op.account);
            this->db.get_account(op.reset_account);

            FC_ASSERT(acnt.reset_account == op.current_reset_account,
                      "Current reset account does not match reset account on account.");
            FC_ASSERT(acnt.reset_account != op.reset_account, "Reset account must change");

            this->db.modify(acnt, [&](account_object &a) {
                a.reset_account = op.reset_account;
            });
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void delegate_vesting_shares_evaluator<Major, Hardfork, Release>::do_apply(
                const delegate_vesting_shares_operation<Major, Hardfork, Release> &op) {
            FC_ASSERT(this->db.has_hardfork(STEEMIT_HARDFORK_0_17__101),
                      "delegate_vesting_shares_operation is not enabled until HF 17"); //TODO: Delete after hardfork

            const auto &delegator = this->db.get_account(op.delegator);
            const auto &delegatee = this->db.get_account(op.delegatee);
            auto delegation = this->db.template find<vesting_delegation_object, by_delegation>(
                    boost::make_tuple(op.delegator, op.delegatee));

            auto available_shares = delegator.vesting_shares - delegator.delegated_vesting_shares -
                                    asset<0, 17, 0>(delegator.to_withdraw - delegator.withdrawn, VESTS_SYMBOL);

            const auto &wso = this->db.get_witness_schedule_object();
            const auto &gpo = this->db.get_dynamic_global_properties();
            auto min_delegation =
                    asset<0, 17, 0>(wso.median_props.account_creation_fee.amount * 10, STEEM_SYMBOL_NAME) *
                    gpo.get_vesting_share_price();
            auto min_update = wso.median_props.account_creation_fee * gpo.get_vesting_share_price();

            // If delegation doesn't exist, create it
            if (delegation == nullptr) {
                FC_ASSERT(available_shares >= op.vesting_shares,
                          "Account does not have enough vesting shares to delegate.");
                FC_ASSERT(op.vesting_shares >= min_delegation, "Account must delegate a minimum of ${v}",
                          ("v", min_delegation));

                this->db.template create<vesting_delegation_object>([&](vesting_delegation_object &obj) {
                    obj.delegator = op.delegator;
                    obj.delegatee = op.delegatee;
                    obj.vesting_shares = op.vesting_shares;
                    obj.min_delegation_time = this->db.head_block_time();
                });

                this->db.modify(delegator, [&](account_object &a) {
                    a.delegated_vesting_shares += op.vesting_shares;
                });

                this->db.modify(delegatee, [&](account_object &a) {
                    a.received_vesting_shares += op.vesting_shares;
                });
            } else if (op.vesting_shares >= delegation->vesting_shares) {
                auto delta = op.vesting_shares - delegation->vesting_shares;

                FC_ASSERT(delta >= min_update, "Golos Power increase is not enough of a different. min_update: ${min}",
                          ("min", min_update));
                FC_ASSERT(available_shares >= op.vesting_shares - delegation->vesting_shares,
                          "Account does not have enough vesting shares to delegate.");

                this->db.modify(delegator, [&](account_object &a) {
                    a.delegated_vesting_shares += delta;
                });

                this->db.modify(delegatee, [&](account_object &a) {
                    a.received_vesting_shares += delta;
                });

                this->db.modify(*delegation, [&](vesting_delegation_object &obj) {
                    obj.vesting_shares = op.vesting_shares;
                });
            } else {
                auto delta = delegation->vesting_shares - op.vesting_shares;

                FC_ASSERT(delta >= min_update, "Golos Power increase is not enough of a different. min_update: ${min}",
                          ("min", min_update));
                FC_ASSERT(op.vesting_shares >= min_delegation || op.vesting_shares.amount == 0,
                          "Delegation must be removed or leave minimum delegation amount of ${v}",
                          ("v", min_delegation));

                this->db.template create<vesting_delegation_expiration_object>(
                        [&](vesting_delegation_expiration_object &obj) {
                            obj.delegator = op.delegator;
                            obj.vesting_shares = delta;
                            obj.expiration = std::max(this->db.head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS,
                                                      delegation->min_delegation_time);

                        });

                this->db.modify(delegatee, [&](account_object &a) {
                    a.received_vesting_shares -= delta;
                });

                if (op.vesting_shares.amount > 0) {
                    this->db.modify(*delegation, [&](vesting_delegation_object &obj) {
                        obj.vesting_shares = op.vesting_shares;
                    });
                } else {
                    this->db.remove(*delegation);
                }
            }
        }
    }
} // golos::chain

#include <golos/chain/evaluators/steem_evaluator.tpp>