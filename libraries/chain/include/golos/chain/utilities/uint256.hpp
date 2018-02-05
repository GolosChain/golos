#pragma once

#include <golos/protocol/types.hpp>

#include <fc/uint128.hpp>

namespace golos {
    namespace chain {
        namespace utilities {
            inline boost::multiprecision::uint256_t to256(const fc::uint128 &t) {
                boost::multiprecision::uint256_t v(t.hi);
                v <<= 64;
                v += t.lo;
                return v;
            }
        }
    }
}
