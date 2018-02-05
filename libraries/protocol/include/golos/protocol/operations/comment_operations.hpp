#ifndef GOLOS_COMMENT_OPERATIONS_HPP
#define GOLOS_COMMENT_OPERATIONS_HPP

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

namespace golos {
    namespace protocol {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct comment_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type parent_author;
            string parent_permlink;

            account_name_type author;
            string permlink;
            string title;
            string body;
            string json_metadata;

            void validate() const;

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                a.insert(author);
            }
        };

        struct beneficiary_route_type {
            beneficiary_route_type() {
            }

            beneficiary_route_type(const account_name_type &a, const uint16_t &w) : account(a), weight(w) {
            }

            account_name_type account;
            uint16_t weight;

            // For use by std::sort such that the route is sorted first by name (ascending)
            bool operator<(const beneficiary_route_type &o) const {
                return string_less()(account, o.account);
            }
        };

        struct comment_payout_beneficiaries {
            vector<beneficiary_route_type> beneficiaries;

            void validate() const;
        };

        typedef static_variant<comment_payout_beneficiaries> comment_options_extension;

        typedef flat_set<comment_options_extension> comment_options_extensions_type;


        /**
         *  Authors of posts may not want all of the benefits that come from creating a post. This
         *  operation allows authors to update properties associated with their post.
         *
         *  The max_accepted_payout may be decreased, but never increased.
         *  The percent_steem_dollars may be decreased, but never increased
         *
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct comment_options_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type author;
            string permlink;

            asset<Major, Hardfork, Release> max_accepted_payout = {1000000000, SBD_SYMBOL_NAME};       /// SBD value of the maximum payout this post will receive
            uint16_t percent_steem_dollars = STEEMIT_100_PERCENT; /// the percent of Golos Gold to key, unkept amounts will be received as Golos Power
            bool allow_votes = true;      /// allows a post to receive votes;
            bool allow_curation_rewards = true; /// allows voters to recieve curation rewards. Rewards return to reward fund.
            comment_options_extensions_type extensions;

            void validate() const;

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                a.insert(author);
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct comment_payout_extension_operation
                : public base_operation<Major, Hardfork, Release> {
            account_name_type payer;
            account_name_type author;
            string permlink;

            optional<fc::time_point_sec> extension_time;
            optional<asset<Major, Hardfork, Release>> amount;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                if (amount && amount->symbol_name() == SBD_SYMBOL_NAME) {
                    a.insert(payer);
                }
            }

            void get_required_owner_authorities(flat_set<account_name_type> &a) const {
                if (amount && amount->symbol_name() == SBD_SYMBOL_NAME) {
                    a.insert(payer);
                }
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct delete_comment_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type author;
            string permlink;

            void validate() const;

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                a.insert(author);
            }
        };
    }
}

FC_REFLECT((golos::protocol::comment_operation<0, 16, 0>),
           (parent_author)(parent_permlink)(author)(permlink)(title)(body)(json_metadata))
FC_REFLECT((golos::protocol::comment_operation<0, 17, 0>),
           (parent_author)(parent_permlink)(author)(permlink)(title)(body)(json_metadata))

FC_REFLECT((golos::protocol::delete_comment_operation<0, 16, 0>), (author)(permlink));
FC_REFLECT((golos::protocol::delete_comment_operation<0, 17, 0>), (author)(permlink));

FC_REFLECT((golos::protocol::beneficiary_route_type), (account)(weight))
FC_REFLECT((golos::protocol::comment_payout_beneficiaries), (beneficiaries))

FC_REFLECT_TYPENAME((golos::protocol::comment_options_extension))
FC_REFLECT((golos::protocol::comment_options_operation<0, 16, 0>),
           (author)(permlink)(max_accepted_payout)(percent_steem_dollars)(allow_votes)(allow_curation_rewards)(
                   extensions))
FC_REFLECT((golos::protocol::comment_options_operation<0, 17, 0>),
           (author)(permlink)(max_accepted_payout)(percent_steem_dollars)(allow_votes)(allow_curation_rewards)(
                   extensions))

FC_REFLECT((golos::protocol::comment_payout_extension_operation<0, 17, 0>),
           (payer)(author)(permlink)(extension_time)(amount));

#endif //GOLOS_COMMENT_OPERATIONS_HPP
