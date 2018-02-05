#pragma once

#include <golos/protocol/asset.hpp>

namespace golos {
    namespace chain {
        namespace utilities {

            inline protocol::asset<0, 17, 0> to_sbd(const protocol::price<0, 17, 0> &p, const protocol::asset<0, 17, 0> &steem) {
                FC_ASSERT(steem.symbol == STEEM_SYMBOL_NAME);
                if (p.is_null()) {
                    return protocol::asset<0, 17, 0>(0, SBD_SYMBOL_NAME);
                }
                return steem * p;
            }

            inline protocol::asset<0, 17, 0> to_steem(const protocol::price<0, 17, 0> &p, const protocol::asset<0, 17, 0> &sbd) {
                FC_ASSERT(sbd.symbol == SBD_SYMBOL_NAME);
                if (p.is_null()) {
                    return protocol::asset<0, 17, 0>(0, STEEM_SYMBOL_NAME);
                }
                return sbd * p;
            }
        }
    }
}