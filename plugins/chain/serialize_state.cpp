#include <golos/plugins/chain/plugin.hpp>

#include <golos/chain/steem_object_types.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/proposal_object.hpp>
#include <golos/chain/transaction_object.hpp>
#include <golos/chain/block_summary_object.hpp>
// #include <golos/plugins/follow/follow_objects.hpp>

#include <boost/filesystem/fstream.hpp>
#include <fc/crypto/sha256.hpp>


namespace fc {

template<typename S, typename T>
inline datastream<S>& operator<<(datastream<S>& s, const chainbase::object_id<T>& id) {
    s.write((const char*)&id._id, sizeof(id._id));
    return s;
}
template<typename S>
inline datastream<S>& operator<<(datastream<S>& s, const chainbase::shared_string& x) {
    std::string t(x.data(), x.size());
    fc::raw::pack(s, t);
    return s;
}

} // fc

namespace golos { namespace plugins { namespace chain {


namespace bfs = boost::filesystem;
using namespace golos::chain;


class ofstream_sha256: public bfs::ofstream {
public:
    ofstream_sha256(const bfs::path& p): bfs::ofstream(p, std::ios_base::binary) {
        bfs::ofstream::exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }
    ~ofstream_sha256() {
    }

    template<typename T>
    void write(const T& x) {
        write((const char*)&x, sizeof(T));
    }
    void write(const char* p, uint32_t l) {
        _e.write(p, l);
        bfs::ofstream::write(p, l);
    }
    fc::sha256 hash() {
        return _e.result();
    }

private:
    fc::sha256::encoder _e;
};


struct state_header {
    char magic[12] = "Golos\astatE";
    uint32_t tables_count;
};
struct table_header {
    uint32_t type_id;
    uint32_t records_count;
};

template<typename Idx>
void serialize_table(const database& db, ofstream_sha256& out) {
    auto start = fc::time_point::now();
    size_t n = 0, l = 0;
    uint32_t min = -1, max = 0;

    const auto& generic = db.get_index<Idx>();
    const auto& indices = generic.indicies();
    table_header hdr({chainbase::generic_index<Idx>::value_type::type_id, static_cast<uint32_t>(indices.size())});
    wlog("Saving ${name}, ${n} record(s), type: ${t}",
        ("name", generic.name())("n", hdr.records_count)("t", hdr.type_id));
    out.write(hdr);

    const auto& idx = indices.template get<by_id>();
    auto itr = idx.begin();
    auto etr = idx.end();
    for (; itr != etr; itr++) {
        auto& item = *itr;
        auto data = fc::raw::pack(item);
        auto sz = data.size();
        if (sz < min) min = sz;
        if (sz > max) max = sz;
        l += sz;
        out.write(data.data(), sz);
        n++;
    }
    auto end = fc::time_point::now();
    ilog("  done, ${n} record(s) ${min}-${max} bytes each (${s.1} avg, ${l} total) saved in ${t} sec",
        ("n", n)("min", min)("max", max)("l", l)("s", double(l)/n)
        ("t", double((end - start).count()) / 1000000.0));
}


void plugin::serialize_state(const bfs::path& output) {
    // can't throw here, because if will be false-detected as db opening error, which can kill state
    try {
        ofstream_sha256 out(output);
        auto start = fc::time_point::now();
        wlog("---------------------------------------------------------------------------");
        wlog("Serializing state to ${dst}", ("dst",output.string()));
        auto& db_ = db();
        auto hdr = state_header{};
        hdr.tables_count = db_.index_list_size();
        out.write(hdr);

        for (auto i = db_.index_list_begin(), e = db_.index_list_end(); e != i; ++i) {
            auto idx = *i;
            ilog("index `${i}` (rev:${r}, type:${t}) contains ${l} records",
                ("i",idx->name())("l",idx->size())("r",idx->revision())("t",idx->type_id()));
        }
        ilog("---------------------------------------------------------------------------");

#define STORE(T) serialize_table<T>(db_, out);
        STORE(dynamic_global_property_index);
        STORE(account_index);
        STORE(account_authority_index);
        STORE(account_bandwidth_index);
        STORE(witness_index);
        STORE(transaction_index);
        STORE(block_summary_index);
        STORE(witness_schedule_index);
        STORE(comment_index);
        STORE(comment_vote_index);
        STORE(witness_vote_index);
        STORE(limit_order_index);
        // STORE(feed_history_index);
        STORE(convert_request_index);
        STORE(liquidity_reward_balance_index);
        // STORE(hardfork_property_index);
        STORE(withdraw_vesting_route_index);
        STORE(owner_authority_history_index);
        STORE(account_recovery_request_index);
        STORE(change_recovery_account_request_index);
        STORE(escrow_index);
        STORE(savings_withdraw_index);
        STORE(decline_voting_rights_request_index);
        STORE(vesting_delegation_index);
        STORE(vesting_delegation_expiration_index);
        STORE(account_metadata_index);
        // STORE(proposal_index);
        // STORE(required_approval_index);

        // STORE(golos::plugins::follow::follow_index);
        // STORE(golos::plugins::follow::feed_index);
        // STORE(golos::plugins::follow::blog_index);
        // STORE(golos::plugins::follow::reputation_index);
        // STORE(golos::plugins::follow::follow_count_index);
        // STORE(golos::plugins::follow::blog_author_stats_index);
        // STORE(golos::plugins::social_network::comment_content_index);
        // STORE(golos::plugins::social_network::comment_last_update_index);
        // STORE(golos::plugins::social_network::comment_reward_index);
#undef STORE

        auto end = fc::time_point::now();
        wlog("Done in ${t} sec.", ("t", double((end - start).count()) / 1000000.0));
        wlog("SHA256 hash: ${h}", ("h", out.hash().str()));
        out.close();

    } catch (const boost::exception& e) {
        std::cerr << boost::diagnostic_information(e) << "\n";
    } catch (const fc::exception& e) {
        std::cerr << e.to_detail_string() << "\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
    } catch (...) {
        std::cerr << "unknown exception\n";
    }
}

}}} // golos::plugins::chain

// missing reflections
FC_REFLECT((golos::chain::comment_object),
    (id)(parent_author)(parent_permlink)(author)(permlink)(created)(last_payout)(depth)(children)
    (children_rshares2)(net_rshares)(abs_rshares)(vote_rshares)(children_abs_rshares)(cashout_time)(max_cashout_time)
    (reward_weight)(net_votes)(total_votes)(root_comment)(mode)
    (curation_reward_curve)(auction_window_reward_destination)(auction_window_size)(max_accepted_payout)
    (percent_steem_dollars)(allow_replies)(allow_votes)(allow_curation_rewards)(curation_rewards_percent)
    (beneficiaries));

FC_REFLECT((golos::chain::delegator_vote_interest_rate), (account)(interest_rate)(payout_strategy));

FC_REFLECT((golos::chain::comment_vote_object),
    (id)(voter)(comment)(orig_rshares)(rshares)(vote_percent)(auction_time)(last_update)(num_changes)
    (delegator_vote_interest_rates))

FC_REFLECT((golos::chain::witness_vote_object), (id)(witness)(account));
