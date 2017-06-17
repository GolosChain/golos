#include <cds/init.h>
// Libcds implementation of FCQueue
#include <cds/container/fcqueue.h>
#include <cds/threading/model.h>
 
#include <boost/asio.hpp>
#include <boost/array.hpp>
 
#include <string>
#include <cstdio>
#include <iostream>
#include <vector>
#include <random>
#include <cstdlib>
#include <algorithm>
#include <queue>
#include <thread>
#include <csignal>
#include <atomic>

#pragma once
class statServer {

private:
    std::atomic_bool QUEUE_ENABLED;
    std::thread _sender_thr;
    cds::container::FCQueue<std::string, std::queue<std::string>, cds::container::fcqueue::traits> stat_q;
    std::vector<std::string> _recipient_ip_vec;
    int _port = 8125;
    int _sender_sleeping_time = 3;

    void _init();

public:
    void add_address(const std::string & address);
    bool can_push();
    statServer();
    ~statServer();
    void stop();
    void push(const std::string & item);
    void start();
};