#include <cstdlib>
#include <iostream>

#ifdef BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE unit_tests

#include <boost/test/unit_test.hpp>

#else
#include <boost/test/included/unit_test.hpp>
#endif

// extern uint32_t STEEMIT_TESTING_GENESIS_TIMESTAMP;

boost::unit_test::test_suite *init_unit_test_suite(int argc, char *argv[]) {
    std::srand(time(NULL));
    std::cout << "Random number generator seeded to " << time(NULL) << std::endl;
    /*
       const char* genesis_timestamp_str = getenv("STEEMIT_TESTING_GENESIS_TIMESTAMP");
       if( genesis_timestamp_str != nullptr )
       {
          STEEMIT_TESTING_GENESIS_TIMESTAMP = std::stoul( genesis_timestamp_str );
       }
       std::cout << "STEEMIT_TESTING_GENESIS_TIMESTAMP is " << STEEMIT_TESTING_GENESIS_TIMESTAMP << std::endl;
    */
    return nullptr;
}