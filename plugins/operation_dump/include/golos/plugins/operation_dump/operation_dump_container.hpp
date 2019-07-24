#pragma once

#include <sstream>
#include <boost/filesystem/fstream.hpp>
#include <fc/io/raw.hpp>

namespace golos { namespace plugins { namespace operation_dump {

namespace bfs = boost::filesystem;

// Structure size can differ - uses sizeof
struct dump_header {
    char magic[13] = "Golos\adumpOP";
    uint32_t version = 1;
};

using operation_number = std::pair<uint32_t, uint16_t>;

class dump_buffer : public std::stringstream {
public:
    dump_buffer() {
    }

    using std::stringstream::write;

    void write(const operation_number& op_num) {
        fc::raw::pack(*this, op_num);
    }
};

using dump_buffers = std::map<std::string, dump_buffer>;

class dump_file : public bfs::ofstream {
public:
    dump_file(const bfs::path& p): bfs::ofstream(p, std::ios_base::binary | std::ios_base::app) {
        bfs::ofstream::exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }

    void write(const dump_header& hdr) {
        bfs::ofstream::write((const char*)&hdr, sizeof(dump_header));
    }
};

} } } // golos::plugins::operation_dump
