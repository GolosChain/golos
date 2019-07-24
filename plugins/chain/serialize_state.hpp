#pragma once

// this file must only be included from serialize_state.cpp
// required to declare some packers before <fc/io/raw.hpp> so they become visible to compiler

namespace fc {
    class uint128_t;
    template<typename Storage> class fixed_string;
}
namespace golos { namespace chain {
    using account_name_type = fc::fixed_string<fc::uint128_t>;
    struct shared_authority;
    class comment_object;
    class savings_withdraw_object;
}}
namespace golos { namespace protocol {
    struct beneficiary_route_type;
}}


namespace fc { namespace raw {

template<typename S> void pack(S&, const golos::chain::comment_object&);
template<typename S> void pack(S&, const golos::chain::savings_withdraw_object&);
template<typename S> void pack(S&, const golos::chain::account_name_type&);
template<typename S> void pack(S&, const golos::chain::shared_authority&);
template<typename S> void pack(S&, const golos::protocol::beneficiary_route_type&);

}} // fc::raw
