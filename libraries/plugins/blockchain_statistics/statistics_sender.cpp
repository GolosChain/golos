#include "statistics_sender.hpp"

statServer::statServer() {
    _init();
}
statServer::~statServer() {
    stop();
}
void statServer::add_address(const std::string & address) {
    _recipient_ip_vec.push_back(address);

}
void statServer::_init() {
    QUEUE_ENABLED = true;
    cds::Initialize();
}
bool statServer::can_push() {
    return QUEUE_ENABLED.load();
}

void statServer::stop() {
    QUEUE_ENABLED = false;
    _sender_thr.join();
    cds::Terminate();
}

void statServer::push(const std::string & item) {
    stat_q.enqueue(item);
}

void statServer::start() {
    _init();
    auto run_broadcast_loop = [&]() {
        boost::asio::io_service io_service;
        // Server binds to any address and any port.
        boost::asio::ip::udp::socket socket(io_service,
                              boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
        socket.set_option(boost::asio::socket_base::broadcast(true));
        // Broadcast will go to port 8125.
        // (Etsy/StatD using 8125 as default for udp
        std::vector<boost::asio::ip::udp::endpoint> broadcast_endpoint_vec;
        for (auto address : _recipient_ip_vec) {
            broadcast_endpoint_vec.push_back(
                boost::asio::ip::udp::endpoint(
                    boost::asio::ip::address::from_string(address),
                    _port
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

                std::this_thread::sleep_for(std::chrono::seconds(_sender_sleeping_time));
            }
        }
        cds::threading::Manager::detachThread();
    };
    try
    {   
        if (_recipient_ip_vec.empty()) {
            throw "No one recipient IP was set.";
        }
        std::thread sending_thr(run_broadcast_loop);
        _sender_thr = std::move(sending_thr);
    }
    catch (const std::exception &ex)
    {
         std::cerr << ex.what() << std::endl;
    }
}
