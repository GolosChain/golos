#include <golos/plugins/operation_dump/operation_dump_plugin.hpp>
#include <golos/plugins/operation_dump/operation_dump_container.hpp>
#include <golos/plugins/operation_dump/operation_dump_visitor.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <appbase/application.hpp>

namespace golos { namespace plugins { namespace operation_dump {

namespace bfs = boost::filesystem;

using block_operations = std::map<uint32_t, std::vector<operation>>;

struct post_operation_clarifier {
    operation_dump_plugin& _plugin;
    golos::chain::database& _db;
    uint32_t _block_num;

    post_operation_clarifier(operation_dump_plugin& plugin, golos::chain::database& db, uint32_t block_num)
            : _plugin(plugin), _db(db), _block_num(block_num) {
    }

    typedef void result_type;

    template<typename T>
    void add_clarification(clarifications<T>& clars, T value) const {
        clars[_block_num].push(value);
        // removing forks and rollbacks
        auto itr = clars.find(_block_num);
        ++itr;
        clars.erase(itr, clars.end());
    }

    template<typename T>
    result_type operator()(const T&) const {
    }

    result_type operator()(const vote_operation& op) const {
        const auto& comment = _db.get_comment(op.author, op.permlink);
        const auto& vote_idx = _db.get_index<comment_vote_index, by_comment_voter>();
        auto vote_itr = vote_idx.find(std::make_tuple(comment.id, _db.get_account(op.voter).id));

        add_clarification(_plugin.vote_rshares, vote_itr->rshares);
    }

    result_type operator()(const delete_comment_operation& op) const {
        auto not_deleted = (_db.find_comment(op.author, op.permlink) != nullptr);

        add_clarification(_plugin.not_deleted_comments, not_deleted);
    }

    result_type operator()(const transfer_operation& op) const {
        auto golos_amount = (op.amount.symbol == STEEM_SYMBOL) ? op.amount : _db.to_steem(op.amount);

        add_clarification(_plugin.transfer_golos_amounts, golos_amount);
    }
};

class operation_dump_plugin::operation_dump_plugin_impl final {
public:
    operation_dump_plugin_impl(operation_dump_plugin& self)
            : _plugin(self), _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    ~operation_dump_plugin_impl() {
    }

    void erase_block(uint32_t block_num) {
        virtual_ops.erase(block_num);
        _plugin.vote_rshares.erase(block_num);
        _plugin.not_deleted_comments.erase(block_num);
        _plugin.transfer_golos_amounts.erase(block_num);
    }

    void on_block(const signed_block& block) {
        auto lib = _db.last_non_undoable_block_num();

        for (auto block_num = start_block; block_num <= lib; ++block_num) {
            auto block = _db.get_block_log().read_block_by_num(block_num);
            if (!block) {
               return;
            }

            try {
                uint16_t op_in_block = 0;

                operation_dump_visitor op_visitor(_plugin, *block, op_in_block, _db);

                for (const auto& trx : block->transactions) {
                    for (const auto& op : trx.operations) {
                        op.visit(op_visitor);
                        ++op_in_block;
                    }
                }

                for (const auto& op : virtual_ops[block_num]) {
                    op.visit(op_visitor);
                    ++op_in_block;
                }
            } catch (...) {
                erase_block(block_num);
                start_block = block_num+1;
                throw;
            }

            erase_block(block_num);
        }

        start_block = lib+1;

        for (auto& it : _plugin.buffers) {
            bfs::create_directories(operation_dump_dir);
            dump_file file(operation_dump_dir / it.first);
            if (file.tellp() == 0) {
                file.write(dump_header());
            }
            file << it.second.rdbuf();
        }
        _plugin.buffers.clear();
    }

    void on_operation(const operation_notification& note) {
        if (is_virtual_operation(note.op)) {
            virtual_ops[note.block].push_back(note.op);
            // remove ops if there were forks and rollbacks
            auto itr = virtual_ops.find(note.block);
            ++itr;
            virtual_ops.erase(itr, virtual_ops.end());
        } else if (!_db.is_generating() && !_db.is_producing()) {
            note.op.visit(post_operation_clarifier(_plugin, _db, note.block));
        }
    }

    operation_dump_plugin& _plugin;

    database& _db;

    bfs::path operation_dump_dir;

    uint32_t start_block = 1;

    block_operations virtual_ops;
};

operation_dump_plugin::operation_dump_plugin() = default;

operation_dump_plugin::~operation_dump_plugin() = default;

const std::string& operation_dump_plugin::name() {
    static std::string name = "operation_dump";
    return name;
}

void operation_dump_plugin::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
    cfg.add_options() (
        "operation-dump-dir", bpo::value<bfs::path>()->default_value("operation_dump"),
        "The location of the dir to dump operations to (abs path or relative to application data dir)."
    );
}

void operation_dump_plugin::plugin_initialize(const bpo::variables_map& options) {
    ilog("Initializing operation dump plugin");

    my = std::make_unique<operation_dump_plugin::operation_dump_plugin_impl>(*this);

    auto odd = options.at("operation-dump-dir").as<bfs::path>();
    if (odd.is_relative()) {
        my->operation_dump_dir = appbase::app().data_dir() / odd;
    } else {
        my->operation_dump_dir = odd;
    }

    my->_db.applied_block.connect([&](const signed_block& b) {
        my->on_block(b);
    });

    my->_db.post_apply_operation.connect([&](const operation_notification& note) {
        my->on_operation(note);
    });
}

void operation_dump_plugin::plugin_startup() {
    ilog("Starting up operation dump plugin");
}

void operation_dump_plugin::plugin_shutdown() {
    ilog("Shutting down operation dump plugin");
}

} } } // golos::plugins::operation_dump
