#include <golos/protocol/steem_operations.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>
#include <fc/io/json.hpp>

namespace golos { namespace protocol {
        void validate_account_name(const std::string &name) {
            GOLOS_CHECK_VALUE(is_valid_account_name(name), "Account name ${name} is invalid", ("name", name));
        }

        void validate_permlink(const std::string &permlink) {
            GOLOS_CHECK_VALUE(permlink.size() < STEEMIT_MAX_PERMLINK_LENGTH, "permlink is too long");
            GOLOS_CHECK_VALUE(fc::is_utf8(permlink), "permlink not formatted in UTF8");
        }

        inline void validate_account_json_metadata(const string& json_metadata) {
            if (json_metadata.size() > 0) {
                GOLOS_CHECK_VALUE_UTF8(json_metadata);
                GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            }
        }

        bool inline is_asset_type(asset asset, asset_symbol_type symbol) {
            return asset.symbol == symbol;
        }

        void account_create_operation::validate() const {
            GOLOS_CHECK_PARAM(new_account_name, validate_account_name(new_account_name));
            GOLOS_CHECK_PARAM(owner, owner.validate());
            GOLOS_CHECK_PARAM(active, active.validate());
            GOLOS_CHECK_PARAM(fee, {
                GOLOS_CHECK_VALUE(is_asset_type(fee, STEEM_SYMBOL), "Account creation fee must be GOLOS");
                GOLOS_CHECK_VALUE(fee >= asset(0, STEEM_SYMBOL), "Account creation fee cannot be negative");
            });
            GOLOS_CHECK_PARAM(json_metadata, validate_account_json_metadata(json_metadata));
        }

        struct account_create_with_delegation_extension_validate_visitor {
            account_create_with_delegation_extension_validate_visitor() {
            }

            using result_type = void;

            void operator()(const account_referral_options& aro) const {
                aro.validate();
            }
        };

        void account_referral_options::validate() const {
            validate_account_name(referrer);
            GOLOS_CHECK_PARAM(break_fee, {
                GOLOS_CHECK_VALUE(break_fee.symbol == STEEM_SYMBOL, "Break fee in GOLOS only is allowed.");
                GOLOS_CHECK_VALUE(break_fee.amount >= 0, "Negative break fee is not allowed.");
            });
        }

        void account_create_with_delegation_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(new_account_name);
            GOLOS_CHECK_PARAM_ACCOUNT(creator);
            GOLOS_CHECK_PARAM(fee,        {GOLOS_CHECK_ASSET_GE0(fee, GOLOS)});
            GOLOS_CHECK_PARAM(delegation, {GOLOS_CHECK_ASSET_GE0(delegation, GESTS)});
            GOLOS_CHECK_PARAM_VALIDATE(owner);
            GOLOS_CHECK_PARAM_VALIDATE(active);
            GOLOS_CHECK_PARAM_VALIDATE(posting);
            GOLOS_CHECK_PARAM(json_metadata, validate_account_json_metadata(json_metadata));

            for (auto& e : extensions) {
                e.visit(account_create_with_delegation_extension_validate_visitor());
            }
        }

        void account_update_operation::validate() const {
            GOLOS_CHECK_PARAM(account, validate_account_name(account));
            /*if( owner )
               owner->validate();
            if( active )
               active->validate();
            if( posting )
               posting->validate();*/
            GOLOS_CHECK_PARAM(json_metadata, validate_account_json_metadata(json_metadata));
        }

        void account_metadata_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(account);
            GOLOS_CHECK_PARAM(json_metadata, {
                GOLOS_CHECK_VALUE_NOT_EMPTY(json_metadata);
                validate_account_json_metadata(json_metadata);
            });
        }

        void comment_operation::validate() const {
            GOLOS_CHECK_PARAM(title, {
                GOLOS_CHECK_VALUE(title.size() < 256, "Title larger than size limit");
                GOLOS_CHECK_VALUE(fc::is_utf8(title), "Title not formatted in UTF8");
            });

            GOLOS_CHECK_PARAM(body, {
                GOLOS_CHECK_VALUE(body.size() > 0, "Body is empty");
                GOLOS_CHECK_VALUE(fc::is_utf8(body), "Body not formatted in UTF8");
            });

            if (parent_author.size()) {
                GOLOS_CHECK_PARAM(parent_author, validate_account_name(parent_author));
            }

            GOLOS_CHECK_PARAM(author,          validate_account_name(author));
            GOLOS_CHECK_PARAM(parent_permlink, validate_permlink(parent_permlink));
            GOLOS_CHECK_PARAM(permlink,        validate_permlink(permlink));

            if (json_metadata.size() > 0) {
                GOLOS_CHECK_PARAM(json_metadata, {
                    GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
                });
            }
        }

        struct comment_options_extension_validate_visitor {
            comment_options_extension_validate_visitor() {
            }

            using result_type = void;

            void operator()( const comment_payout_beneficiaries& cpb ) const {
                cpb.validate();
            }

            void operator()( const comment_auction_window_reward_destination& cawrd ) const {
                cawrd.validate();
            }

            void operator()(const comment_curation_rewards_percent& ccrp) const {
                ccrp.validate();
            }
        };

        void comment_payout_beneficiaries::validate() const {
            uint32_t sum = 0;  // avoid overflow

            GOLOS_CHECK_PARAM(beneficiaries, {
                GOLOS_CHECK_VALUE(beneficiaries.size(), "Must specify at least one beneficiary");
                GOLOS_CHECK_VALUE(beneficiaries.size() < 128,
                    "Cannot specify more than 127 beneficiaries."); // Require size serialization fits in one byte.

                for (auto& beneficiary : beneficiaries) {
                    validate_account_name(beneficiary.account);
                    GOLOS_CHECK_VALUE(beneficiary.weight <= STEEMIT_100_PERCENT,
                        "Cannot allocate more than 100% of rewards to one account");
                    sum += beneficiary.weight;
                }

                GOLOS_CHECK_VALUE(sum <= STEEMIT_100_PERCENT,
                    "Cannot allocate more than 100% of rewards to a comment");

                for (size_t i = 1; i < beneficiaries.size(); i++) {
                    GOLOS_CHECK_VALUE(beneficiaries[i - 1] < beneficiaries[i],
                        "Benficiaries ${first} and ${second} not in sorted order (account ascending)",
                        ("first", beneficiaries[i-1].account)("second", beneficiaries[i].account));
                }
            });
        }

        void comment_auction_window_reward_destination::validate() const {
            GOLOS_CHECK_PARAM(destination, {
                GOLOS_CHECK_VALUE(destination == to_reward_fund || destination == to_curators,
                    "Auction window reward must go either to reward_fund or to curators"
                );
            });
        }

        void comment_curation_rewards_percent::validate() const {
            GOLOS_CHECK_PARAM(percent, {
                GOLOS_CHECK_VALUE(STEEMIT_MIN_CURATION_PERCENT <= percent && percent <= STEEMIT_MAX_CURATION_PERCENT,
                    "Curation rewards percent must be between ${min} and ${max}.",
                    ("min", STEEMIT_MIN_CURATION_PERCENT)("max", STEEMIT_MAX_CURATION_PERCENT));
            });
        }

        void comment_options_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(author);
            GOLOS_CHECK_PARAM(percent_steem_dollars, GOLOS_CHECK_VALUE_LE(percent_steem_dollars, STEEMIT_100_PERCENT));
            GOLOS_CHECK_PARAM(max_accepted_payout, GOLOS_CHECK_ASSET_GE0(max_accepted_payout, GBG));
            GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
            for (auto& e : extensions) {
                e.visit(comment_options_extension_validate_visitor());
            }
        }

        void delete_comment_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(author);
            GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
        }

        void challenge_authority_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(challenger);
            GOLOS_CHECK_PARAM_ACCOUNT(challenged);
            GOLOS_CHECK_LOGIC(challenged != challenger,
                logic_exception::cannot_challenge_yourself,
                "cannot challenge yourself");
        }

        void prove_authority_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(challenged);
        }

        void vote_operation::validate() const {
            GOLOS_CHECK_PARAM(voter, validate_account_name(voter));
            GOLOS_CHECK_PARAM(author, validate_account_name(author));
            GOLOS_CHECK_PARAM(weight, {
                GOLOS_CHECK_VALUE(abs(weight) <= STEEMIT_100_PERCENT,
                        "Weight is not a STEEMIT percentage");
            });
            GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
        }

        void transfer_operation::validate() const {
            try {
                GOLOS_CHECK_PARAM(from, validate_account_name(from));
                GOLOS_CHECK_PARAM(to, validate_account_name(to));
                GOLOS_CHECK_PARAM(amount, {
                    GOLOS_CHECK_VALUE(amount.symbol != VESTS_SYMBOL, "transferring of Golos Power (GESTS) is not allowed.");
                    GOLOS_CHECK_VALUE(amount.amount > 0, "Cannot transfer a negative amount (aka: stealing)");
                });
                GOLOS_CHECK_PARAM(memo, {
                    GOLOS_CHECK_VALUE(memo.size() < STEEMIT_MAX_MEMO_SIZE, "Memo is too large");
                    GOLOS_CHECK_VALUE(fc::is_utf8(memo), "Memo is not UTF8");
                });
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        void transfer_to_vesting_operation::validate() const {
            GOLOS_CHECK_PARAM(from, validate_account_name(from));
            GOLOS_CHECK_PARAM(amount, {
                GOLOS_CHECK_VALUE(is_asset_type(amount, STEEM_SYMBOL), "Amount must be GOLOS");
                GOLOS_CHECK_VALUE(amount > asset(0, STEEM_SYMBOL), "Must transfer a nonzero amount");
            });
            GOLOS_CHECK_PARAM(to, {
                if (to != account_name_type()) {
                    validate_account_name(to);
                }
            });
        }

        void withdraw_vesting_operation::validate() const {
            GOLOS_CHECK_PARAM(account, validate_account_name(account));
            GOLOS_CHECK_PARAM(vesting_shares, {
                GOLOS_CHECK_VALUE(is_asset_type(vesting_shares, VESTS_SYMBOL), "Amount must be GESTS");
                GOLOS_CHECK_VALUE(vesting_shares.amount >= 0, "Cannot withdraw negative GESTS");
            });
        }

        void set_withdraw_vesting_route_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from_account);
            GOLOS_CHECK_PARAM_ACCOUNT(to_account);
            GOLOS_CHECK_PARAM(percent, GOLOS_CHECK_VALUE_LEGE(percent, 0, STEEMIT_100_PERCENT));
        }


        void chain_properties_17::validate() const {
            GOLOS_CHECK_ASSET_GE(account_creation_fee, GOLOS, STEEMIT_MIN_ACCOUNT_CREATION_FEE);
            GOLOS_CHECK_VALUE_GE(maximum_block_size, STEEMIT_MIN_BLOCK_SIZE_LIMIT);
            GOLOS_CHECK_VALUE_LEGE(sbd_interest_rate, 0, STEEMIT_100_PERCENT);
        }

        void chain_properties_18::validate() const {
            chain_properties_17::validate();
            GOLOS_CHECK_ASSET_GT0(create_account_min_golos_fee, GOLOS);
            GOLOS_CHECK_ASSET_GT0(create_account_min_delegation, GOLOS);
            GOLOS_CHECK_ASSET_GT0(min_delegation, GOLOS);
            GOLOS_CHECK_VALUE_GT(create_account_delegation_time,
                (GOLOS_CREATE_ACCOUNT_DELEGATION_TIME).to_seconds() / 2);
        }

        void chain_properties_19::validate() const {
            chain_properties_18::validate();
            // GOLOS_CHECK_VALUE_LE(auction_window_size, STEEMIT_MAX_AUCTION_WINDOW_SIZE_SECONDS); // auction_window_size is limited by 16bit
            GOLOS_CHECK_VALUE_LE(max_referral_interest_rate, GOLOS_MAX_REFERRAL_INTEREST_RATE);
            GOLOS_CHECK_VALUE_LE(max_referral_term_sec, GOLOS_MAX_REFERRAL_TERM_SEC);

            GOLOS_CHECK_PARAM(min_referral_break_fee, {
                GOLOS_CHECK_VALUE(is_asset_type(min_referral_break_fee, STEEM_SYMBOL),
                    "Minimum break free must be GOLOS");
                GOLOS_CHECK_VALUE(min_referral_break_fee >= asset(0, STEEM_SYMBOL),
                    "Break free must be more or equal 0 GOLOS ");
            });

            GOLOS_CHECK_PARAM(max_referral_break_fee, {
                GOLOS_CHECK_VALUE(is_asset_type(max_referral_break_fee, STEEM_SYMBOL),
                    "Maximum break free must be GOLOS");
                GOLOS_CHECK_VALUE(max_referral_break_fee >= min_referral_break_fee,
                    "Maximum break free cann't be more than minimum break free");
                GOLOS_CHECK_VALUE(max_referral_break_fee <= GOLOS_MAX_REFERRAL_BREAK_FEE,
                    "Maximum break free cann't be more than ${max}", ("max", GOLOS_MAX_REFERRAL_BREAK_FEE));
            });

            GOLOS_CHECK_VALUE_LEGE(posts_window, 1, std::numeric_limits<uint16_t>::max() / 2);
            GOLOS_CHECK_VALUE_LEGE(posts_per_window, 1, posts_window);
            GOLOS_CHECK_VALUE_LEGE(comments_window, 1, std::numeric_limits<uint16_t>::max() / 2);
            GOLOS_CHECK_VALUE_LEGE(comments_per_window, 1, comments_window);
            GOLOS_CHECK_VALUE_LEGE(votes_window, 1, std::numeric_limits<uint16_t>::max() / 2);
            GOLOS_CHECK_VALUE_LEGE(votes_per_window, 1, votes_window);
            GOLOS_CHECK_VALUE_LE(max_delegated_vesting_interest_rate, STEEMIT_MAX_DELEGATED_VESTING_INTEREST_RATE);
            GOLOS_CHECK_VALUE_GE(custom_ops_bandwidth_multiplier, 1);
            GOLOS_CHECK_VALUE_LEGE(min_curation_percent, STEEMIT_MIN_CURATION_PERCENT, max_curation_percent);
            GOLOS_CHECK_VALUE_LEGE(max_curation_percent, min_curation_percent, STEEMIT_MAX_CURATION_PERCENT);

            GOLOS_CHECK_PARAM(curation_reward_curve, {
                GOLOS_CHECK_VALUE(
                    curation_reward_curve == curation_curve::bounded ||
                    curation_reward_curve == curation_curve::linear ||
                    curation_reward_curve == curation_curve::square_root,
                    "Curation curve should bounded or liner, or square_root");
            });

            GOLOS_CHECK_PARAM(allow_return_auction_reward_to_fund, {
                GOLOS_CHECK_VALUE(
                    allow_return_auction_reward_to_fund || allow_distribute_auction_reward,
                    "One or both options should be enabled: allow_return_auction_reward_to_fund and allow_distribute_auction_reward");
            });
        }

        void chain_properties_21::validate() const {
            chain_properties_19::validate();
            GOLOS_CHECK_VALUE_LE(worker_from_content_fund_percent, STEEMIT_100_PERCENT);
            GOLOS_CHECK_VALUE_LE(worker_from_vesting_fund_percent, STEEMIT_100_PERCENT);
            GOLOS_CHECK_VALUE_LE(worker_from_witness_fund_percent, STEEMIT_100_PERCENT);
            GOLOS_CHECK_VALUE_LE(worker_techspec_approve_term_sec, GOLOS_WORKER_TECHSPEC_APPROVE_TERM_SEC);
            GOLOS_CHECK_VALUE_LE(worker_result_approve_term_sec, GOLOS_WORKER_RESULT_APPROVE_TERM_SEC);
        }

        void witness_update_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(owner);
            GOLOS_CHECK_PARAM(url, {
                GOLOS_CHECK_VALUE_NOT_EMPTY(url);
                // GOLOS_CHECK_VALUE_MAX_SIZE(url, STEEMIT_MAX_WITNESS_URL_LENGTH);   // can't add without HF
                GOLOS_CHECK_VALUE_UTF8(url);
            });
            GOLOS_CHECK_PARAM(fee, {
                GOLOS_CHECK_ASSET_GE0(fee, GOLOS);  //"cannot be negative"
            });
            GOLOS_CHECK_PARAM_VALIDATE(props);
        }

        struct chain_properties_validator {
            using result_type = void;

            template <typename Props>
            void operator()(const Props& p) const {
                p.validate();
            }
        };

        void chain_properties_update_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(owner);
            GOLOS_CHECK_PARAM(props, props.visit(chain_properties_validator()));
        }

        void account_witness_vote_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(account);
            GOLOS_CHECK_PARAM_ACCOUNT(witness);
        }

        void account_witness_proxy_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(account);
            if (proxy.size()) {
                GOLOS_CHECK_PARAM(proxy, {
                    validate_account_name(proxy);
                    GOLOS_CHECK_VALUE(proxy != account, "Cannot proxy to self");
                });
            }
        }

        void custom_operation::validate() const {
            /// required auth accounts are the ones whose bandwidth is consumed
            GOLOS_CHECK_PARAM(required_auths,
                GOLOS_CHECK_VALUE(required_auths.size() > 0, "at least one account must be specified"));
        }

        void custom_json_operation::validate() const {
            /// required auth accounts are the ones whose bandwidth is consumed
            GOLOS_CHECK_PARAM(required_auths,
                GOLOS_CHECK_VALUE((required_auths.size() + required_posting_auths.size()) > 0, "at least on account must be specified"));
            GOLOS_CHECK_PARAM(id, GOLOS_CHECK_VALUE_MAX_SIZE(id, 32));
            GOLOS_CHECK_PARAM(json, {
                GOLOS_CHECK_VALUE_UTF8(json);
                GOLOS_CHECK_VALUE_JSON(json);
            });
        }

        void custom_binary_operation::validate() const {
            /// required auth accounts are the ones whose bandwidth is consumed
            GOLOS_CHECK_PARAM(required_owner_auths,
                GOLOS_CHECK_VALUE((required_owner_auths.size() + required_active_auths.size() + required_posting_auths.size()) > 0,
                    "at least one account must be specified"));
            GOLOS_CHECK_PARAM(id, GOLOS_CHECK_VALUE_MAX_SIZE(id, 32));
            GOLOS_CHECK_PARAM(required_auths, {
                for (const auto &a : required_auths) {
                    a.validate();
                }
            });
        }


        fc::sha256 pow_operation::work_input() const {
            auto hash = fc::sha256::hash(block_id);
            hash._hash[0] = nonce;
            return fc::sha256::hash(hash);
        }

        void pow_operation::validate() const {
            props.validate();
            GOLOS_CHECK_PARAM_ACCOUNT(worker_account);
            GOLOS_CHECK_PARAM(work, {
                GOLOS_CHECK_VALUE(work_input() == work.input, "Determninistic input does not match recorded input");
                work.validate();
            });
        }

        struct pow2_operation_validate_visitor {
            typedef void result_type;

            template<typename PowType>
            void operator()(const PowType &pow) const {
                pow.validate();
            }
        };

        void pow2_operation::validate() const {
            props.validate();
            work.visit(pow2_operation_validate_visitor());
        }

        struct pow2_operation_get_required_active_visitor {
            typedef void result_type;

            pow2_operation_get_required_active_visitor(flat_set<account_name_type> &required_active)
                    : _required_active(required_active) {
            }

            template<typename PowType>
            void operator()(const PowType &work) const {
                _required_active.insert(work.input.worker_account);
            }

            flat_set<account_name_type> &_required_active;
        };

        void pow2_operation::get_required_active_authorities(flat_set<account_name_type> &a) const {
            if (!new_owner_key) {
                pow2_operation_get_required_active_visitor vtor(a);
                work.visit(vtor);
            }
        }

        void pow::create(const fc::ecc::private_key &w, const digest_type &i) {
            input = i;
            signature = w.sign_compact(input, false);

            auto sig_hash = fc::sha256::hash(signature);
            public_key_type recover = fc::ecc::public_key(signature, sig_hash, false);

            work = fc::sha256::hash(recover);
        }

        void pow2::create(const block_id_type &prev, const account_name_type &account_name, uint64_t n) {
            input.worker_account = account_name;
            input.prev_block = prev;
            input.nonce = n;

            auto prv_key = fc::sha256::hash(input);
            auto input = fc::sha256::hash(prv_key);
            auto signature = fc::ecc::private_key::regenerate(prv_key).sign_compact(input);

            auto sig_hash = fc::sha256::hash(signature);
            public_key_type recover = fc::ecc::public_key(signature, sig_hash);

            fc::sha256 work = fc::sha256::hash(std::make_pair(input, recover));
            pow_summary = work.approx_log_32();
        }

        void equihash_pow::create(const block_id_type &recent_block, const account_name_type &account_name, uint32_t nonce) {
            input.worker_account = account_name;
            input.prev_block = recent_block;
            input.nonce = nonce;

            auto seed = fc::sha256::hash(input);
            proof = fc::equihash::proof::hash(STEEMIT_EQUIHASH_N, STEEMIT_EQUIHASH_K, seed);
            pow_summary = fc::sha256::hash(proof.inputs).approx_log_32();
        }

        void pow::validate() const {
            GOLOS_CHECK_VALUE(work != fc::sha256(), "Work must not equal empty hash value");
            GOLOS_CHECK_VALUE(worker == public_key_type(fc::ecc::public_key(signature, input, false)),
                    "Work doesn't match to worker");
            auto sig_hash = fc::sha256::hash(signature);
            public_key_type recover = fc::ecc::public_key(signature, sig_hash, false);
            GOLOS_CHECK_VALUE(work == fc::sha256::hash(recover), "Invalid work result");
        }

        void pow2::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(input.worker_account);
            pow2 tmp;
            tmp.create(input.prev_block, input.worker_account, input.nonce);
            GOLOS_CHECK_PARAM(pow_summary,
                GOLOS_CHECK_VALUE(pow_summary == tmp.pow_summary, "reported work does not match calculated work"));
        }

        void equihash_pow::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(input.worker_account);
            auto seed = fc::sha256::hash(input);
            GOLOS_CHECK_PARAM(proof, {
                    GOLOS_CHECK_VALUE(proof.n == STEEMIT_EQUIHASH_N, "proof of work 'n' value is incorrect");
                    GOLOS_CHECK_VALUE(proof.k == STEEMIT_EQUIHASH_K, "proof of work 'k' value is incorrect");
                    GOLOS_CHECK_VALUE(proof.seed == seed, "proof of work seed does not match expected seed");
                    GOLOS_CHECK_VALUE(proof.is_valid(), "proof of work is not a solution", ("block_id", input.prev_block)("worker_account", input.worker_account)("nonce", input.nonce));
            });
            GOLOS_CHECK_PARAM(pow_summary,
                    GOLOS_CHECK_VALUE_EQ(pow_summary, fc::sha256::hash(proof.inputs).approx_log_32()));
        }

        void feed_publish_operation::validate() const {
            GOLOS_CHECK_PARAM(publisher, validate_account_name(publisher));
            GOLOS_CHECK_LOGIC((is_asset_type(exchange_rate.base, STEEM_SYMBOL) &&
                       is_asset_type(exchange_rate.quote, SBD_SYMBOL))
                      || (is_asset_type(exchange_rate.base, SBD_SYMBOL) &&
                          is_asset_type(exchange_rate.quote, STEEM_SYMBOL)),
                    logic_exception::price_feed_must_be_for_golos_gbg_market,
                    "Price feed must be a GOLOS/GBG price");
            GOLOS_CHECK_PARAM(exchange_rate, exchange_rate.validate());
        }

        void limit_order_create_operation::validate() const {
            GOLOS_CHECK_PARAM(owner, validate_account_name(owner));
            GOLOS_CHECK_LOGIC((is_asset_type(amount_to_sell, STEEM_SYMBOL) &&
                       is_asset_type(min_to_receive, SBD_SYMBOL))
                      || (is_asset_type(amount_to_sell, SBD_SYMBOL) &&
                          is_asset_type(min_to_receive, STEEM_SYMBOL)),
                    logic_exception::limit_order_must_be_for_golos_gbg_market,
                    "Limit order must be for the GOLOS:GBG market");

            auto price = (amount_to_sell / min_to_receive);
            GOLOS_CHECK_PARAM(price, price.validate());
        }

        void limit_order_create2_operation::validate() const {
            GOLOS_CHECK_PARAM(owner, validate_account_name(owner));
            GOLOS_CHECK_PARAM(exchange_rate, exchange_rate.validate());

            GOLOS_CHECK_LOGIC((is_asset_type(amount_to_sell, STEEM_SYMBOL) &&
                       is_asset_type(exchange_rate.quote, SBD_SYMBOL)) ||
                      (is_asset_type(amount_to_sell, SBD_SYMBOL) &&
                       is_asset_type(exchange_rate.quote, STEEM_SYMBOL)),
                    logic_exception::limit_order_must_be_for_golos_gbg_market,
                    "Limit order must be for the GOLOS:GBG market");

            GOLOS_CHECK_PARAM(amount_to_sell, {
                GOLOS_CHECK_VALUE(amount_to_sell.symbol == exchange_rate.base.symbol,
                        "Sell asset must be the base of the price");
                GOLOS_CHECK_VALUE((amount_to_sell * exchange_rate).amount > 0,
                        "Amount to sell cannot round to 0 when traded");
            });
        }

        void limit_order_cancel_operation::validate() const {
            GOLOS_CHECK_PARAM(owner, validate_account_name(owner));
        }

        void convert_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(owner);
            /// only allow conversion from GBG to GOLOS, allowing the opposite can enable traders to abuse
            /// market fluxuations through converting large quantities without moving the price.
            GOLOS_CHECK_PARAM(amount, GOLOS_CHECK_ASSET_GT0(amount, GBG));
        }

        void report_over_production_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(reporter);
            GOLOS_CHECK_PARAM_ACCOUNT(first_block.witness);
            GOLOS_CHECK_PARAM(first_block, {
                GOLOS_CHECK_VALUE(first_block.witness == second_block.witness, "Witness for first and second blocks must be equal");
                GOLOS_CHECK_VALUE(first_block.timestamp == second_block.timestamp, "Timestamp for first and second blocks must be equal");
                GOLOS_CHECK_VALUE(first_block.signee() == second_block.signee(), "Signee for first and second blocks must be equal");
                GOLOS_CHECK_VALUE(first_block.id() != second_block.id(), "ID for first and second blocks must be different");
            });
        }

        void escrow_transfer_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from);
            GOLOS_CHECK_PARAM_ACCOUNT(to);
            GOLOS_CHECK_PARAM_ACCOUNT(agent);
            GOLOS_CHECK_PARAM(fee, GOLOS_CHECK_ASSET_GE0(fee, GOLOS_OR_GBG));
            GOLOS_CHECK_PARAM(sbd_amount, GOLOS_CHECK_ASSET_GE0(sbd_amount, GBG));
            GOLOS_CHECK_PARAM(steem_amount, GOLOS_CHECK_ASSET_GE0(steem_amount, GOLOS));
            GOLOS_CHECK_LOGIC(sbd_amount.amount > 0 || steem_amount.amount > 0,
                logic_exception::escrow_no_amount_set,
                "escrow must release a non-zero amount");
            GOLOS_CHECK_PARAM(agent, GOLOS_CHECK_VALUE(from != agent && to != agent, "agent must be a third party"));
            GOLOS_CHECK_LOGIC(ratification_deadline < escrow_expiration,
                logic_exception::escrow_wrong_time_limits,
                "ratification deadline must be before escrow expiration");
            if (json_meta.size() > 0) {
                GOLOS_CHECK_PARAM(json_meta, {
                    GOLOS_CHECK_VALUE_UTF8(json_meta);
                    GOLOS_CHECK_VALUE_JSON(json_meta);
                });
            }
        }

        void escrow_approve_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from);
            GOLOS_CHECK_PARAM_ACCOUNT(to);
            GOLOS_CHECK_PARAM_ACCOUNT(agent);
            GOLOS_CHECK_PARAM_ACCOUNT(who);
            GOLOS_CHECK_PARAM(who, GOLOS_CHECK_VALUE(who == to || who == agent, "to or agent must approve escrow"));
        }

        void escrow_dispute_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from);
            GOLOS_CHECK_PARAM_ACCOUNT(to);
            GOLOS_CHECK_PARAM_ACCOUNT(agent);
            GOLOS_CHECK_PARAM_ACCOUNT(who);
            GOLOS_CHECK_PARAM(who, GOLOS_CHECK_VALUE(who == from || who == to, "who must be from or to"));
        }

        void escrow_release_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from);
            GOLOS_CHECK_PARAM_ACCOUNT(to);
            GOLOS_CHECK_PARAM_ACCOUNT(agent);
            GOLOS_CHECK_PARAM_ACCOUNT(who);
            GOLOS_CHECK_PARAM_ACCOUNT(receiver);
            GOLOS_CHECK_PARAM(who,
                GOLOS_CHECK_VALUE(who == from || who == to || who == agent, "who must be from or to or agent"));
            GOLOS_CHECK_PARAM(receiver,
                GOLOS_CHECK_VALUE(receiver == from || receiver == to, "receiver must be from or to"));
            GOLOS_CHECK_PARAM(sbd_amount, GOLOS_CHECK_ASSET_GE0(sbd_amount, GBG));
            GOLOS_CHECK_PARAM(steem_amount, GOLOS_CHECK_ASSET_GE0(steem_amount, GOLOS));
            GOLOS_CHECK_LOGIC(sbd_amount.amount > 0 || steem_amount.amount > 0,
                logic_exception::escrow_no_amount_set,
                "escrow must release a non-zero amount");
        }

        void request_account_recovery_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(recovery_account);
            GOLOS_CHECK_PARAM_ACCOUNT(account_to_recover);
            GOLOS_CHECK_PARAM_VALIDATE(new_owner_authority);
        }

        void recover_account_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(account_to_recover);
            GOLOS_CHECK_PARAM(new_owner_authority, {
                GOLOS_CHECK_LOGIC(!(new_owner_authority == recent_owner_authority),
                    logic_exception::cannot_set_recent_recovery,
                    "Cannot set new owner authority to the recent owner authority");
                GOLOS_CHECK_VALUE(!new_owner_authority.is_impossible(), "new owner authority cannot be impossible");
                GOLOS_CHECK_VALUE(new_owner_authority.weight_threshold, "new owner authority cannot be trivial");
                new_owner_authority.validate();
            });
            GOLOS_CHECK_PARAM(recent_owner_authority, {
                GOLOS_CHECK_VALUE(!recent_owner_authority.is_impossible(), "recent owner authority cannot be impossible");
                recent_owner_authority.validate();
            });
        }

        void change_recovery_account_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(account_to_recover);
            GOLOS_CHECK_PARAM_ACCOUNT(new_recovery_account);
        }

        void transfer_to_savings_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from);
            GOLOS_CHECK_PARAM_ACCOUNT(to);
            GOLOS_CHECK_PARAM(amount, GOLOS_CHECK_ASSET_GT0(amount, GOLOS_OR_GBG));
            GOLOS_CHECK_PARAM(memo, {
                GOLOS_CHECK_VALUE_MAX_SIZE(memo, STEEMIT_MAX_MEMO_SIZE - 1); //-1 to satisfy <= check (vs <)
                GOLOS_CHECK_VALUE_UTF8(memo);
            });
        }

        void transfer_from_savings_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from);
            GOLOS_CHECK_PARAM_ACCOUNT(to);
            GOLOS_CHECK_PARAM(amount, GOLOS_CHECK_ASSET_GT0(amount, GOLOS_OR_GBG));
            GOLOS_CHECK_PARAM(memo, {
                GOLOS_CHECK_VALUE_MAX_SIZE(memo, STEEMIT_MAX_MEMO_SIZE - 1); //-1 to satisfy <= check. TODO: unify <=/<
                GOLOS_CHECK_VALUE_UTF8(memo);
            });
        }

        void cancel_transfer_from_savings_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(from);
        }

        void decline_voting_rights_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(account);
        }

        void reset_account_operation::validate() const {
            // This op is disabled. Maybe just remove it completely?
            GOLOS_CHECK_PARAM_ACCOUNT(reset_account);
            GOLOS_CHECK_PARAM_ACCOUNT(account_to_reset);
            GOLOS_CHECK_PARAM(new_owner_authority, {
                GOLOS_CHECK_VALUE(!new_owner_authority.is_impossible(), "New owner authority cannot be impossible");
                GOLOS_CHECK_VALUE(new_owner_authority.weight_threshold, "New owner authority cannot be trivial");
                new_owner_authority.validate();
            });
        }

        void set_reset_account_operation::validate() const {
            // This op is disabled. Maybe just remove it completely?
            GOLOS_CHECK_PARAM_ACCOUNT(account);
            if (current_reset_account.size()) {
                GOLOS_CHECK_PARAM_ACCOUNT(current_reset_account);
            }
            GOLOS_CHECK_PARAM_ACCOUNT(reset_account);
            GOLOS_CHECK_LOGIC(current_reset_account != reset_account,
                logic_exception::cannot_set_same_reset_account,
                "New reset account cannot be current reset account");
        }

        void delegate_vesting_shares_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(delegator);
            GOLOS_CHECK_PARAM_ACCOUNT(delegatee);
            GOLOS_CHECK_LOGIC(delegator != delegatee, logic_exception::cannot_delegate_to_yourself,
                "You cannot delegate GESTS to yourself");
            GOLOS_CHECK_PARAM(vesting_shares, GOLOS_CHECK_ASSET_GE0(vesting_shares, GESTS));
        }

        void break_free_referral_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(referral);
        }

        void delegate_vesting_shares_with_interest_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(delegator);
            GOLOS_CHECK_PARAM_ACCOUNT(delegatee);
            GOLOS_CHECK_LOGIC(delegator != delegatee, logic_exception::cannot_delegate_to_yourself,
                "You cannot delegate GESTS to yourself");
            GOLOS_CHECK_PARAM(vesting_shares, GOLOS_CHECK_ASSET_GE0(vesting_shares, GESTS));
        }

        void reject_vesting_shares_delegation_operation::validate() const {
            GOLOS_CHECK_PARAM_ACCOUNT(delegator);
            GOLOS_CHECK_PARAM_ACCOUNT(delegatee);
        }

} } // golos::protocol
