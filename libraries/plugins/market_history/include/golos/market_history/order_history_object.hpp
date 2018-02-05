#ifndef GOLOS_ORDER_HISTORY_OBJECT_HPP
#define GOLOS_ORDER_HISTORY_OBJECT_HPP

#include <golos/application/plugin.hpp>

#include <golos/chain/steem_object_types.hpp>

#include <golos/market_history/market_history_object_types.hpp>
#include <golos/market_history/key_interface.hpp>

#include <golos/protocol/asset.hpp>

namespace golos {
    namespace market_history {
        class history_key : public key_interface {
        public:
            history_key();

            history_key(protocol::asset_name_type base, protocol::asset_name_type quote, int64_t sequence)
                    : key_interface(base, quote), sequence(sequence) {
            }

            int64_t sequence = 0;

            friend bool operator<(const history_key &a, const history_key &b) {
                return std::tie(a.base, a.quote, a.sequence) < std::tie(b.base, b.quote, b.sequence);
            }

            friend bool operator==(const history_key &a, const history_key &b) {
                return std::tie(a.base, a.quote, a.sequence) == std::tie(b.base, b.quote, b.sequence);
            }
        };

        struct order_history_object : public chainbase::object<order_history_object_type, order_history_object> {
            order_history_object() {

            }

            template<typename Constructor, typename Allocator>
            order_history_object(Constructor &&c, chainbase::allocator<Allocator> a) {
                c(*this);
            }

            id_type id;

            history_key key;
            fc::time_point_sec time;
            protocol::market_virtual_operations op;
        };

        struct by_time;
        struct by_key;

        typedef multi_index_container<order_history_object, indexed_by<ordered_unique<tag<chain::by_id>,
                member<order_history_object, order_history_object::id_type, &order_history_object::id>>,
                ordered_unique<tag<by_key>, member<order_history_object, history_key, &order_history_object::key>>>,
                chainbase::allocator<order_history_object> > order_history_index;
    }
}

FC_REFLECT((golos::market_history::order_history_object), (id)(key)(time)(op));
CHAINBASE_SET_INDEX_TYPE(golos::market_history::order_history_object, golos::market_history::order_history_index);

FC_REFLECT_DERIVED((golos::market_history::history_key), ((golos::market_history::key_interface)), (sequence));

#endif //GOLOS_ORDER_HISTORY_OBJECT_HPP
