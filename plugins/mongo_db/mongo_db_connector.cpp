#include <golos/plugins/mongo_db/mongo_db_connector.hpp>

#include <fc/log/logger.hpp>

#include <mongocxx/exception/exception.hpp>


namespace golos {
namespace plugins {
namespace mongo_db {

    mongo_db_connector::mongo_db_connector() {
    }

    mongo_db_connector::~mongo_db_connector() {
    }

    bool mongo_db_connector::initialize(const std::string& uri_str) {
        try {
            uri = mongocxx::uri {uri_str};
            mongo_conn = mongocxx::client {uri};
            db_name = uri.database().empty() ? "Golos" : uri.database();
            mongo_database = mongo_conn[db_name];

            ilog("MongoDB connector initialized.");

            return true;
        }
        catch (mongocxx::exception & ex) {
            ilog("Exception in MongoDB initialize: ${p}", ("p", ex.what()));
            return false;
        }
        catch (...) {
            ilog("Unknown exception in MongoDB connector");
            return false;
        }
    }
}}}
