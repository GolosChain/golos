#ifndef GOLOS_MARKET_HISTORY_OBJECT_TYPES_HPP
#define GOLOS_MARKET_HISTORY_OBJECT_TYPES_HPP

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef MARKET_HISTORY_SPACE_ID
#define MARKET_HISTORY_SPACE_ID 7
#endif

namespace golos {
    namespace market_history {
        using namespace boost::multi_index;

        using boost::multi_index_container;

        using chainbase::object;
        using chainbase::object_id;
        using chainbase::allocator;

        enum market_history_object_types {
            bucket_object_type = (MARKET_HISTORY_SPACE_ID << 8),
            order_history_object_type = (MARKET_HISTORY_SPACE_ID << 8) + 1,
        };
    }
}

#endif //GOLOS_MARKET_HISTORY_OBJECT_TYPES_HPP