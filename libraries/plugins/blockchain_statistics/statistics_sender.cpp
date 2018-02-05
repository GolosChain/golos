#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/system/error_code.hpp>

#include <fc/exception/exception.hpp>
#include <boost/exception/all.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>

#include <string>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <algorithm>

#include "include/statistics_sender.hpp"

stat_client::stat_client(uint32_t default_port) : default_port(default_port) {
}

bool stat_client::can_start() {
    return !recipient_endpoint_set.empty();
}

void stat_client::send(const std::string & str) {
    boost::asio::io_service io_service;

    boost::asio::ip::udp::socket socket(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
    socket.set_option(boost::asio::socket_base::broadcast(true));

    for (auto endpoint : recipient_endpoint_set) {
        socket.send_to(boost::asio::buffer(str), endpoint);
    }
}

void stat_client::add_address(const std::string & address) {
    // Parsing "IP:PORT". If there is no port, then use Default one from configs.
    try
    {
        boost::asio::ip::udp::endpoint ep;
        boost::asio::ip::address ip;
        uint16_t port;        
        boost::system::error_code ec;

        auto pos = address.find(':');

        if (pos != std::string::npos) {
            ip = boost::asio::ip::address::from_string( address.substr( 0, pos ) , ec);
            port = boost::lexical_cast<uint16_t>( address.substr( pos + 1, address.size() ) );
        }
        else {
            ip = boost::asio::ip::address::from_string( address , ec);
            port = default_port;
        }
        
        if (ip.is_unspecified()) {
            // TODO something with exceptions and logs!
            ep = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
            recipient_endpoint_set.insert(ep);
        }
        else {
            ep = boost::asio::ip::udp::endpoint(ip, port);
            recipient_endpoint_set.insert(ep);
        }
    }
    FC_CAPTURE_AND_LOG(())
}

std::vector<std::string> stat_client::get_endpoint_string_vector() {
    std::vector<std::string> ep_vec;
    for (auto x : recipient_endpoint_set) {
        ep_vec.push_back( x.address().to_string() + ":" + std::to_string(x.port()));
    }
    return ep_vec;
}
