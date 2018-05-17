#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>

namespace golos {
namespace plugins {
namespace mongo_db {

    using bulk_ptr = std::unique_ptr<mongocxx::bulk_write>;

    class mongo_db_connector {
    public:
        mongo_db_connector();
        mongo_db_connector(const mongo_db_connector&) = delete;
        mongo_db_connector(mongo_db_connector&&) = default;
        ~mongo_db_connector();
        mongo_db_connector& operator=(const mongo_db_connector&) = delete;
        mongo_db_connector& operator=(mongo_db_connector&&) = default;

        virtual bool initialize(const std::string& uri_str);

        bsoncxx::types::b_oid get_comment_oid(const std::string &auth, const std::string &permlink);

    protected:
        std::string db_name;

        //MongoDB connection members
        mongocxx::options::bulk_write bulk_opts;
        mongocxx::client mongo_conn;
        mongocxx::database mongo_database;
        mongocxx::uri uri;
    };
}}}
