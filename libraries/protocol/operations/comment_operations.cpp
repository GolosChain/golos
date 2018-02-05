#include <golos/protocol/operations/comment_operations.hpp>

#include <fc/utf8.hpp>

namespace golos {
    namespace protocol {
        /// TODO: after the hardfork, we can rename this method validate_permlink because it is strictily less restrictive than before
        ///  Issue #56 contains the justificiation for allowing any UTF-8 string to serve as a permlink, content will be grouped by tags
        ///  going forward.
        inline void validate_permlink(const string &permlink) {
            FC_ASSERT(permlink.size() < STEEMIT_MAX_PERMLINK_LENGTH, "permlink is too long");
            FC_ASSERT(fc::is_utf8(permlink), "permlink not formatted in UTF8");
        }

        inline void validate_account_name(const string &name) {
            FC_ASSERT(is_valid_account_name(name), "Account name ${n} is invalid", ("n", name));
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void comment_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(this->title.size() < 256, "Title larger than size limit");
            FC_ASSERT(fc::is_utf8(this->title), "Title not formatted in UTF8");
            FC_ASSERT(this->body.size() > 0, "Body is empty");
            FC_ASSERT(fc::is_utf8(this->body), "Body not formatted in UTF8");


            if (parent_author.size()) {
                validate_account_name(parent_author);
            }
            validate_account_name(author);
            validate_permlink(parent_permlink);
            validate_permlink(permlink);
        }

        struct comment_options_extension_validate_visitor {
            comment_options_extension_validate_visitor() {
            }

            typedef void result_type;

            void operator()(const comment_payout_beneficiaries &cpb) const {
                cpb.validate();
            }
        };

        void comment_payout_beneficiaries::validate() const {
            uint32_t sum = 0;

            FC_ASSERT(beneficiaries.size(), "Must specify at least one beneficiary");
            FC_ASSERT(beneficiaries.size() < 128,
                      "Cannot specify more than 127 beneficiaries."); // Require size serialization fits in one byte.

            validate_account_name(beneficiaries[0].account);
            FC_ASSERT(beneficiaries[0].weight <= STEEMIT_100_PERCENT,
                      "Cannot allocate more than 100% of rewards to one account");
            sum += beneficiaries[0].weight;
            FC_ASSERT(sum <= STEEMIT_100_PERCENT,
                      "Cannot allocate more than 100% of rewards to a comment"); // Have to check incrementally to avoid overflow

            for (size_t i = 1; i < beneficiaries.size(); i++) {
                validate_account_name(beneficiaries[i].account);
                FC_ASSERT(beneficiaries[i].weight <= STEEMIT_100_PERCENT,
                          "Cannot allocate more than 100% of rewards to one account");
                sum += beneficiaries[i].weight;

                FC_ASSERT(sum <= STEEMIT_100_PERCENT,
                          "Cannot allocate more than 100% of rewards to a comment"); // Have to check incrementally to avoid overflow
                FC_ASSERT(beneficiaries[i - 1] < beneficiaries[i],
                          "Benficiaries must be specified in sorted order (account ascending)");
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void comment_payout_extension_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(this->payer);
            validate_account_name(this->author);
            validate_permlink(this->permlink);

            FC_ASSERT((this->amount || this->extension_time) && !(this->amount && this->extension_time),
                      "Payout window can be extended by required SBD amount or by required time amount");

            if (this->amount) {
                FC_ASSERT(this->amount->symbol_name() == SBD_SYMBOL_NAME,
                          "Payout window extension is only available with SBD");
                FC_ASSERT(this->amount->amount > 0, "Cannot extend payout window with 0 SBD");
            }

            if (this->extension_time) {
                FC_ASSERT(*this->extension_time <= fc::time_point_sec(STEEMIT_CASHOUT_WINDOW_SECONDS),
                          "Payout window extension cannot be larger than a week");
                FC_ASSERT(*this->extension_time > fc::time_point_sec(0),
                          "Payout window extension cannot be extended for 0 seconds");
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void comment_options_operation<Major, Hardfork, Release>::validate() const {
            validate_account_name(this->author);
            FC_ASSERT(this->percent_steem_dollars <= STEEMIT_100_PERCENT, "Percent cannot exceed 100%");
            FC_ASSERT(this->max_accepted_payout.symbol_name() == SBD_SYMBOL_NAME, "Max accepted payout must be in SBD");
            FC_ASSERT(this->max_accepted_payout.amount.value >= 0, "Cannot accept less than 0 payout");
            validate_permlink(this->permlink);
            for (auto &e : this->extensions) {
                e.visit(comment_options_extension_validate_visitor());
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void delete_comment_operation<Major, Hardfork, Release>::validate() const {
      validate_permlink( permlink );
      validate_account_name( author );
   }
    }
}

#include <golos/protocol/operations/comment_operations.tpp>