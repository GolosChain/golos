#pragma once

class statServer {

private:
    std::atomic<bool> QUEUE_ENABLED;   
    std::thread _sender_thr;
    cds::container::FCQueue<string, std::queue<string>, cds::container::fcqueue::traits> stat_q;    
    std::vector<std::string> _recipient_ip_vec;
    int _port = 8125;
    int _sender_sleeping_time = 3;

    void _init();

public:    
    void add_address(const string & address);
    bool can_push();
    statServer();
    ~statServer();
    void stop();
    void push(const std::string & item);
    void start();
};