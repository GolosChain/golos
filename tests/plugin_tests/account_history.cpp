#include <string>
#include <cstdint>

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include "database_fixture.hpp"
#include "comment_reward.hpp"


using golos::chain::account_name_set;
using golos::plugins::json_rpc::msg_pack;


struct account_history_fixture : public golos::chain::add_operations_database_fixture {
    typedef std::map<uint32_t, std::set<std::string>> checked_accounts_map; ///<  pair { [block], [accaunt names] }

    checked_accounts_map check(const account_name_set& names) {
        uint32_t head_block_num = db->head_block_num();
        ilog("Check history accounts, block num is " + std::to_string(head_block_num));
        checked_accounts_map _founded_accs;
        for (auto n : names) {
            msg_pack mp;
            mp.args = std::vector<fc::variant>({fc::variant(n), fc::variant(100), fc::variant(100)});
            auto accs = ah_plugin->get_account_history(mp);
            for (auto a : accs) {
                auto it = _founded_accs.find(a.second.block);
                if (it == _founded_accs.end()) {
                    std::set<std::string> set;
                    set.insert(n);
                    _founded_accs.insert(std::make_pair(a.second.block, set));
                } else {
                    it->second.insert(n);
                }
            }
        }
        return _founded_accs;
    }
};


BOOST_FIXTURE_TEST_SUITE(account_history_plugin, account_history_fixture)

BOOST_AUTO_TEST_CASE(account_history_blocks) {
    BOOST_TEST_MESSAGE("Testing: account_history_blocks");
    const uint32_t HISTORY_BLOCKS = 3;
    initialize({{"history-blocks", std::to_string(HISTORY_BLOCKS)}});
    add_operations();

    account_name_set names = {"alice", "bob", "sam", "dave"};
    auto _founded_accs = check(names);

    std::set<uint32_t> blocks;
    for (auto a : _founded_accs) {
        for (auto n : a.second) {
            ilog("block:" + std::to_string(a.first) + ", \"" + n + "\"");
            auto iter = names.find(n);
            bool is_found = (iter != names.end());
            BOOST_CHECK(is_found);
            if (is_found) {
                blocks.insert(a.first);
            } else {
                BOOST_TEST_MESSAGE("Account [" + std::to_string(a.first) + "]: \"" + n + "\" is not found");
            }
        }
    }
    BOOST_CHECK_EQUAL(blocks.size(), HISTORY_BLOCKS);
}

BOOST_AUTO_TEST_SUITE_END()
