#include <golos/chain/database.hpp>
#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/proposal_object.hpp>

namespace golos {
    namespace chain {

        bool proposal_object::is_authorized_to_execute(database &db) const {
            transaction_evaluation_state dry_run_eval(&db);

            try {
                verify_authority(proposed_transaction.operations,
                        available_key_approvals,
                        [&](std::string account_name) {
                            return authority(db.get<account_authority_object, by_account>(account_name).active);
                        },
                        [&](std::string account_name) {
                            return authority(db.get<account_authority_object, by_account>(account_name).owner);
                        },
                        [&](std::string account_name) {
                            return authority(db.get<account_authority_object, by_account>(account_name).posting);
                        },
                        STEEMIT_MAX_SIG_CHECK_DEPTH,
                        true, /* allow committeee */
                        available_active_approvals,
                        available_owner_approvals,
                        available_posting_approvals);
            }
            catch (const fc::exception &e) {
                //idump((available_active_approvals));
                //wlog((e.to_detail_string()));
                return false;
            }
            return true;
        }


//        void required_approval_index::object_inserted(const value_type &obj) {
//            for (const auto &a : obj.required_active_approvals) {
//                account_to_proposals[a].insert(obj.id);
//            }
//            for (const auto &a : obj.required_owner_approvals) {
//                account_to_proposals[a].insert(obj.id);
//            }
//            for (const auto &a : obj.required_posting_approvals) {
//                account_to_proposals[a].insert(obj.id);
//            }
//            for (const auto &a : obj.available_active_approvals) {
//                account_to_proposals[a].insert(obj.id);
//            }
//            for (const auto &a : obj.available_owner_approvals) {
//                account_to_proposals[a].insert(obj.id);
//            }
//            for (const auto &a : obj.available_posting_approvals) {
//                account_to_proposals[a].insert(obj.id);
//            }
//        }
//
//        void required_approval_index::remove(account_name_type a, proposal_object::id_type p) {
//            auto itr = account_to_proposals.find(a);
//            if (itr != account_to_proposals.end()) {
//                itr->second.erase(p);
//                if (itr->second.empty()) {
//                    account_to_proposals.erase(itr->first);
//                }
//            }
//        }
//
//        void required_approval_index::object_removed(const value_type &obj) {
//            for (const auto &a : obj.required_active_approvals) {
//                remove(a, obj.id);
//            }
//            for (const auto &a : obj.required_owner_approvals) {
//                remove(a, obj.id);
//            }
//            for (const auto &a : obj.required_posting_approvals) {
//                remove(a, obj.id);
//            }
//            for (const auto &a : obj.available_active_approvals) {
//                remove(a, obj.id);
//            }
//            for (const auto &a : obj.available_owner_approvals) {
//                remove(a, obj.id);
//            }
//            for (const auto &a : obj.available_posting_approvals) {
//                remove(a, obj.id);
//            }
//        }
    }
}