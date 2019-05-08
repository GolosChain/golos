#pragma once

#include <golos/plugins/operation_dump/operation_dump_container.hpp>
#include <golos/protocol/operations.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/plugins/follow/follow_operations.hpp>
#include <golos/plugins/tags/tag_visitor.hpp>
#include <queue>

namespace golos { namespace plugins { namespace operation_dump {

using namespace golos::plugins::follow;

#define COMMENT_ID(OP) hash_id(std::string(OP.author) + "/" + OP.permlink)

#define TAGS_NUMBER 15
#define TAG_MAX_LENGTH 512

class operation_dump_visitor {
public:
    using result_type = void;

    dump_buffers& _buffers;

    const signed_block& _block;
    uint16_t& _op_in_block;

    database& _db;

    std::queue<uint64_t> rshares_offsets;

    operation_dump_visitor(dump_buffers& buffers, const signed_block& block, uint16_t& op_in_block, database& db)
            : _buffers(buffers), _block(block), _op_in_block(op_in_block), _db(db) {
    }

    uint64_t hash_id(const std::string& id) {
        return fc::hash64(id.c_str(), id.length());
    }

    dump_buffer& write_op_header(const std::string& file_name, uint64_t op_related_id) {
        auto& b = _buffers[file_name];
        b.write(operation_number(_block.block_num(), _op_in_block));
        fc::raw::pack(b, op_related_id);
        return b;
    }

    template<typename T>
    auto operator()(const T&) -> result_type {
    }

    auto operator()(const transfer_operation& op) -> result_type {
        auto& b = write_op_header("transfers", 0);

        fc::raw::pack(b, op);
    }

    auto operator()(const comment_operation& op) -> result_type {
        auto& b = write_op_header("comments", COMMENT_ID(op));

        fc::raw::pack(b, op.parent_author);
        fc::raw::pack(b, op.parent_permlink);
        fc::raw::pack(b, op.author);
        fc::raw::pack(b, op.permlink);
        fc::raw::pack(b, op.title);
        fc::raw::pack(b, op.body);

        auto meta = golos::plugins::tags::get_metadata(op.json_metadata, TAGS_NUMBER, TAG_MAX_LENGTH);
        fc::raw::pack(b, meta);
    }

    auto operator()(const delete_comment_operation& op) -> result_type {
        if (_db.find_comment(op.author, op.permlink)) {
            return;
        }

        write_op_header("delete_comments", COMMENT_ID(op));
    }

    auto operator()(const comment_benefactor_reward_operation& op) -> result_type {
        auto& b = write_op_header("benefactor_rewards", COMMENT_ID(op));

        fc::raw::pack(b, op);
    }

    auto operator()(const author_reward_operation& op) -> result_type {
        auto& b = write_op_header("author_rewards", COMMENT_ID(op));

        fc::raw::pack(b, op);
    }

    auto operator()(const curation_reward_operation& op) -> result_type {
        auto& b = write_op_header("curation_rewards", hash_id(std::string(op.comment_author) + "/" + op.comment_permlink));

        fc::raw::pack(b, op);
    }

    auto operator()(const auction_window_reward_operation& op) -> result_type {
        auto& b = write_op_header("auction_window_rewards", hash_id(std::string(op.comment_author) + "/" + op.comment_permlink));

        fc::raw::pack(b, op);
    }

    auto operator()(const total_comment_reward_operation& op) -> result_type {
        auto& b = write_op_header("total_comment_rewards", COMMENT_ID(op));

        fc::raw::pack(b, op);
    }

    auto operator()(const vote_operation& op) -> result_type {
        auto& b = write_op_header("votes", COMMENT_ID(op));

        fc::raw::pack(b, op);

        fc::raw::pack(b, _block.timestamp);

        rshares_offsets.push(b.tellp());
        fc::raw::pack(b, int64_t(0));
    }

    auto operator()(const vote_rshares_operation& op) -> result_type {
        auto& b = _buffers["votes"];

        auto pos = b.tellp();
        b.seekp(rshares_offsets.front());
        fc::raw::pack(b, op.rshares);
        b.seekp(pos);
        rshares_offsets.pop();
    }

    // Not logs if operation failed in plugin, but logs if plugin not exists
    auto operator()(const custom_json_operation& op) -> result_type {
        if (op.id != "follow") { // follows, reblogs, delete_reblogs
            return;
        }

        std::vector<follow_plugin_operation> fpops;

        auto v = fc::json::from_string(op.json);
        try {
            if (v.is_array() && v.size() > 0 && v.get_array()[0].is_array()) {
                fc::from_variant(v, fpops);
            } else {
                fpops.emplace_back();
                fc::from_variant(v, fpops[0]);
            }
        } catch (...) {
            // Normal cases failed, try this strange case from follow-plugin
            try {
                auto fop = v.as<follow_operation>();
                fpops.emplace_back(fop);
            } catch (...) {
            }
        }

        for (const follow_plugin_operation& fpop : fpops) {
            fpop.visit(*this);
        }
    }

    auto operator()(const follow_operation& op) -> result_type {
        auto& b = write_op_header("follows", hash_id(std::string(op.follower) + "/" + op.following));

        fc::raw::pack(b, op.follower);
        fc::raw::pack(b, op.following);

        uint16_t what = 0;
        for (const auto& target : op.what) {
            if (target == "blog") {
                what |= 1 << blog;
            } else if (target == "ignore") {
                what |= 1 << ignore;
            }
        }
        fc::raw::pack(b, what);
    }

    auto operator()(const reblog_operation& op) -> result_type {
        auto& b = write_op_header("reblogs", COMMENT_ID(op));

        fc::raw::pack(b, op.account);
        fc::raw::pack(b, op.author);
        fc::raw::pack(b, op.permlink);
        fc::raw::pack(b, op.title); // Usually empty, but no problems to dump
        fc::raw::pack(b, op.body);
        // Usually it is:
        // {"app": "golos.io/0.1", "format": "text"}
        // Not seems to be need
        //fc::raw::pack(b, op.json_metadata);

        fc::raw::pack(b, _block.timestamp);
    }

    auto operator()(const delete_reblog_operation& op) -> result_type {
        auto& b = write_op_header("delete_reblogs", COMMENT_ID(op));

        fc::raw::pack(b, op.account);
    }
};

} } } // golos::plugins::operation_dump
