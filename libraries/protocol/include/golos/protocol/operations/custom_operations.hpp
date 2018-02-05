#ifndef GOLOS_CUSTOM_OPERATIONS_HPP
#define GOLOS_CUSTOM_OPERATIONS_HPP

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

namespace golos {
    namespace protocol {
        /**
         * @brief provides a generic way to add higher level protocols on top of witness consensus
         * @ingroup operations
         *
         * There is no validation for this operation other than that required auths are valid
         */
        struct custom_operation : public base_operation<0, 17, 0> {
            flat_set <account_name_type> required_auths;
            uint16_t id = 0;
            vector<char> data;

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                for (const auto &i : required_auths) {
                    a.insert(i);
                }
            }
        };


        /** serves the same purpose as custom_operation but also supports required posting authorities. Unlike custom_operation,
         * this operation is designed to be human readable/developer friendly.
         **/
        struct custom_json_operation : public base_operation<0, 17, 0> {
            flat_set <account_name_type> required_auths;
            flat_set <account_name_type> required_posting_auths;
            string id; ///< must be less than 32 characters long
            string json; ///< must be proper utf8 / JSON string.

            void validate() const;

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                for (const auto &i : required_auths) {
                    a.insert(i);
                }
            }

            void get_required_posting_authorities(flat_set <account_name_type> &a) const {
                for (const auto &i : required_posting_auths) {
                    a.insert(i);
                }
            }
        };

        struct custom_binary_operation : public base_operation<0, 17, 0> {
            flat_set <account_name_type> required_owner_auths;
            flat_set <account_name_type> required_active_auths;
            flat_set <account_name_type> required_posting_auths;
            vector <authority> required_auths;

            string id; ///< must be less than 32 characters long
            vector<char> data;

            void validate() const;

            void get_required_owner_authorities(flat_set <account_name_type> &a) const {
                for (const auto &i : required_owner_auths) {
                    a.insert(i);
                }
            }

            void get_required_active_authorities(flat_set <account_name_type> &a) const {
                for (const auto &i : required_active_auths) {
                    a.insert(i);
                }
            }

            void get_required_posting_authorities(flat_set <account_name_type> &a) const {
                for (const auto &i : required_posting_auths) {
                    a.insert(i);
                }
            }

            void get_required_authorities(vector <authority> &a) const {
                for (const auto &i : required_auths) {
                    a.push_back(i);
                }
            }
        };
    }
}

FC_REFLECT((golos::protocol::custom_operation), (required_auths)(id)(data))

FC_REFLECT((golos::protocol::custom_json_operation), (required_auths)(required_posting_auths)(id)(json))

FC_REFLECT((golos::protocol::custom_binary_operation), (required_owner_auths)(required_active_auths)(required_posting_auths)(required_auths)(id)(data))

#endif //GOLOS_CUSTOM_OPERATIONS_HPP
