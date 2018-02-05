#include <golos/protocol/operations/proposal_operations.hpp>
#include <golos/protocol/operations/operations.hpp>
#include <golos/protocol/types.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/exception/exception.hpp>
#include <fc/time.hpp>

namespace golos {
    namespace protocol {
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_create_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(!proposed_operations.empty());
            for (const auto &op : proposed_operations) {
                operation_validate(op.op);
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_update_operation<Major, Hardfork, Release>::validate() const {
            FC_ASSERT(!(active_approvals_to_add.empty() && active_approvals_to_remove.empty() &&
                        owner_approvals_to_add.empty() && owner_approvals_to_remove.empty() &&
                        posting_approvals_to_add.empty() && posting_approvals_to_remove.empty() &&
                        key_approvals_to_add.empty() && key_approvals_to_remove.empty()));
            for (auto a : active_approvals_to_add) {
                FC_ASSERT(active_approvals_to_remove.find(a) == active_approvals_to_remove.end(),
                          "Cannot add and remove approval at the same time.");
            }
            for (auto a : owner_approvals_to_add) {
                FC_ASSERT(owner_approvals_to_remove.find(a) == owner_approvals_to_remove.end(),
                          "Cannot add and remove approval at the same time.");
            }
            for (auto a : posting_approvals_to_add) {
                FC_ASSERT(posting_approvals_to_remove.find(a) == posting_approvals_to_remove.end(),
                          "Cannot add and remove approval at the same time.");
            }
            for (auto a : key_approvals_to_add) {
                FC_ASSERT(key_approvals_to_remove.find(a) == key_approvals_to_remove.end(),
                          "Cannot add and remove approval at the same time.");
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_delete_operation<Major, Hardfork, Release>::validate() const {

        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_update_operation<Major, Hardfork, Release>::get_required_authorities(vector<authority> &o) const {
            authority auth;
            for (const auto &k : key_approvals_to_add) {
                auth.key_auths[k] = 1;
            }
            for (const auto &k : key_approvals_to_remove) {
                auth.key_auths[k] = 1;
            }
            auth.weight_threshold = auth.key_auths.size();

            if (auth.key_auths.size() > 0) {
                o.emplace_back(std::move(auth));
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_update_operation<Major, Hardfork, Release>::get_required_active_authorities(
                flat_set<account_name_type> &a) const {
            for (const auto &i : active_approvals_to_add) {
                a.insert(i);
            }
            for (const auto &i : active_approvals_to_remove) {
                a.insert(i);
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void proposal_update_operation<Major, Hardfork, Release>::get_required_owner_authorities(
                flat_set<account_name_type> &a) const {
            for (const auto &i : owner_approvals_to_add) {
                a.insert(i);
            }
            for (const auto &i : owner_approvals_to_remove) {
                a.insert(i);
            }
        }
    }
} // golos::chain

#include <golos/protocol/operations/proposal_operations.tpp>