#include <golos/plugins/mongo_db/mongo_db_connector.hpp>

#include <fc/log/logger.hpp>

#include <mongocxx/exception/exception.hpp>

#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/value_context.hpp>
#include <bsoncxx/json.hpp>


namespace golos {
namespace plugins {
namespace mongo_db {

    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;

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

    bsoncxx::types::b_oid mongo_db_connector::get_comment_oid(const std::string& auth, const std::string& permlink) {
        auto comments = mongo_database["comment_object"];
        auto comment_doc = comments.find_one(document{} << "author" << auth << "permlink" << permlink << finalize);
        auto comment = comment_doc->view();
        auto id = comment["_id"].get_oid();
        return id;
    }
}}}
