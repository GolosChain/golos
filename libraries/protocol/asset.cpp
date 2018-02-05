#include <golos/protocol/asset.hpp>

/*

The bounds on asset serialization are as follows:

index : field
0     : decimals
1..6  : symbol
   7  : \0
*/

namespace golos {
    namespace protocol {
        typedef boost::multiprecision::int128_t int128_t;

        bool operator==(const asset_symbol_type &a, const asset_name_type &b) {
            auto ta = (const char *) &a;
            FC_ASSERT(ta[7] == 0);
            return b == &ta[1];
        }

        bool operator==(const asset_name_type &b, const asset_symbol_type &a) {
            auto ta = (const char *) &a;
            FC_ASSERT(ta[7] == 0);
            return b == &ta[1];
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::asset() : asset_interface<Major,
                Hardfork, Release, asset_symbol_type, share_type>(0, STEEM_SYMBOL) {

        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::asset(share_type a,
                                                                                          asset_symbol_type id)
                : asset_interface<Major, Hardfork, Release, asset_symbol_type, share_type>(a, id) {

        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::asset(share_type a,
                                                                                          asset_name_type name)
                : asset_interface<Major, Hardfork, Release, asset_symbol_type, share_type>(a, STEEM_SYMBOL) {
            string s = fc::trim(name);

            this->symbol = uint64_t(3);
            char *sy = (char *) &this->symbol;

            size_t symbol_size = name.size();

            if (symbol_size > 0) {
                FC_ASSERT(symbol_size <= 6);

                std::string symbol_string(name);

                FC_ASSERT(std::find_if(symbol_string.begin(), symbol_string.end(), [&](const char &c) -> bool {
                    return std::isdigit(c);
                }) == symbol_string.end());
                memcpy(sy + 1, symbol_string.c_str(), symbol_size);
            }
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        double asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::to_real() const {
            return double(this->amount.value) / precision();
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        uint8_t asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::get_decimals() const {
            auto a = (const char *) &this->symbol;
            uint8_t result = uint8_t(a[0]);
            FC_ASSERT(result < 15);
            return result;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::set_decimals(uint8_t d) {
            FC_ASSERT(d < 15);
            auto a = (char *) &this->symbol;
            a[0] = d;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset_name_type asset<Major, Hardfork, Release,
                type_traits::static_range<Hardfork <= 16>>::symbol_name() const {
            auto a = (const char *) &this->symbol;
            FC_ASSERT(a[7] == 0);
            return &a[1];
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset_symbol_type asset<Major, Hardfork, Release,
                type_traits::static_range<Hardfork <= 16>>::symbol_type_value() const {
            return this->symbol;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        int64_t asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::precision() const {
            static int64_t table[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000ll, 1000000000ll,
                                      10000000000ll, 100000000000ll, 1000000000000ll, 10000000000000ll,
                                      100000000000000ll};
            uint8_t d = get_decimals();
            return table[d];
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        string asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>>::to_string() const {
            int64_t prec = precision();
            string result = fc::to_string(this->amount.value / prec);
            if (prec > 1) {
                auto fract = this->amount.value % prec;
                // prec is a power of ten, so for example when working with
                // 7.005 we have fract = 5, prec = 1000.  So prec+fract=1005
                // has the correct number of zeros and we can simply trim the
                // leading 1.
                result += "." + fc::to_string(prec + fract).erase(0, 1);
            }
            return result + " " + symbol_name();
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release> asset<Major, Hardfork, Release,
                type_traits::static_range<Hardfork <= 16>>::from_string(const string &from) {
            try {
                string s = fc::trim(from);
                auto space_pos = s.find(' ');
                auto dot_pos = s.find('.');

                asset<Major, Hardfork, Release> result;
                result.symbol = uint64_t(3);
                auto sy = (char *) &result.symbol;

                if (space_pos == std::string::npos && dot_pos == std::string::npos &&
                    std::find_if(from.begin(), from.end(), [&](const std::string::value_type &c) -> bool {
                        return std::isdigit(c);
                    }) == from.end()) {
                    result.amount = 0;
                } else if (dot_pos != std::string::npos) {
                    FC_ASSERT(space_pos > dot_pos);

                    auto intpart = s.substr(0, dot_pos);
                    auto fractpart = "1" + s.substr(dot_pos + 1, space_pos - dot_pos - 1);
                    result.set_decimals(fractpart.size() - 1);

                    result.amount = fc::to_int64(intpart);
                    result.amount.value *= result.precision();
                    result.amount.value += fc::to_int64(fractpart);
                    result.amount.value -= result.precision();
                } else {
                    auto intpart = s.substr(0, space_pos);
                    result.amount = fc::to_int64(intpart);
                    result.set_decimals(0);
                }
                auto symbol = s.substr(space_pos + 1);
                size_t symbol_size = symbol.size();

                if (symbol_size > 0) {
                    FC_ASSERT(symbol_size <= 6);
                    memcpy(sy + 1, symbol.c_str(), symbol_size);
                }

                return result;
            } FC_CAPTURE_AND_RETHROW((from))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::asset() : asset_interface<Major,
                Hardfork, Release, asset_name_type, share_type>(0, STEEM_SYMBOL_NAME), decimals(3) {

        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::asset(share_type a,
                                                                                          asset_symbol_type name)
                : asset_interface<Major, Hardfork, Release, asset_name_type, share_type>(a, STEEM_SYMBOL_NAME),
                decimals(3) {
            auto ta = (const char *) &name;
            FC_ASSERT(ta[7] == 0);
            this->symbol = &ta[1];

            uint8_t result = uint8_t(ta[0]);
            FC_ASSERT(result < 15);
            this->decimals = result;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::asset(share_type a,
                                                                                          asset_name_type name,
                                                                                          uint8_t d)
                : asset_interface<Major, Hardfork, Release, asset_name_type, share_type>(a, name), decimals(d) {

        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        uint8_t asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::get_decimals() const {
            return this->decimals;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::set_decimals(uint8_t d) {
            this->decimals = d;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        double asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::to_real() const {
            return double(this->amount.value) / precision();
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset_symbol_type asset<Major, Hardfork, Release,
                type_traits::static_range<Hardfork >= 17>>::symbol_type_value() const {
            FC_ASSERT(this->decimals < 15, "Precision should be less than 15");

            asset_symbol_type result = uint64_t(this->decimals);
            char *sy = (char *) &result;
            std::string symbol_string = fc::trim(this->symbol);
            std::size_t symbol_size = symbol_string.size();

            if (symbol_size > 0) {
                FC_ASSERT(this->symbol.size() <= 6,
                          "Asset symbol type can only present symbols with length less or equal than 6");

                FC_ASSERT(std::find_if(symbol_string.begin(), symbol_string.end(), [&](const char &c) -> bool {
                    return std::isdigit(c);
                }) == symbol_string.end());

                memcpy(sy + 1, symbol_string.c_str(), symbol_size);
            }

            return result;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset_name_type asset<Major, Hardfork, Release,
                type_traits::static_range<Hardfork >= 17>>::symbol_name() const {
            return this->symbol;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        int64_t asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::precision() const {
            static int64_t table[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000ll, 1000000000ll,
                                      10000000000ll, 100000000000ll, 1000000000000ll, 10000000000000ll,
                                      100000000000000ll};
            return table[this->decimals];
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        string asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>>::to_string() const {
            int64_t prec = precision();
            string result = fc::to_string(this->amount.value / prec);
            if (prec > 1) {
                auto fract = this->amount.value % prec;
                // prec is a power of ten, so for example when working with
                // 7.005 we have fract = 5, prec = 1000.  So prec+fract=1005
                // has the correct number of zeros and we can simply trim the
                // leading 1.
                result += "." + fc::to_string(prec + fract).erase(0, 1);
            }
            return result + " " + this->symbol;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release> asset<Major, Hardfork, Release,
                type_traits::static_range<Hardfork >= 17>>::from_string(const string &from) {
            try {
                string s = fc::trim(from);
                auto space_pos = s.find(' ');
                auto dot_pos = s.find('.');

                asset<Major, Hardfork, Release> result;

                if (space_pos == std::string::npos && dot_pos == std::string::npos &&
                    std::find_if(from.begin(), from.end(), [&](const std::string::value_type &c) -> bool {
                        return std::isdigit(c);
                    }) == from.end()) {
                    result.amount = 0;
                } else if (dot_pos != std::string::npos) {
                    FC_ASSERT(space_pos > dot_pos);

                    auto intpart = s.substr(0, dot_pos);
                    auto fractpart = "1" + s.substr(dot_pos + 1, space_pos - dot_pos - 1);
                    result.decimals = static_cast<uint8_t>(fractpart.size() - 1);

                    result.amount = fc::to_int64(intpart);
                    result.amount.value *= result.precision();
                    result.amount.value += fc::to_int64(fractpart);
                    result.amount.value -= result.precision();
                } else {
                    auto intpart = s.substr(0, space_pos);
                    result.amount = fc::to_int64(intpart);
                    result.decimals = 0;
                }

                auto symbol = s.substr(space_pos + 1);

                if (symbol.size() > 0) {
                    result.symbol = symbol;
                }

                return result;
            } FC_CAPTURE_AND_RETHROW((from))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        price<Major, Hardfork, Release> price<Major, Hardfork, Release>::max(asset_name_type base,
                                                                             asset_name_type quote) {
            return asset<Major, Hardfork, Release>(share_type(STEEMIT_MAX_SHARE_SUPPLY), base) /
                   asset<Major, Hardfork, Release>(share_type(1), quote);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        price<Major, Hardfork, Release> price<Major, Hardfork, Release>::min(asset_name_type base,
                                                                             asset_name_type quote) {
            return asset<Major, Hardfork, Release>(1, base) /
                   asset<Major, Hardfork, Release>(STEEMIT_MAX_SHARE_SUPPLY, quote);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool price<Major, Hardfork, Release>::is_null() const {
            return *this == price<Major, Hardfork, Release>();
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void price<Major, Hardfork, Release>::validate() const {
            try {
                FC_ASSERT(base.amount > share_type(0));
                FC_ASSERT(quote.amount > share_type(0));
                FC_ASSERT(base.symbol_name() != quote.symbol_name());
            } FC_CAPTURE_AND_RETHROW((base)(quote))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        price<Major, Hardfork, Release> price<Major, Hardfork, Release>::call_price(
                const asset<Major, Hardfork, Release> &debt, const asset<Major, Hardfork, Release> &collateral,
                uint16_t collateral_ratio) {
            try {
                //wdump((debt)(collateral)(collateral_ratio));
                boost::rational<int128_t> swan(debt.amount.value, collateral.amount.value);
                boost::rational<int128_t> ratio(collateral_ratio, STEEMIT_COLLATERAL_RATIO_DENOM);
                auto cp = swan * ratio;

                while (cp.numerator() > STEEMIT_MAX_SHARE_SUPPLY || cp.denominator() > STEEMIT_MAX_SHARE_SUPPLY) {
                    cp = boost::rational<int128_t>((cp.numerator() >> 1) + 1, (cp.denominator() >> 1) + 1);
                }

                return ~(asset<Major, Hardfork, Release>(cp.numerator().convert_to<int64_t>(), debt.symbol_name()) /
                         asset<Major, Hardfork, Release>(cp.denominator().convert_to<int64_t>(),
                                                         collateral.symbol_name()));
            } FC_CAPTURE_AND_RETHROW((debt)(collateral)(collateral_ratio))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        void price_feed<Major, Hardfork, Release>::validate() const {
            try {
                if (!settlement_price.is_null()) {
                    settlement_price.validate();
                }
                FC_ASSERT(maximum_short_squeeze_ratio >= STEEMIT_MIN_COLLATERAL_RATIO);
                FC_ASSERT(maximum_short_squeeze_ratio <= STEEMIT_MAX_COLLATERAL_RATIO);
                FC_ASSERT(maintenance_collateral_ratio >= STEEMIT_MIN_COLLATERAL_RATIO);
                FC_ASSERT(maintenance_collateral_ratio <= STEEMIT_MAX_COLLATERAL_RATIO);
                max_short_squeeze_price(); // make sure that it doesn't overflow

                //FC_ASSERT( maintenance_collateral_ratio >= maximum_short_squeeze_ratio );
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool price_feed<Major, Hardfork, Release>::is_for(asset_name_type asset_name) const {
            try {
                if (!settlement_price.is_null()) {
                    return (settlement_price.base.symbol_name() == asset_name);
                }
                if (!core_exchange_rate.is_null()) {
                    return (core_exchange_rate.base.symbol_name() == asset_name);
                }
                // (null, null) is valid for any feed
                return true;
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        price<Major, Hardfork, Release> price_feed<Major, Hardfork, Release>::max_short_squeeze_price() const {
            boost::rational<int128_t> sp(settlement_price.base.amount.value,
                                         settlement_price.quote.amount.value); //debt.amount.value,collateral.amount.value);
            boost::rational<int128_t> ratio(STEEMIT_COLLATERAL_RATIO_DENOM, maximum_short_squeeze_ratio);
            auto cp = sp * ratio;

            while (cp.numerator() > STEEMIT_MAX_SHARE_SUPPLY || cp.denominator() > STEEMIT_MAX_SHARE_SUPPLY) {
                cp = boost::rational<int128_t>((cp.numerator() >> 1) + (cp.numerator() & 1),
                                               (cp.denominator() >> 1) + (cp.denominator() & 1));
            }

            return (asset<Major, Hardfork, Release>(cp.numerator().convert_to<int64_t>(),
                                                    settlement_price.base.symbol_name()) /
                    asset<Major, Hardfork, Release>(cp.denominator().convert_to<int64_t>(),
                                                    settlement_price.quote.symbol_name()));
        }

        // compile-time table of powers of 10 using template metaprogramming

        template<int N>
        struct p10 {
            static const int64_t v = 10 * p10<N - 1>::v;
        };

        template<>
        struct p10<0> {
            static const int64_t v = 1;
        };

        const int64_t scaled_precision_lut[19] = {p10<0>::v, p10<1>::v, p10<2>::v, p10<3>::v, p10<4>::v, p10<5>::v,
                                                  p10<6>::v, p10<7>::v, p10<8>::v, p10<9>::v, p10<10>::v, p10<11>::v,
                                                  p10<12>::v, p10<13>::v, p10<14>::v, p10<15>::v, p10<16>::v,
                                                  p10<17>::v, p10<18>::v};
    }
} // golos::protocol

#include <golos/protocol/asset.tpp>