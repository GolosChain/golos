#include <golos/market_history/bucket_object.hpp>

namespace golos {
    namespace market_history {
        bucket_key::bucket_key() {

        }

        protocol::price<0, 17, 0> bucket_object::high() const {
            return protocol::asset<0, 17, 0>(high_base, key.base) / protocol::asset<0, 17, 0>(high_quote, key.quote);
        }

        protocol::price<0, 17, 0> bucket_object::low() const {
            return protocol::asset<0, 17, 0>(low_base, key.base) / protocol::asset<0, 17, 0>(low_quote, key.quote);
        }
    }
}
