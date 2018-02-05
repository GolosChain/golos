#pragma once

#include <thread>
#include <vector>
#include <atomic>

#include <golos/chain/steem_object_types.hpp>

#include <fc/api.hpp>
#include <fc/uint128.hpp>
#include <boost/asio/ip/udp.hpp>

using namespace golos::chain;

class stat_client final {
public:
    stat_client() = default;
    stat_client(uint32_t default_port);

    ~stat_client() = default;

    bool can_start();
    // sends a string to all endpoints
    void send(const std::string & str);
    
    // adds address to _recipient_ip_vec.
    void add_address(const std::string & address);

    /// returns statistics recievers endpoints
    std::vector<std::string> get_endpoint_string_vector();

private:
    // Stat sender will send data to all endpoints from recipient_endpoint_set
    std::set<boost::asio::ip::udp::endpoint> recipient_endpoint_set;
    // DefaultPort for asio broadcasting 
    uint32_t default_port;
    void init();
};
