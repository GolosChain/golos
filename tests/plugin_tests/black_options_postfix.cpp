#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include "database_fixture.hpp"
#include "comment_reward.hpp"
#include "options_fixture.hpp"


using namespace golos::test;


struct blacklist_key {
    std::string key = "history-blacklist-ops";
};


BOOST_FIXTURE_TEST_CASE(black_options_postfix, operation_options_fixture) {
    init_plugin(test_options<combine_postfix<blacklist_key>>());

    size_t _chacked_ops_count = 0;
    for (const auto &co : _db_init._added_ops) {
        auto iter = _founded_ops.find(co.first);
        bool is_not_found = (iter == _founded_ops.end());
        BOOST_CHECK(is_not_found);
        if (is_not_found) {
            ++_chacked_ops_count;
        } else {
            BOOST_TEST_MESSAGE("Operation \"" + co.second + "\" by \"" + co.first + "\" is found");
        }
    }
    BOOST_CHECK_EQUAL(_chacked_ops_count, _db_init._added_ops.size());
}
