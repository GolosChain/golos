#include <golos/chain/database.hpp>

#include <golos/chain/objects/account_object.hpp>

namespace golos {
    namespace chain {
        share_type cut_fee(share_type a, uint16_t p) {
            if (a == 0 || p == 0) {
                return 0;
            }
            if (p == STEEMIT_100_PERCENT) {
                return a;
            }

            fc::uint128 r(a.value);
            r *= p;
            r /= STEEMIT_100_PERCENT;
            return r.to_uint64();
        }

        void account_balance_object::adjust_balance(const protocol::asset<0, 17, 0> &delta) {
            assert(delta.symbol == asset_name);
            balance += delta.amount;
        }

        void account_statistics_object::pay_fee(share_type core_fee, share_type cashback_vesting_threshold) {
            if (core_fee > cashback_vesting_threshold) {
                pending_fees += core_fee;
            } else {
                pending_vested_fees += core_fee;
            }
        }

//        set<account_name_type> account_member_index::get_account_members(const value_type &a) const {
//            set<account_name_type> result;
//            for (auto auth : a.owner.account_auths) {
//                result.insert(auth.first);
//            }
//            for (auto auth : a.active.account_auths) {
//                result.insert(auth.first);
//            }
//            for (auto auth : a.posting.account_auths) {
//                result.insert(auth.first);
//            }
//            return result;
//        }
//
//        set<public_key_type> account_member_index::get_key_members(const value_type &a) const {
//            set<public_key_type> result;
//            for (auto auth : a.owner.key_auths) {
//                result.insert(auth.first);
//            }
//            for (auto auth : a.active.key_auths) {
//                result.insert(auth.first);
//            }
//            for (auto auth : a.posting.key_auths) {
//                result.insert(auth.first);
//            }
////            result.insert(a.options.memo_key);
//            return result;
//        }
//
//        void account_member_index::object_inserted(const value_type &obj) {
//            auto account_members = get_account_members(obj);
//            for (auto item : account_members) {
//                account_to_account_memberships[item].insert(obj.account);
//            }
//
//            auto key_members = get_key_members(obj);
//            for (auto item : key_members) {
//                account_to_key_memberships[item].insert(obj.account);
//            }
//        }
//
//        void account_member_index::object_removed(const value_type &obj) {
//            auto key_members = get_key_members(obj);
//            for (auto item : key_members) {
//                account_to_key_memberships[item].erase(obj.account);
//            }
//
//            auto account_members = get_account_members(obj);
//            for (auto item : account_members) {
//                account_to_account_memberships[item].erase(obj.account);
//            }
//        }
//
//        void account_member_index::about_to_modify(const value_type &before) {
//            before_key_members.clear();
//            before_account_members.clear();
//            before_key_members = get_key_members(before);
//            before_account_members = get_account_members(before);
//        }
//
//        void account_member_index::object_modified(const value_type &after) {
//            {
//                set<account_name_type> after_account_members = get_account_members(after);
//                vector<account_name_type> removed;
//                removed.reserve(before_account_members.size());
//                std::set_difference(before_account_members.begin(), before_account_members.end(),
//                        after_account_members.begin(), after_account_members.end(),
//                        std::inserter(removed, removed.end()));
//
//                for (auto itr = removed.begin(); itr != removed.end(); ++itr) {
//                    account_to_account_memberships[*itr].erase(after.account);
//                }
//
//                vector<account_name_type> added;
//                added.reserve(after_account_members.size());
//                std::set_difference(after_account_members.begin(), after_account_members.end(),
//                        before_account_members.begin(), before_account_members.end(),
//                        std::inserter(added, added.end()));
//
//                for (auto itr = added.begin(); itr != added.end(); ++itr) {
//                    account_to_account_memberships[*itr].insert(after.account);
//                }
//            }
//
//
//            {
//                set<public_key_type> after_key_members = get_key_members(after);
//
//                vector<public_key_type> removed;
//                removed.reserve(before_key_members.size());
//                std::set_difference(before_key_members.begin(), before_key_members.end(),
//                        after_key_members.begin(), after_key_members.end(),
//                        std::inserter(removed, removed.end()));
//
//                for (auto itr = removed.begin(); itr != removed.end(); ++itr) {
//                    account_to_key_memberships[*itr].erase(after.account);
//                }
//
//                vector<public_key_type> added;
//                added.reserve(after_key_members.size());
//                std::set_difference(after_key_members.begin(), after_key_members.end(),
//                        before_key_members.begin(), before_key_members.end(),
//                        std::inserter(added, added.end()));
//
//                for (auto itr = added.begin(); itr != added.end(); ++itr) {
//                    account_to_key_memberships[*itr].insert(after.account);
//                }
//            }
//        }
//
//        void account_referrer_index::object_inserted(const value_type &obj) {
//        }
//
//        void account_referrer_index::object_removed(const value_type &obj) {
//        }
//
//        void account_referrer_index::about_to_modify(const value_type &before) {
//        }
//
//        void account_referrer_index::object_modified(const value_type &after) {
//        }
    }
}