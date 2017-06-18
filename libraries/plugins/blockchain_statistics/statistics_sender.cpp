#include "include/statistics_sender.hpp"

#include <boost/asio.hpp> 

#include <string>
#include <cstdio>
#include <iostream>
#include <vector>
#include <random>
#include <cstdlib>
#include <algorithm>
#include <queue>

statClient::statClient() {
    QUEUE_ENABLED = false;
}
statClient::~statClient() {
    QUEUE_ENABLED = false;
    sender_thread.join();
    cds::Terminate();
}
void statClient::add_address(const std::string & address) {
    recipient_ip_vec.push_back(address);
}
void statClient::_init(int br_port, int timeout) {    
    QUEUE_ENABLED = true;
    port = br_port;
    sender_sleeping_time = timeout;
    cds::Initialize();
}

void statClient::stop() {
    QUEUE_ENABLED = false;
}

void statClient::push(const std::string & item) {
    stat_q.enqueue(item);
}

void statClient::start(int br_port, int timeout) {
    std::cout << "Client is on!!" << std::endl;
    _init(br_port, timeout);
    // Lambda which implements the data sending loop
    auto run_broadcast_loop = [&]() {
        boost::asio::io_service io_service;
        // Client binds to any address and any port.
        boost::asio::ip::udp::socket socket(io_service,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
        socket.set_option(boost::asio::socket_base::broadcast(true));
        // Broadcast will go to port 8125.
        // (Etsy/StatD using 8125 as default for udp
        std::vector<boost::asio::ip::udp::endpoint> broadcast_endpoint_vec;
        for (auto address : recipient_ip_vec) {
            broadcast_endpoint_vec.push_back(
                boost::asio::ip::udp::endpoint(
                    boost::asio::ip::address::from_string(address),
                    port
                )
            );
        }            

        cds::threading::Manager::attachThread();

        while (QUEUE_ENABLED.load() || !stat_q.empty()) {
            if (!stat_q.empty()) {
               
                std::string tmp_s;
                stat_q.dequeue(tmp_s);
                for (auto recipient : broadcast_endpoint_vec) {
                    socket.send_to(boost::asio::buffer(tmp_s), recipient);
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(sender_sleeping_time));
        }
                
        cds::threading::Manager::detachThread();
    };
    try
    {   
        if (recipient_ip_vec.empty()) {
            throw "No any recipient IP was set.";
        }
        std::thread sending_thr(run_broadcast_loop);
        sender_thread = std::move(sending_thr);
    }
    catch (const std::exception &ex)
    {
         std::cerr << ex.what() << std::endl;
    }
}
