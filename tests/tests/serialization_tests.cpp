#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>

#include <golos/chain/objects/steem_objects.hpp>
#include <golos/chain/database.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/variant.hpp>

#include "../common/database_fixture.hpp"

#include <cmath>

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

typedef asset<0, 17, 0> latest_asset;

BOOST_FIXTURE_TEST_SUITE(serialization_tests, clean_database_fixture)

    /*
 BOOST_AUTO_TEST_CASE( account_name_type_test )
 {

    auto test = []( const string& data ) {
       fixed_string<> a(data);
       std::string    b(data);

       auto ap = fc::raw::pack( empty );
       auto bp = fc::raw::pack( emptystr );
       FC_ASSERT( ap.size() == bp.size() );
       FC_ASSERT( std::equal( ap.begin(), ap.end(), bp.begin() ) );

       auto sfa = fc::raw::unpack<std::string>( ap );
       auto afs = fc::raw::unpack<fixed_string<>>( bp );
    }
    test( std::string() );
    test( "helloworld" );
    test( "1234567890123456" );

    auto packed_long_string = fc::raw::pack( std::string( "12345678901234567890" ) );
    auto unpacked = fc::raw::unpack<fixed_string<>>( packed_long_string );
    idump( (unpacked) );
 }
 */

    BOOST_AUTO_TEST_CASE(serialization_raw_test) {
        try {
            ACTORS((alice)(bob))
            transfer_operation<0, 17, 0> op;
            op.from = "alice";
            op.to = "bob";
            op.amount = latest_asset(100, STEEM_SYMBOL_NAME);

            trx.operations.push_back(op);
            auto packed = fc::raw::pack(trx);
            signed_transaction unpacked = fc::raw::unpack<signed_transaction>(packed);
            unpacked.validate();
            BOOST_CHECK(trx.digest() == unpacked.digest());
        } catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(serialization_json_test) {
        try {
            ACTORS((alice)(bob))
            transfer_operation<0, 17, 0> op;
            op.from = "alice";
            op.to = "bob";
            op.amount = latest_asset(100, STEEM_SYMBOL_NAME);

            fc::variant test(op.amount);
            auto tmp = test.as<latest_asset>();
            BOOST_REQUIRE(tmp == op.amount);

            trx.operations.push_back(op);
            fc::variant packed(trx);
            signed_transaction unpacked = packed.as<signed_transaction>();
            unpacked.validate();
            BOOST_CHECK(trx.digest() == unpacked.digest());
        } catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(asset_test) {
        try {
            BOOST_CHECK_EQUAL(typename BOOST_IDENTITY_TYPE ((latest_asset ))().get_decimals(), 3);
            BOOST_CHECK_EQUAL(typename BOOST_IDENTITY_TYPE ((latest_asset ))().symbol.operator std::string(), "TESTS");
            BOOST_CHECK_EQUAL(typename BOOST_IDENTITY_TYPE ((latest_asset ))().to_string(), "0.000 TESTS");

            BOOST_TEST_MESSAGE("Asset Test");
            latest_asset steem = latest_asset::from_string("123.456 TESTS");
            latest_asset sbd = latest_asset::from_string("654.321 TBD");
            latest_asset tmp = latest_asset::from_string("0.456 TESTS");
            BOOST_CHECK_EQUAL(tmp.amount.value, 456);
            tmp = latest_asset::from_string("0.056 TESTS");
            BOOST_CHECK_EQUAL(tmp.amount.value, 56);

            BOOST_CHECK(std::abs(steem.to_real() - 123.456) < 0.0005);
            BOOST_CHECK_EQUAL(steem.amount.value, 123456);
            BOOST_CHECK_EQUAL(steem.get_decimals(), 3);
            BOOST_CHECK_EQUAL(steem.symbol.operator std::string(), "TESTS");
            BOOST_CHECK_EQUAL(steem.to_string(), "123.456 TESTS");
            BOOST_CHECK_EQUAL(steem.symbol.operator std::string(), STEEM_SYMBOL_NAME);
            BOOST_CHECK_EQUAL(latest_asset(50, STEEM_SYMBOL_NAME).to_string(), "0.050 TESTS");
            BOOST_CHECK_EQUAL(latest_asset(50000, STEEM_SYMBOL_NAME).to_string(), "50.000 TESTS");

            BOOST_CHECK(std::abs(sbd.to_real() - 654.321) < 0.0005);
            BOOST_CHECK_EQUAL(sbd.amount.value, 654321);
            BOOST_CHECK_EQUAL(sbd.get_decimals(), 3);
            BOOST_CHECK_EQUAL(sbd.symbol.operator std::string(), "TBD");
            BOOST_CHECK_EQUAL(sbd.to_string(), "654.321 TBD");
            BOOST_CHECK_EQUAL(sbd.symbol.operator std::string(), SBD_SYMBOL_NAME);
            BOOST_CHECK_EQUAL(latest_asset(50, SBD_SYMBOL_NAME).to_string(), "0.050 TBD");
            BOOST_CHECK_EQUAL(latest_asset(50000, SBD_SYMBOL_NAME).to_string(), "50.000 TBD");

            BOOST_CHECK_THROW(steem.set_decimals(100), fc::exception);
            char *steem_sy = (char *) &steem.symbol;
            steem_sy[0] = 100;
            BOOST_CHECK_THROW(steem.get_decimals(), fc::exception);
            steem_sy[6] = 'A';
            steem_sy[7] = 'A';

            auto check_sym = [](const latest_asset &a) -> std::string {
                auto symbol = a.symbol;
                wlog("symbol_name is ${s}", ("s", symbol));
                return symbol;
            };

            BOOST_CHECK_THROW(check_sym(steem), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1.00000000000000000000 TESTS"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1.000TESTS"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1. 333 TESTS"),
                              fc::exception); // Fails because symbol is '333 TESTS', which is too long
            BOOST_CHECK_THROW(latest_asset::from_string("1 .333 TESTS"), fc::exception);
            latest_asset unusual = latest_asset::from_string(
                    "1. 333 X"); // Passes because symbol '333 X' is short enough
            FC_ASSERT(unusual.get_decimals() == 0);
            FC_ASSERT(unusual.symbol == "333 X");
            BOOST_CHECK_THROW(latest_asset::from_string("1 .333 X"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1 .333"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1 1.1"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("11111111111111111111111111111111111111111111111 TESTS"),
                              fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1.1.1 TESTS"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1.abc TESTS"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string(" TESTS"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("TESTS"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1.333"), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("1.333 "), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string(""), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string(" "), fc::exception);
            BOOST_CHECK_THROW(latest_asset::from_string("  "), fc::exception);

            BOOST_CHECK_EQUAL(latest_asset::from_string("100 TESTS").amount.value, 100);
        } FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(json_tests) {
        try {
            auto var = fc::json::variants_from_string("10.6 ");
            var = fc::json::variants_from_string("10.5");
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(extended_private_key_type_test) {
        try {
            fc::ecc::extended_private_key key = fc::ecc::extended_private_key(fc::ecc::private_key::generate(),
                                                                              fc::sha256(), 0, 0, 0);
            extended_private_key_type type = extended_private_key_type(key);
            std::string packed = std::string(type);
            extended_private_key_type unpacked = extended_private_key_type(packed);
            BOOST_CHECK(type == unpacked);
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(extended_public_key_type_test) {
        try {
            fc::ecc::extended_public_key key = fc::ecc::extended_public_key(
                    fc::ecc::private_key::generate().get_public_key(), fc::sha256(), 0, 0, 0);
            extended_public_key_type type = extended_public_key_type(key);
            std::string packed = std::string(type);
            extended_public_key_type unpacked = extended_public_key_type(packed);
            BOOST_CHECK(type == unpacked);
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(version_test) {
        try {
            BOOST_REQUIRE_EQUAL(string(version(1, 2, 3)), "1.2.3");

            fc::variant ver_str("3.0.0");
            version ver;
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == version(3, 0, 0));

            ver_str = fc::variant("0.0.0");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == version());

            ver_str = fc::variant("1.0.1");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == version(1, 0, 1));

            ver_str = fc::variant("1_0_1");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == version(1, 0, 1));

            ver_str = fc::variant("12.34.56");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == version(12, 34, 56));

            ver_str = fc::variant("256.0.0");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("0.256.0");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("0.0.65536");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("1.0");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("1.0.0.1");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);
        } FC_LOG_AND_RETHROW();
    }

    BOOST_AUTO_TEST_CASE(hardfork_version_test) {
        try {
            BOOST_REQUIRE_EQUAL(string(hardfork_version(1, 2)), "1.2.0");

            fc::variant ver_str("3.0.0");
            hardfork_version ver;
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == hardfork_version(3, 0));

            ver_str = fc::variant("0.0.0");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == hardfork_version());

            ver_str = fc::variant("1.0.0");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == hardfork_version(1, 0));

            ver_str = fc::variant("1_0_0");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == hardfork_version(1, 0));

            ver_str = fc::variant("12.34.00");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == hardfork_version(12, 34));

            ver_str = fc::variant("256.0.0");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("0.256.0");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("0.0.1");
            fc::from_variant(ver_str, ver);
            BOOST_REQUIRE(ver == hardfork_version(0, 0));

            ver_str = fc::variant("1.0");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("1.0.0.1");
            STEEMIT_REQUIRE_THROW(fc::from_variant(ver_str, ver), fc::exception);
        } FC_LOG_AND_RETHROW();
    }

BOOST_AUTO_TEST_SUITE_END()
#endif