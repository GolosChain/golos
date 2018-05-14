#include <golos/plugins/mongo_db/mongo_db_deleter.hpp>
#include <golos/plugins/mongo_db/mongo_db_writer.hpp>

#include <fc/log/logger.hpp>

#include <mongocxx/exception/exception.hpp>


namespace golos {
namespace plugins {
namespace mongo_db {

    using bsoncxx::builder::stream::finalize;

    mongo_db_deleter::mongo_db_deleter() : mongo_db_connector() {
    }

    mongo_db_deleter::~mongo_db_deleter() {
    }

    bool mongo_db_deleter::remove_document(const named_document_ptr& named_doc) {
        if (removed_blocks.find(named_doc->collection_name) == removed_blocks.end()) {
            removed_blocks[named_doc->collection_name] = std::make_unique<mongocxx::bulk_write>(bulk_opts);
        }

        auto view = named_doc->doc.view();
        auto itr = view.find("_id");
        if (view.end() == itr) {
            return false;
        }
        document filter;
        filter << "_id" << view["_id"].get_oid();
        mongocxx::model::delete_one msg{filter.view()};
        removed_blocks[named_doc->collection_name]->append(msg);

        return true;
    }

    void mongo_db_deleter::write_changes() {
        auto iter = removed_blocks.begin();
        for (; iter != removed_blocks.end(); ++iter) {

            auto& oper = *iter;
            try {
                const std::string& collection_name = oper.first;
                mongocxx::collection _collection = mongo_database[collection_name];

                auto& bulkp = oper.second;
                if (!_collection.bulk_write(*bulkp)) {
                    ilog("Failed to remove documents from Mongo DB");
                }
            }
            catch (const std::exception& e) {
                ilog("Unknown exception while remove documents from mongo: ${e}", ("e", e.what()));
                // If we got some errors removing document from mongo just skip this document and move on
                removed_blocks.erase(iter);
                throw;
            }
        }
        removed_blocks.clear();
    }

    void mongo_db_deleter::delete_comment(const std::string& auth, const std::string& permlink) {

        mongocxx::collection _collection = mongo_database["comment_object"];
        _collection.delete_one(document{} << "author" << auth << "permlink" << permlink << finalize);

        _collection = mongo_database["comment_content_object"];
        _collection.delete_one(document{} << "comment_permlink" << permlink << finalize); 
    }

}}}
