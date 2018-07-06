#include <string>
#include <cstdint>

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include "database_fixture.hpp"
#include "comment_reward.hpp"
#include "options_fixture.hpp"


using namespace golos::test;


struct whitelist_key {
    std::string key = "history-whitelist-ops";
};


static uint32_t HISTORY_BLOCKS = 2;


struct blocks_limit_key {
    typedef uint32_t opt_type;
    std::string key = "history-blocks";
    std::string opt = std::to_string(HISTORY_BLOCKS);
};


BOOST_FIXTURE_TEST_CASE(operation_history_blocks, operation_options_fixture) {
    auto tops = test_options<combine_postfix<whitelist_key>>();
    tops.fill_options<blocks_limit_key>();
    init_plugin(tops);

    size_t _chacked_ops_count = 0;
    for (auto it = _founded_ops.begin(); it != _founded_ops.end(); ++it) {
        auto iter = _db_init._added_ops.find(it->first);
        bool is_found = (iter != _db_init._added_ops.end());
        BOOST_CHECK(is_found);
        if (is_found) {
            BOOST_CHECK_EQUAL(iter->second, it->second);
            if (iter->second == it->second) {
                ++_chacked_ops_count;
            }
        } else {
            BOOST_TEST_MESSAGE("Operation \"" + it->second + "\" by \"" + it->first + "\" is not found");
        }
    }
    BOOST_CHECK_EQUAL(_chacked_ops_count, HISTORY_BLOCKS);
}
