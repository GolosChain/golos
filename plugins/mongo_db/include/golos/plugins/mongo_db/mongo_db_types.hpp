#pragma once

#include <golos/protocol/block.hpp>
#include <golos/chain/database.hpp>
#include <golos/protocol/transaction.hpp>
#include <golos/protocol/operations.hpp>

#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/value_context.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/optional.hpp>

#include <fc/crypto/sha1.hpp>

namespace golos {
namespace plugins {
namespace mongo_db {

    using namespace golos::chain;
    using namespace golos::protocol;
    using golos::chain::to_string;
    using golos::chain::shared_string;
    using bsoncxx::builder::stream::document;

    struct named_document {
        std::string collection_name;
        document doc;
        bool is_removal;
        //bool is_virtual;
        std::string index_to_create;
        std::string key;
        std::string keyval;
    };

    struct hashed_idx;

        typedef boost::multi_index_container<
            named_document,
            boost::multi_index::indexed_by<
                boost::multi_index::random_access<
                >,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<hashed_idx>,
                    boost::multi_index::composite_key<
                        named_document,
                        boost::multi_index::member<named_document,std::string,&named_document::collection_name>,
                        boost::multi_index::member<named_document,std::string,&named_document::key>,
                        boost::multi_index::member<named_document,std::string,&named_document::keyval>,
                        boost::multi_index::member<named_document,bool,&named_document::is_removal>//,
                        //boost::multi_index::member<named_document,bool,&named_document::is_virtual>
                    >
                >
            >
        > db_map;

    void bmi_insert_or_replace(db_map& bmi, named_document doc);

    //void bmi_merge(db_map& bmi_what, db_map& bmi_with);

    using named_document_ptr = std::unique_ptr<named_document>;

    inline void format_oid(document& doc, const std::string& name, const std::string& value) {
        auto oid = fc::sha1::hash(value).str().substr(0, 24);
        doc << name << bsoncxx::oid(oid);
    }

    inline void format_oid(document& doc, const std::string& value) {
        format_oid(doc, "_id", value);
    }

    // Helper functions
    inline void format_value(document& doc, const std::string& name, const asset& value) {
        doc << name + "_value" << value.to_real();
        doc << name + "_symbol" << value.symbol_name();
    }

    inline void format_value(document& doc, const std::string& name, const std::string& value) {
        doc << name << value;
    }

    inline void format_value(document& doc, const std::string& name, const bool value) {
        doc << name << value;
    }

    inline void format_value(document& doc, const std::string& name, const double value) {
        doc << name << value;
    }

    inline void format_value(document& doc, const std::string& name, const fc::uint128_t& value) {
        doc << name << static_cast<int64_t>(value.lo);
    }

    inline void format_value(document& doc, const std::string& name, const fc::time_point_sec& value) {
        doc << name << value.to_iso_string();
    }

    inline void format_value(document& doc, const std::string& name, const shared_string& value) {
        doc << name << to_string(value);
    }

    template <typename T>
    inline void format_value(document& doc, const std::string& name, const fc::fixed_string<T>& value) {
        doc << name << static_cast<std::string>(value);
    }

    template <typename T>
    inline void format_value(document& doc, const std::string& name, const T& value) {
        doc << name << static_cast<int64_t>(value);
    }

    template <typename T>
    inline void format_value(document& doc, const std::string& name, const fc::safe<T>& value) {
        doc << name << static_cast<int64_t>(value.value);
    }
    
}}} // golos::plugins::mongo_db
