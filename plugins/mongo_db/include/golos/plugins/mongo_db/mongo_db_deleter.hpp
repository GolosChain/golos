#pragma once

#include <golos/plugins/mongo_db/mongo_db_connector.hpp>
#include <golos/plugins/mongo_db/mongo_db_types.hpp>

#include <appbase/application.hpp>


namespace golos {
namespace plugins {
namespace mongo_db {

    using bulk_ptr = std::unique_ptr<mongocxx::bulk_write>;

    class mongo_db_deleter final : public mongo_db_connector {
    public:
        mongo_db_deleter();
        mongo_db_deleter(const mongo_db_deleter&) = delete;
        mongo_db_deleter(mongo_db_deleter&&) = default;
        ~mongo_db_deleter();
        mongo_db_deleter& operator=(const mongo_db_deleter&) = delete;
        mongo_db_deleter& operator=(mongo_db_deleter&&) = default;

        bool remove_document(const named_document_ptr& named_doc);

        void write_changes();

        void delete_author_reward(const std::string& auth, const std::string& permlink);

        void delete_benefactor_reward(const std::string& auth, const std::string& permlink);

        void delete_comment_content(const std::string& auth, const std::string& permlink);

        void delete_comment_reward(const std::string& auth, const std::string& permlink);

        void delete_comment_vote(const std::string& auth, const std::string& permlink);

        void delete_curation_reward(const std::string& auth, const std::string& permlink);

        void delete_comment(const std::string& auth, const std::string& permlink);

    private:
        // Table name, bulk remove
        std::map<std::string, bulk_ptr> removed_blocks;
    protected:
        bsoncxx::types::b_oid get_comment_oid(const std::string& auth, const std::string& permlink);
    };
}}}

