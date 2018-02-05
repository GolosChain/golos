#pragma once

#include <fc/exception/exception.hpp>

namespace golos {
    namespace network {
        namespace exceptions {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"

            template<uint32_t IncrementalCode = 0, typename What = boost::mpl::string<'P2P Networking Exception'>>
            using basic = fc::basic_exception<90000 + IncrementalCode, What>;

            template<uint32_t IncrementalCode = 1, typename What = boost::mpl::string<'send queue for this peer exceeded maximum size'>>
            using send_queue_overflow = basic<IncrementalCode, What>;

            template<uint32_t IncrementalCode = 2, typename What = boost::mpl::string<'insufficient relay fee'>>
            using insufficient_relay_fee = basic<IncrementalCode, What>;

            template<uint32_t IncrementalCode = 3, typename What = boost::mpl::string<'already connected to requested peer'>>
            using already_connected_to_requested_peer = basic<IncrementalCode, What>;

            template<uint32_t IncrementalCode = 4, typename What = boost::mpl::string<'block is older than our undo history allows us to process'>>
            using block_older_than_undo_history = basic<IncrementalCode, What>;

            template<uint32_t IncrementalCode = 5, typename What = boost::mpl::string<'peer is on another fork'>>
            using peer_is_on_an_unreachable_fork = basic<IncrementalCode, What>;

            template<uint32_t IncrementalCode = 6, typename What = boost::mpl::string<'unlinkable block'>>
            using unlinkable_block = basic<IncrementalCode, What>;
#pragma GCC diagnostic pop
        }
    }
}
