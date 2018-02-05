#ifndef GOLOS_BUCKET_OBJECT_HPP
#define GOLOS_BUCKET_OBJECT_HPP

#include <golos/application/plugin.hpp>

#include <golos/chain/steem_object_types.hpp>

#include <golos/market_history/market_history_object_types.hpp>
#include <golos/market_history/key_interface.hpp>

#include <golos/protocol/asset.hpp>

namespace golos {
    namespace market_history {
        class bucket_key : public key_interface {
        public:
            bucket_key(protocol::asset_name_type a, protocol::asset_name_type b, uint32_t s, fc::time_point_sec o)
                    : key_interface(a, b), seconds(s), open(o) {
            }

            bucket_key();

            uint32_t seconds = 0;
            fc::time_point_sec open;

            friend bool operator<(const bucket_key &a, const bucket_key &b) {
                return std::tie(a.base, a.quote, a.seconds, a.open) < std::tie(b.base, b.quote, b.seconds, b.open);
            }

            friend bool operator==(const bucket_key &a, const bucket_key &b) {
                return std::tie(a.base, a.quote, b.seconds, a.open) == std::tie(b.base, b.quote, b.seconds, b.open);
            }
        };

        struct by_id;
        struct by_key;

        struct bucket_object : public object<bucket_object_type, bucket_object> {
            bucket_object() {

            }

            template<typename Constructor, typename Allocator>
            bucket_object(Constructor &&c, chainbase::allocator<Allocator> a) {
                c(*this);
            }

            id_type id;

            protocol::price<0, 17, 0> high() const;

            protocol::price<0, 17, 0> low() const;

            bucket_key key;
            protocol::share_type high_base;
            protocol::share_type high_quote;
            protocol::share_type low_base;
            protocol::share_type low_quote;
            protocol::share_type open_base;
            protocol::share_type open_quote;
            protocol::share_type close_base;
            protocol::share_type close_quote;
            protocol::share_type base_volume;
            protocol::share_type quote_volume;
        };

        typedef multi_index_container<bucket_object, indexed_by<
                ordered_unique<tag<by_id>, member<bucket_object, bucket_object::id_type, &bucket_object::id>>,
                ordered_unique<tag<by_key>, member<bucket_object, bucket_key, &bucket_object::key>>>,
                chainbase::allocator<bucket_object> > bucket_index;
    }
}

FC_REFLECT_DERIVED((golos::market_history::bucket_key), ((golos::market_history::key_interface)), (seconds)(open));

FC_REFLECT((golos::market_history::bucket_object),
           (id)(key)(high_base)(high_quote)(low_base)(low_quote)(open_base)(open_quote)(close_base)(close_quote)(
                   base_volume)(quote_volume))
CHAINBASE_SET_INDEX_TYPE(golos::market_history::bucket_object, golos::market_history::bucket_index);

#endif //GOLOS_BUCKET_OBJECT_HPP