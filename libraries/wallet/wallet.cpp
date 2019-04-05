#include <graphene/utilities/git_revision.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/utilities/words.hpp>

#include <golos/protocol/base.hpp>
#include <golos/wallet/wallet.hpp>
#include <golos/wallet/api_documentation.hpp>
#include <golos/wallet/reflect_util.hpp>
#include <golos/wallet/remote_node_api.hpp>
#include <golos/protocol/config.hpp>
#include <golos/protocol/validate_helper.hpp>
#include <golos/plugins/follow/follow_operations.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <list>

#include <boost/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm/sort.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <fc/container/deque.hpp>
#include <fc/git_revision.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/macros.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/smart_ref_impl.hpp>

#ifndef WIN32
# include <sys/types.h>
# include <sys/stat.h>
#endif

#define WALLET_CHECK_UNLOCKED() \
    GOLOS_ASSERT(!is_locked(), golos::wallet::wallet_is_locked, "The wallet must be unlocked before operation")

#define BRAIN_KEY_WORD_COUNT 16

namespace golos { namespace wallet {

        struct logic_errors {
            enum types {
                detected_private_key_in_memo,
                owner_authority_change_would_render_account_irrecoverable,
                private_key_not_available,
                no_account_in_lut,
                malformed_private_key,
            };
        };

        GOLOS_DECLARE_DERIVED_EXCEPTION(
            wallet_is_locked, golos::golos_exception,
            10000, "The wallet must be unlocked");

} } // golos::wallet

namespace golos {

template<>
std::string get_logic_error_namespace<golos::wallet::logic_errors::types>() {
    return "wallet";
}

} // golos

namespace golos { namespace wallet {

        namespace detail {

            template<class T>
            optional<T> maybe_id( const string& name_or_id ) {
                if( std::isdigit( name_or_id.front() ) ) {
                    try {
                        return fc::variant(name_or_id).as<T>();
                    }
                    catch (const fc::exception&) {
                    }
                }
                return optional<T>();
            }

            string pubkey_to_shorthash( const public_key_type& key ) {
                uint32_t x = fc::sha256::hash(key)._hash[0];
                static const char hd[] = "0123456789abcdef";
                string result;

                result += hd[(x >> 0x1c) & 0x0f];
                result += hd[(x >> 0x18) & 0x0f];
                result += hd[(x >> 0x14) & 0x0f];
                result += hd[(x >> 0x10) & 0x0f];
                result += hd[(x >> 0x0c) & 0x0f];
                result += hd[(x >> 0x08) & 0x0f];
                result += hd[(x >> 0x04) & 0x0f];
                result += hd[(x        ) & 0x0f];

                return result;
            }


            fc::ecc::private_key derive_private_key( const std::string& prefix_string, int sequence_number ) {
                std::string sequence_string = std::to_string(sequence_number);
                fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
                fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
                return derived_key;
            }

            string normalize_brain_key(string s) {
                size_t i = 0, n = s.length();
                std::string result;
                result.reserve(n);

                bool preceded_by_whitespace = false;
                bool non_empty = false;
                while (i < n) {
                    char c = s[i++];
                    switch (c) {
                        case ' ': case '\t': case '\r': case '\n': case '\v': case '\f':
                            preceded_by_whitespace = true;
                            continue;

                        case 'a':
                        case 'b':
                        case 'c':
                        case 'd':
                        case 'e':
                        case 'f':
                        case 'g':
                        case 'h':
                        case 'i':
                        case 'j':
                        case 'k':
                        case 'l':
                        case 'm':
                        case 'n':
                        case 'o':
                        case 'p':
                        case 'q':
                        case 'r':
                        case 's':
                        case 't':
                        case 'u':
                        case 'v':
                        case 'w':
                        case 'x':
                        case 'y':
                        case 'z':
                            c ^= 'a' ^ 'A';     // ASCII upper/lowercase diffs only in 1 bit
                            break;

                        default:
                            break;
                    }
                    if (preceded_by_whitespace && non_empty)
                        result.push_back(' ');
                    result.push_back(c);
                    preceded_by_whitespace = false;
                    non_empty = true;
                }
                return result;
            }

            struct op_prototype_visitor {
                typedef void result_type;

                int t = 0;
                flat_map< std::string, operation >& name2op;

                op_prototype_visitor(
                        int _t,
                        flat_map< std::string, operation >& _prototype_ops
                ):t(_t), name2op(_prototype_ops) {}

                template<typename Type>
                result_type operator()( const Type& op )const {
                    string name = fc::get_typename<Type>::name();
                    size_t p = name.rfind(':');
                    if( p != string::npos )
                        name = name.substr( p+1 );
                    name2op[ name ] = Type();
                }
            };

            class wallet_api_impl
            {
            public:
                api_documentation method_documentation;
            private:
                void enable_umask_protection() {
#ifdef __unix__
                    _old_umask = umask( S_IRWXG | S_IRWXO );
#endif
                }

                void disable_umask_protection() {
#ifdef __unix__
                    umask( _old_umask );
#endif
                }

                void init_prototype_ops() {
                    operation op;
                    for( int t=0; t<op.count(); t++ ) {
                        op.set_which( t );
                        op.visit( op_prototype_visitor(t, _prototype_ops) );
                    }
                    return;
                }

                map<transaction_handle_type, signed_transaction> _builder_transactions;

            public:
                wallet_api& self;
                wallet_api_impl( wallet_api& s, const wallet_data& initial_data, const golos::protocol::chain_id_type& _steem_chain_id, fc::api_connection& con ):
                    self( s ),
                    _remote_database_api( con.get_remote_api< remote_database_api >( 0, "database_api" ) ),
                    _remote_operation_history( con.get_remote_api< remote_operation_history >( 0, "operation_history" ) ),
                    _remote_account_history( con.get_remote_api< remote_account_history >( 0, "account_history" ) ),
                    _remote_social_network( con.get_remote_api< remote_social_network >( 0, "social_network" ) ),
                    _remote_network_broadcast_api( con.get_remote_api< remote_network_broadcast_api >( 0, "network_broadcast_api" ) ),
                    _remote_follow( con.get_remote_api< remote_follow >( 0, "follow" ) ),
                    _remote_market_history( con.get_remote_api< remote_market_history >( 0, "market_history" ) ),
                    _remote_private_message( con.get_remote_api< remote_private_message>( 0, "private_message" ) ),
                    _remote_account_by_key( con.get_remote_api< remote_account_by_key>( 0, "account_by_key" ) ) ,
                    _remote_witness_api( con.get_remote_api< remote_witness_api >( 0, "witness_api" ) )
                {
                    init_prototype_ops();

                    _wallet.ws_server = initial_data.ws_server;
                    steem_chain_id = _steem_chain_id;
                }
                virtual ~wallet_api_impl()
                {}

                void encrypt_keys() {
                    if( !is_locked() ) {
                        plain_keys data;
                        data.keys = _keys;
                        data.checksum = _checksum;
                        auto plain_txt = fc::raw::pack(data);
                        _wallet.cipher_keys = fc::aes_encrypt( data.checksum, plain_txt );
                    }
                }

                bool copy_wallet_file( string destination_filename ) {
                    fc::path src_path = get_wallet_filename();
                    if( !fc::exists( src_path ) )
                        return false;
                    fc::path dest_path = destination_filename + _wallet_filename_extension;
                    int suffix = 0;
                    while( fc::exists(dest_path) ) {
                        ++suffix;
                        dest_path = destination_filename + "-" + std::to_string( suffix ) + _wallet_filename_extension;
                    }
                    wlog( "backing up wallet ${src} to ${dest}",
                          ("src", src_path)
                                  ("dest", dest_path) );

                    fc::path dest_parent = fc::absolute(dest_path).parent_path();
                    try {
                        enable_umask_protection();
                        if( !fc::exists( dest_parent ) )
                            fc::create_directories( dest_parent );
                        fc::copy( src_path, dest_path );
                        disable_umask_protection();
                    }
                    catch(...) {
                        disable_umask_protection();
                        throw;
                    }
                    return true;
                }

                bool is_locked()const {
                    return _checksum == fc::sha512();
                }

                variant info() const {
                    auto dynamic_props = _remote_database_api->get_dynamic_global_properties();
                    auto median_props = _remote_database_api->get_chain_properties();
                    fc::mutable_variant_object result(fc::variant(dynamic_props).get_object());
                    result["witness_majority_version"] =
                        std::string(_remote_witness_api->get_witness_schedule().majority_version);
                    result["hardfork_version"] =
                        std::string(_remote_database_api->get_hardfork_version());
                    result["head_block_num"] = dynamic_props.head_block_number;
                    result["head_block_id"] = dynamic_props.head_block_id;
                    result["head_block_age"] =
                        fc::get_approximate_relative_time_string(
                            dynamic_props.time, time_point_sec(time_point::now()), " old");
                    result["participation"] =
                        (100 * dynamic_props.recent_slots_filled.popcount()) / 128.0;
                    result["median_sbd_price"] = _remote_witness_api->get_current_median_history_price();
                    result["account_creation_fee"] = median_props.account_creation_fee;

                    auto hf = _remote_database_api->get_hardfork_version();
                    if (hf >= hardfork_version(0, STEEMIT_HARDFORK_0_18)) {
                        result["create_account_min_golos_fee"] = median_props.create_account_min_golos_fee;
                        result["create_account_min_delegation"] = median_props.create_account_min_delegation;
                        result["create_account_delegation_time"] = median_props.create_account_delegation_time;
                        result["min_delegation"] = median_props.min_delegation;
                    }
                    if (hf >= hardfork_version(0, STEEMIT_HARDFORK_0_19)) {
                        result["max_referral_interest_rate"] = median_props.max_referral_interest_rate;
                        result["max_referral_term_sec"] = median_props.max_referral_term_sec;
                        result["min_referral_break_fee"] = median_props.min_referral_break_fee;
                        result["max_referral_break_fee"] = median_props.max_referral_break_fee;
                        result["posts_window"] = median_props.posts_window;
                        result["posts_per_window"] = median_props.posts_per_window;
                        result["comments_window"] = median_props.comments_window;
                        result["comments_per_window"] = median_props.comments_per_window;
                        result["votes_window"] = median_props.votes_window;
                        result["votes_per_window"] = median_props.votes_per_window;
                        result["auction_window_size"] = median_props.auction_window_size;
                        result["max_delegated_vesting_interest_rate"] = median_props.max_delegated_vesting_interest_rate;
                        result["custom_ops_bandwidth_multiplier"] = median_props.custom_ops_bandwidth_multiplier;
                        result["allow_distribute_auction_reward"] = median_props.allow_distribute_auction_reward;
                        result["allow_return_auction_reward_to_fund"] = median_props.allow_return_auction_reward_to_fund;
                        result["min_curation_percent"] = median_props.min_curation_percent;
                        result["max_curation_percent"] = median_props.max_curation_percent;
                        result["curation_reward_curve"] = median_props.curation_reward_curve;
                    }
                    if (hf >= hardfork_version(0, STEEMIT_HARDFORK_0_21)) {
                        result["worker_from_content_fund_percent"] = median_props.worker_from_content_fund_percent;
                        result["worker_from_vesting_fund_percent"] = median_props.worker_from_vesting_fund_percent;
                        result["worker_from_witness_fund_percent"] = median_props.worker_from_witness_fund_percent;
                        result["worker_techspec_approve_term_sec"] = median_props.worker_techspec_approve_term_sec;
                        result["worker_result_approve_term_sec"] = median_props.worker_result_approve_term_sec;
                    }

                    return result;
                }

                variant_object database_info() const {
                    auto info = _remote_database_api->get_database_info();

                    auto convert = [](std::size_t value) {
                        auto gb = value / (1024 * 1024 * 1024);
                        auto mb = value / (1024 * 1024);
                        auto kb = value / (1024);

                        if (gb) {
                            return std::to_string(gb) + "G";
                        } else if (mb) {
                            return std::to_string(mb) + "M";
                        } else if (kb) {
                            return std::to_string(kb) + "K";
                        }

                        return std::to_string(value);
                    };

                    fc::mutable_variant_object result;

                    result["total_size"] = convert(info.total_size);
                    result["used_size"] = convert(info.used_size);
                    result["free_size"] = convert(info.free_size);
                    result["reserved_size"] = convert(info.reserved_size);
                    result["index_list"] = info.index_list;

                    return result;
                }

                variant_object about() const {
                    string client_version( golos::utilities::git_revision_description );
                    const size_t pos = client_version.find( '/' );
                    if( pos != string::npos && client_version.size() > pos )
                        client_version = client_version.substr( pos + 1 );

                    fc::mutable_variant_object result;
                    //result["blockchain_version"]       = STEEM_BLOCKCHAIN_VERSION;
                    result["client_version"]           = client_version;
                    result["steem_revision"]           = golos::utilities::git_revision_sha;
                    result["steem_revision_age"]       = fc::get_approximate_relative_time_string( fc::time_point_sec( golos::utilities::git_revision_unix_timestamp ) );
                    result["fc_revision"]              = fc::git_revision_sha;
                    result["fc_revision_age"]          = fc::get_approximate_relative_time_string( fc::time_point_sec( fc::git_revision_unix_timestamp ) );
                    result["compile_date"]             = "compiled on " __DATE__ " at " __TIME__;
                    result["boost_version"]            = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
                    result["openssl_version"]          = OPENSSL_VERSION_TEXT;

                    std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int*)) + "-bit";
#if defined(__APPLE__)
                    std::string os = "osx";
#elif defined(__linux__)
                    std::string os = "linux";
#elif defined(_MSC_VER)
      std::string os = "win32";
#else
      std::string os = "other";
#endif
                    result["build"] = os + " " + bitness;

                    try {
                        //auto v = _remote_api->get_version();
                        //result["server_blockchain_version"] = v.blockchain_version;
                        //result["server_steem_revision"] = v.steem_revision;
                        //result["server_fc_revision"] = v.fc_revision;
                    } catch( fc::exception& ) {
                        result["server"] = "could not retrieve server version information";
                    }

                    return result;
                }

                fc::variant make_operation_id(transaction_handle_type handle, uint32_t op_index) {
                    return fc::mutable_variant_object()("transaction",handle)("op_index",op_index);
                }

                signed_transaction &get_builder_transaction(transaction_handle_type handle) {
                    auto trx = _builder_transactions.find(handle);
                    if (trx == _builder_transactions.end()) {
                        GOLOS_THROW_MISSING_OBJECT("transaction", handle);
                    }
                    return trx->second;
                }

                transaction_handle_type begin_builder_transaction() {
                    transaction_handle_type handle = 0;
                    if (!_builder_transactions.empty()) {
                        handle = (--_builder_transactions.end())->first + 1;
                    }
                    _builder_transactions[handle];
                    return handle;
                }

                void add_operation_to_builder_transaction(
                    transaction_handle_type handle, const operation& op
                ) {
                    auto& trx = get_builder_transaction(handle);
                    trx.operations.emplace_back(op);
                }

                void add_operation_copy_to_builder_transaction(
                    transaction_handle_type src_handle,
                    transaction_handle_type dst_handle,
                    uint32_t op_index
                ) {
                    auto& src_trx = get_builder_transaction(src_handle);
                    auto& dst_trx = get_builder_transaction(dst_handle);
                    if (op_index >= src_trx.operations.size())
                        GOLOS_THROW_MISSING_OBJECT("operation", make_operation_id(src_handle,op_index));
                    const auto op = src_trx.operations[op_index];
                    dst_trx.operations.emplace_back(op);
                }

                void replace_operation_in_builder_transaction(
                    transaction_handle_type handle,
                    uint32_t op_index,
                    const operation& new_op
                ) {
                    auto& trx = get_builder_transaction(handle);
                    if (op_index >= trx.operations.size())
                        GOLOS_THROW_MISSING_OBJECT("operation", make_operation_id(handle,op_index));
                    trx.operations[op_index] = new_op;
                }

                transaction preview_builder_transaction(transaction_handle_type handle) {
                    return get_builder_transaction(handle);
                }

                signed_transaction sign_builder_transaction(transaction_handle_type handle, bool broadcast) {
                    auto& trx = get_builder_transaction(handle);
                    return _builder_transactions[handle] = sign_transaction(trx, broadcast);
                }

                signed_transaction propose_builder_transaction(
                    transaction_handle_type handle,
                    std::string author,
                    std::string title,
                    std::string memo,
                    std::string expiration,
                    std::string review_period_time,
                    bool broadcast
                ) {
                    (void)get_builder_transaction(handle);
                    proposal_create_operation op;
                    op.author = author;
                    op.title = title;
                    op.memo = memo;

                    auto dyn_props = _remote_database_api->get_dynamic_global_properties();

                    op.expiration_time = time_converter(expiration,
                        dyn_props.time, dyn_props.time + STEEMIT_MAX_PROPOSAL_LIFETIME_SEC).time();

                    // copy tx to avoid malforming if sign_transaction fails
                    signed_transaction trx = _builder_transactions[handle];
                    std::transform(
                        trx.operations.begin(), trx.operations.end(), std::back_inserter(op.proposed_operations),
                        [](const operation& op) -> operation_wrapper { return op; });

                    auto review_period_time_sec = time_converter(review_period_time,
                        dyn_props.time, time_point_sec::min()).time();
                    if (review_period_time_sec > time_point_sec::min()) {
                        op.review_period_time = review_period_time_sec;
                    }

                    trx.operations = {op};
                    trx = sign_transaction(trx, broadcast);
                    return _builder_transactions[handle] = trx;
                }

                void remove_builder_transaction(transaction_handle_type handle) {
                    _builder_transactions.erase(handle);
                }

                signed_transaction approve_proposal(
                    const std::string& author,
                    const std::string& title,
                    const approval_delta& delta,
                    bool broadcast)
                {
                    proposal_update_operation update_op;

                    update_op.author = author;
                    update_op.title = title;

                    for (const std::string& name : delta.active_approvals_to_add)
                        update_op.active_approvals_to_add.insert(name);
                    for (const std::string& name : delta.active_approvals_to_remove)
                        update_op.active_approvals_to_remove.insert(name);
                    for (const std::string& name : delta.owner_approvals_to_add)
                        update_op.owner_approvals_to_add.insert(name);
                    for (const std::string& name : delta.owner_approvals_to_remove)
                        update_op.owner_approvals_to_remove.insert(name);
                    for (const std::string& name : delta.posting_approvals_to_add)
                        update_op.posting_approvals_to_add.insert(name);
                    for (const std::string& name : delta.posting_approvals_to_remove)
                        update_op.posting_approvals_to_remove.insert(name);
                    for (const std::string& k : delta.key_approvals_to_add)
                        update_op.key_approvals_to_add.insert(public_key_type(k));
                    for (const std::string& k : delta.key_approvals_to_remove)
                        update_op.key_approvals_to_remove.insert(public_key_type(k));

                    signed_transaction tx;
                    tx.operations.push_back(update_op);
                    tx.validate();
                    return sign_transaction(tx, broadcast);
                }

                std::vector<database_api::proposal_api_object> get_proposed_transactions(
                    std::string account, uint32_t from, uint32_t limit
                ) {
                    return _remote_database_api->get_proposed_transactions(account, from, limit);
                }

                golos::api::account_api_object get_account(const string& account_name) const {
                    auto accounts = _remote_database_api->get_accounts( { account_name } );
                    if (accounts.size() != 1 || account_name != accounts[0].name) {
                        GOLOS_THROW_MISSING_OBJECT("account", account_name);
                    }
                    return accounts.front();
                }

                string get_wallet_filename() const { return _wallet_filename; }

                optional<fc::ecc::private_key> try_get_private_key(const public_key_type& id)const {
                    auto it = _keys.find(id);
                    if( it != _keys.end() )
                        return wif_to_key( it->second );
                    return optional<fc::ecc::private_key>();
                }

                fc::ecc::private_key get_private_key(const public_key_type& id)const {
                    auto has_key = try_get_private_key( id );
                    if (!has_key)
                        GOLOS_THROW_MISSING_OBJECT("private_key", id);
                    return *has_key;
                }


                fc::ecc::private_key get_private_key_for_account(const golos::api::account_api_object& account)const {
                    vector<public_key_type> active_keys = account.active.get_keys();
                    if (active_keys.size() != 1)
                        FC_THROW("Expecting a simple authority with one active key");
                    return get_private_key(active_keys.front());
                }

                // imports the private key into the wallet, and associate it in some way (?) with the
                // given account name.
                // @returns true if the key matches a current active/owner/memo key for the named
                //          account, false otherwise (but it is stored either way)
                bool import_key(string wif_key) {
                    fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
                    if (!optional_private_key)
                        FC_THROW("Invalid private key");
                    golos::chain::public_key_type wif_pub_key = optional_private_key->get_public_key();

                    _keys[wif_pub_key] = wif_key;
                    return true;
                }

                bool load_wallet_file(string wallet_filename = "") {
                    // TODO: Merge imported wallet with existing wallet, instead of replacing it
                    if( wallet_filename == "" )
                        wallet_filename = _wallet_filename;

                    if( ! fc::exists( wallet_filename ) )
                        return false;

                    _wallet = fc::json::from_file( wallet_filename ).as< wallet_data >();

                    return true;
                }

                void save_wallet_file(string wallet_filename = "") {
                    //
                    // Serialize in memory, then save to disk
                    //
                    // This approach lessens the risk of a partially written wallet
                    // if exceptions are thrown in serialization
                    //

                    encrypt_keys();

                    if( wallet_filename == "" )
                        wallet_filename = _wallet_filename;

                    wlog( "saving wallet to file ${fn}", ("fn", wallet_filename) );

                    string data = fc::json::to_pretty_string( _wallet );
                    try {
                        enable_umask_protection();
                        //
                        // Parentheses on the following declaration fails to compile,
                        // due to the Most Vexing Parse. Thanks, C++
                        //
                        // http://en.wikipedia.org/wiki/Most_vexing_parse
                        //
                        fc::ofstream outfile{ fc::path( wallet_filename ) };
                        outfile.write( data.c_str(), data.length() );
                        outfile.flush();
                        outfile.close();
                        disable_umask_protection();
                    } catch(...) {
                        disable_umask_protection();
                        throw;
                    }
                }

                // This function generates derived keys starting with index 0 and keeps incrementing
                // the index until it finds a key that isn't registered in the block chain. To be
                // safer, it continues checking for a few more keys to make sure there wasn't a short gap
                // caused by a failed registration or the like.
                int find_first_unused_derived_key_index(const fc::ecc::private_key& parent_key) {
                    int first_unused_index = 0;
                    int number_of_consecutive_unused_keys = 0;
                    for (int key_index = 0; ; ++key_index) {
                        fc::ecc::private_key derived_private_key = derive_private_key(key_to_wif(parent_key), key_index);
                        golos::chain::public_key_type derived_public_key = derived_private_key.get_public_key();
                        if( _keys.find(derived_public_key) == _keys.end() ) {
                            if (number_of_consecutive_unused_keys) {
                                ++number_of_consecutive_unused_keys;
                                if (number_of_consecutive_unused_keys > 5)
                                    return first_unused_index;
                            } else {
                                first_unused_index = key_index;
                                number_of_consecutive_unused_keys = 1;
                            }
                        } else {
                            // key_index is used
                            first_unused_index = 0;
                            number_of_consecutive_unused_keys = 0;
                        }
                    }
                }

                signed_transaction create_account_with_private_key(fc::ecc::private_key owner_privkey,
                                                                   string account_name,
                                                                   string creator_account_name,
                                                                   bool broadcast = false,
                                                                   bool save_wallet = true) {
                    try {
                        int active_key_index = find_first_unused_derived_key_index(owner_privkey);
                        fc::ecc::private_key active_privkey = derive_private_key( key_to_wif(owner_privkey), active_key_index);

                        int memo_key_index = find_first_unused_derived_key_index(active_privkey);
                        fc::ecc::private_key memo_privkey = derive_private_key( key_to_wif(active_privkey), memo_key_index);

                        golos::chain::public_key_type owner_pubkey = owner_privkey.get_public_key();
                        golos::chain::public_key_type active_pubkey = active_privkey.get_public_key();
                        golos::chain::public_key_type memo_pubkey = memo_privkey.get_public_key();

                        account_create_operation account_create_op;

                        account_create_op.creator = creator_account_name;
                        account_create_op.new_account_name = account_name;
                        account_create_op.fee = _remote_database_api->get_chain_properties().account_creation_fee;
                        account_create_op.owner = authority(1, owner_pubkey, 1);
                        account_create_op.active = authority(1, active_pubkey, 1);
                        account_create_op.memo_key = memo_pubkey;

                        signed_transaction tx;

                        tx.operations.push_back( account_create_op );
                        tx.validate();

                        if( save_wallet )
                            save_wallet_file();
                        if( broadcast ) {
                            auto result = _remote_network_broadcast_api->broadcast_transaction_synchronous( tx );
                            FC_UNUSED(result);
                        }
                        return tx;
                    } FC_CAPTURE_AND_RETHROW( (account_name)(creator_account_name)(broadcast) ) }

                signed_transaction set_voting_proxy(string account_to_modify, string proxy, bool broadcast /* = false */) {
                    try {
                        account_witness_proxy_operation op;
                        op.account = account_to_modify;
                        op.proxy = proxy;

                        signed_transaction tx;
                        tx.operations.push_back( op );
                        tx.validate();

                        return sign_transaction( tx, broadcast );
                    } FC_CAPTURE_AND_RETHROW( (account_to_modify)(proxy)(broadcast) ) }

                optional< witness_api::witness_api_object > get_witness( string owner_account ) {
                    return _remote_witness_api->get_witness_by_account( owner_account );
                }

                void set_transaction_expiration( uint32_t tx_expiration_seconds ) {
                    GOLOS_CHECK_VALUE_LT( tx_expiration_seconds, STEEMIT_MAX_TIME_UNTIL_EXPIRATION );
                    _tx_expiration_seconds = tx_expiration_seconds;
                }

                annotated_signed_transaction sign_transaction(signed_transaction tx, bool broadcast = false)
                {
                    flat_set< account_name_type > req_active_approvals;
                    flat_set< account_name_type > req_owner_approvals;
                    flat_set< account_name_type > req_posting_approvals;
                    vector< authority > other_auths;

                    // gets required account names of operations
                    tx.get_required_authorities( req_active_approvals, req_owner_approvals,
                        req_posting_approvals, other_auths );

                    for( const auto& auth : other_auths )
                        for( const auto& a : auth.account_auths )
                            req_active_approvals.insert(a.first);

                    // collects all keys to common set and accounts to common map

                    flat_map<string, golos::api::account_api_object> approving_account_lut;

                    flat_set<public_key_type> approving_key_set;

                    std::vector<account_name_type> active_account_auths;
                    std::vector<account_name_type> owner_account_auths;
                    std::vector<account_name_type> posting_account_auths;

                    auto fetch_keys = [&](const authority& auth) {
                        for (const public_key_type& approving_key : auth.get_keys()) {
                            wdump((approving_key));
                            approving_key_set.insert( approving_key );
                        }
                    };

                    if (!req_active_approvals.empty()) {
                        auto req_active_accs =_remote_database_api->get_accounts(std::vector<account_name_type>(
                            req_active_approvals.begin(), req_active_approvals.end()));
                        for (auto& acc : req_active_accs) {
                            approving_account_lut[acc.name] = acc;
                            fetch_keys(acc.active);
                            for (const auto& auth : acc.active.account_auths) {
                                active_account_auths.push_back(auth.first);
                            }
                        }
                    }
                    if (!req_owner_approvals.empty()) {
                        auto req_owner_accs =_remote_database_api->get_accounts(std::vector<account_name_type>(
                            req_owner_approvals.begin(), req_owner_approvals.end()));
                        for (auto& acc : req_owner_accs) {
                            approving_account_lut[acc.name] = acc;
                            fetch_keys(acc.owner);
                            for (const auto& auth : acc.owner.account_auths) {
                                owner_account_auths.push_back(auth.first);
                            }
                        }
                    }
                    if (!req_posting_approvals.empty()) {
                        auto req_posting_accs =_remote_database_api->get_accounts(std::vector<account_name_type>(
                            req_posting_approvals.begin(), req_posting_approvals.end()));
                        for (auto& acc : req_posting_accs) {
                            approving_account_lut[acc.name] = acc;
                            fetch_keys(acc.posting);
                            for (const auto& auth : acc.posting.account_auths) {
                                posting_account_auths.push_back(auth.first);
                            }
                        }
                    }

                    if (!active_account_auths.empty()) {
                        auto active_account_auth_objs = _remote_database_api->get_accounts(active_account_auths);
                        for (auto& acc : active_account_auth_objs) {
                            approving_account_lut[acc.name] = acc;
                            fetch_keys(acc.active);
                        }
                    }
                    if (!owner_account_auths.empty()) {
                        auto owner_account_auth_objs = _remote_database_api->get_accounts(owner_account_auths);
                        for (auto& acc : owner_account_auth_objs) {
                            approving_account_lut[acc.name] = acc;
                            fetch_keys(acc.owner);
                        }
                    }
                    if (!posting_account_auths.empty()) {
                        auto posting_account_auth_objs = _remote_database_api->get_accounts(posting_account_auths);
                        for (auto& acc : posting_account_auth_objs) {
                            approving_account_lut[acc.name] = acc;
                            fetch_keys(acc.posting);
                        }
                    }

                    auto get_account_from_lut = [&]( const std::string& name ) -> const golos::api::account_api_object& {
                        auto it = approving_account_lut.find( name );
                        GOLOS_CHECK_LOGIC( it != approving_account_lut.end(),
                                logic_errors::no_account_in_lut,
                                "No account in lut: '${name}'", ("name",name) );
                        return it->second;
                    };

                    // get keys of each other auth into common set
                    for( const authority& a : other_auths ) {
                        for( const auto& k : a.key_auths ) {
                            wdump((k.first));
                            approving_key_set.insert( k.first );
                        }
                    }

                    auto dyn_props = _remote_database_api->get_dynamic_global_properties();
                    tx.set_reference_block( dyn_props.head_block_id );
                    tx.set_expiration( dyn_props.time + fc::seconds(_tx_expiration_seconds) );
                    tx.signatures.clear();

                    // checking each key from common set exists in wallet's keys
                    // and adding available ones to available set and map
                    //idump((_keys));
                    flat_set< public_key_type > available_keys;
                    flat_map< public_key_type, fc::ecc::private_key > available_private_keys;
                    for( const public_key_type& key : approving_key_set )
                    {
                        auto it = _keys.find(key);
                        if( it != _keys.end() )
                        {
                            fc::optional<fc::ecc::private_key> privkey = wif_to_key( it->second );
                            GOLOS_CHECK_LOGIC(privkey.valid(),
                                    logic_errors::malformed_private_key,
                                    "Malformed private key in _keys for public key ${key}",
                                    ("key",key));
                            available_keys.insert(key);
                            available_private_keys[key] = *privkey;
                        }
                    }

                    // removing excessive keys from available keys set
                    auto minimal_signing_keys = tx.minimize_required_signatures(
                            steem_chain_id,
                            available_keys,
                            [&]( const string& account_name ) -> const authority&
                            { return (get_account_from_lut( account_name ).active); },
                            [&]( const string& account_name ) -> const authority&
                            { return (get_account_from_lut( account_name ).owner); },
                            [&]( const string& account_name ) -> const authority&
                            { return (get_account_from_lut( account_name ).posting); },
                            STEEMIT_MAX_SIG_CHECK_DEPTH
                    );

                    // checking if each private key exists and signing tx with it
                    for( const public_key_type& k : minimal_signing_keys ) {
                        auto it = available_private_keys.find(k);
                        GOLOS_CHECK_LOGIC( it != available_private_keys.end(),
                                logic_errors::private_key_not_available,
                                "Private key for public key ${key} not available",
                                ("key",k));
                        tx.sign( it->second, steem_chain_id );
                    }

                    if( broadcast ) {
                        try {
                            auto result = _remote_network_broadcast_api->broadcast_transaction_synchronous( tx );
                            annotated_signed_transaction rtrx(tx);
                            rtrx.block_num = result.block_num;
                            rtrx.transaction_num = result.trx_num;
                            return rtrx;
                        } catch (const fc::exception& e) {
                            elog("Caught exception while broadcasting tx ${id}: ${e}", ("id", tx.id().str())("e", e.to_detail_string()) );
                            throw;
                        }
                    }
                    return tx;
                }

                std::map<string,std::function<string(fc::variant,const fc::variants&)>> get_result_formatters() const {
                    std::map<string,std::function<string(fc::variant,const fc::variants&)> > m;
                    m["help"] = [](variant result, const fc::variants& a) {
                        return result.get_string();
                    };

                    m["gethelp"] = [](variant result, const fc::variants& a) {
                        return result.get_string();
                    };

                    m["list_my_accounts"] = [](variant result, const fc::variants& a ) {
                        std::stringstream out;

                        auto accounts = result.as<vector<golos::api::account_api_object>>();
                        asset total_steem;
                        asset total_vest(0, VESTS_SYMBOL );
                        asset total_sbd(0, SBD_SYMBOL );
                        for( const auto& a : accounts ) {
                            total_steem += a.balance;
                            total_vest += a.vesting_shares;
                            total_sbd += a.sbd_balance;
                            out << std::left << std::setw( 17 ) << std::string(a.name)
                                << std::right << std::setw(18) << fc::variant(a.balance).as_string() <<" "
                                << std::right << std::setw(26) << fc::variant(a.vesting_shares).as_string() <<" "
                                << std::right << std::setw(16) << fc::variant(a.sbd_balance).as_string() <<"\n";
                        }
                        out << "-------------------------------------------------------------------------\n";
                        out << std::left << std::setw( 17 ) << "TOTAL"
                            << std::right << std::setw(18) << fc::variant(total_steem).as_string() <<" "
                            << std::right << std::setw(26) << fc::variant(total_vest).as_string() <<" "
                            << std::right << std::setw(16) << fc::variant(total_sbd).as_string() <<"\n";
                        return out.str();
                    };
                    auto acc_history_formatter = [](variant result, const fc::variants& a) {
                        std::stringstream ss;
                        ss << std::left << std::setw(5)  << "#" << " ";
                        ss << std::left << std::setw(10) << "BLOCK #" << " ";
                        ss << std::left << std::setw(15) << "TRX ID" << " ";
                        ss << std::left << std::setw(20) << "OPERATION" << " ";
                        ss << std::left << std::setw(50) << "DETAILS" << "\n";
                        ss << "-------------------------------------------------------------------------------\n";
                        const auto& results = result.get_array();
                        for (const auto& item : results) {
                            ss << std::left << std::setw(5) << item.get_array()[0].as_string() << " ";
                            const auto& op = item.get_array()[1].get_object();
                            ss << std::left << std::setw(10) << op["block"].as_string() << " ";
                            ss << std::left << std::setw(15) << op["trx_id"].as_string() << " ";
                            const auto& opop = op["op"].get_array();
                            ss << std::left << std::setw(20) << opop[0].as_string() << " ";
                            ss << std::left << std::setw(50) << fc::json::to_string(opop[1]) << "\n ";
                        }
                        return ss.str();
                    };
                    m["get_account_history"] = acc_history_formatter;
                    m["filter_account_history"] = acc_history_formatter;

                    /*m["get_open_orders"] = []( variant result, const fc::variants& a ) {
                        auto orders = result.as<vector<database_api::extended_limit_order>>();

                        std::stringstream ss;

                        ss << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;
                        ss << ' ' << setw( 10 ) << "Order #";
                        ss << ' ' << setw( 10 ) << "Price";
                        ss << ' ' << setw( 10 ) << "Quantity";
                        ss << ' ' << setw( 10 ) << "Type";
                        ss << "\n=====================================================================================================\n";
                        for( const auto& o : orders ) {
                            ss << ' ' << setw( 10 ) << o.orderid;
                            ss << ' ' << setw( 10 ) << o.real_price;
                            ss << ' ' << setw( 10 ) << fc::variant( asset( o.for_sale, o.sell_price.base.symbol ) ).as_string();
                            ss << ' ' << setw( 10 ) << (o.sell_price.base.symbol == STEEM_SYMBOL ? "SELL" : "BUY");
                            ss << "\n";
                        }
                        return ss.str();

                    };

                    m["get_order_book"] = []( variant result, const fc::variants& a ) {
                        auto orders = result.as< market_history::get_order_book_return >();
                        std::stringstream ss;
                        asset bid_sum = asset( 0, SBD_SYMBOL );
                        asset ask_sum = asset( 0, SBD_SYMBOL );
                        int spacing = 24;

                        ss << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;

                        ss << ' ' << setw( ( spacing * 4 ) + 6 ) << "Bids" << "Asks\n"
                           << ' '
                           << setw( spacing + 3 ) << "Sum(SBD)"
                           << setw( spacing + 1) << "SBD"
                           << setw( spacing + 1 ) << "STEEM"
                           << setw( spacing + 1 ) << "Price"
                           << setw( spacing + 1 ) << "Price"
                           << setw( spacing + 1 ) << "STEEM "
                           << setw( spacing + 1 ) << "SBD " << "Sum(SBD)"
                           << "\n====================================================================================================="
                           << "|=====================================================================================================\n";

                        for( size_t i = 0; i < orders.bids.size() || i < orders.asks.size(); i++ ) {
                            if ( i < orders.bids.size() ) {
                                bid_sum += asset( orders.bids[i].sbd, SBD_SYMBOL );
                                ss
                                        << ' ' << setw( spacing ) << bid_sum.to_string()
                                        << ' ' << setw( spacing ) << asset( orders.bids[i].sbd, SBD_SYMBOL ).to_string()
                                        << ' ' << setw( spacing ) << asset( orders.bids[i].steem, STEEM_SYMBOL ).to_string()
                                        << ' ' << setw( spacing ) << orders.bids[i].real_price;
                            } else {
                                ss << setw( (spacing * 4 ) + 5 ) << ' ';
                            }

                            ss << " |";

                            if ( i < orders.asks.size() ) {
                                ask_sum += asset( orders.asks[i].sbd, SBD_SYMBOL );
                                ss << ' ' << setw( spacing ) << orders.asks[i].real_price
                                   << ' ' << setw( spacing ) << asset( orders.asks[i].steem, STEEM_SYMBOL ).to_string()
                                   << ' ' << setw( spacing ) << asset( orders.asks[i].sbd, SBD_SYMBOL ).to_string()
                                   << ' ' << setw( spacing ) << ask_sum.to_string();
                            }

                            ss << endl;
                        }

                        ss << endl
                           << "Bid Total: " << bid_sum.to_string() << endl
                           << "Ask Total: " << ask_sum.to_string() << endl;

                        return ss.str();
                    };

                    m["get_withdraw_routes"] = []( variant result, const fc::variants& a )
                    {
                        auto routes = result.as< vector< database_api::withdraw_vesting_route_api_object > >();
                        std::stringstream ss;

                        ss << ' ' << std::left << std::setw( 20 ) << "From";
                        ss << ' ' << std::left << std::setw( 20 ) << "To";
                        ss << ' ' << std::right << std::setw( 8 ) << "Percent";
                        ss << ' ' << std::right << std::setw( 9 ) << "Auto-Vest";
                        ss << "\n==============================================================\n";

                        for( auto& r : routes )
                        {
                            ss << ' ' << std::left << std::setw( 20 ) << r.from_account;
                            ss << ' ' << std::left << std::setw( 20 ) << r.to_account ;
                            ss << ' ' << std::right << std::setw( 8 ) << std::setprecision( 2 ) << std::fixed << double( r.percent ) / 100;
                            ss << ' ' << std::right << std::setw( 9 ) << ( r.auto_vest ? "true" : "false" ) << std::endl;
                        }

                        return ss.str();
                    };


                     */
                    return m;
                }

                operation get_prototype_operation( string operation_name ) {
                    auto it = _prototype_ops.find( operation_name );
                    if( it == _prototype_ops.end() )
                        FC_THROW("Unsupported operation: \"${operation_name}\"", ("operation_name", operation_name));
                    return it->second;
                }

                message_body try_decrypt_private_message(const message_api_object& mo) const {
                    message_body result;

                    fc::sha512 shared_secret;

                    auto it = _keys.find(mo.from_memo_key);
                    auto pub_key = mo.to_memo_key;
                    if (it == _keys.end()) {
                        it = _keys.find(mo.to_memo_key);
                        pub_key = mo.from_memo_key;
                    }
                    if (it ==_keys.end()) {
                        wlog("unable to find keys");
                        return result;
                    }
                    auto priv_key = wif_to_key(it->second);
                    if (!priv_key) {
                        wlog("empty private key");
                        return result;
                    }
                    shared_secret = priv_key->get_shared_secret(pub_key);

                    fc::sha512::encoder enc;
                    fc::raw::pack(enc, mo.nonce);
                    fc::raw::pack(enc, shared_secret);
                    auto encrypt_key = enc.result();

                    uint32_t check = fc::sha256::hash(encrypt_key)._hash[0];

                    if (mo.checksum != check) {
                        wlog("wrong checksum");
                        return result;
                    }

                    auto decrypt_data = fc::aes_decrypt(encrypt_key, mo.encrypted_message);
                    auto msg_json = std::string(decrypt_data.begin(), decrypt_data.end());
                    try {
                        result = fc::json::from_string(msg_json).as<message_body>();
                    } catch (...) {
                        result.body = msg_json;
                    }
                    return result;
                }

                std::vector<extended_message_object> decrypt_private_messages(
                    std::vector<message_api_object> remote_result
                ) const {
                    std::vector<extended_message_object> result;
                    result.reserve(remote_result.size());
                    for (const auto& item : remote_result) {
                        result.emplace_back(item);
                        result.back().message = try_decrypt_private_message(item);
                    }
                    return result;
                }

                annotated_signed_transaction send_private_message(
                    const std::string& from, const std::string& to, const uint64_t nonce, const bool update,
                    const message_body& message, bool broadcast
                ) {
                    WALLET_CHECK_UNLOCKED();

                    auto from_account  = get_account(from);
                    auto to_account    = get_account(to);
                    auto shared_secret = get_private_key(from_account.memo_key).get_shared_secret(to_account.memo_key);

                    fc::sha512::encoder enc;
                    fc::raw::pack(enc, nonce);
                    fc::raw::pack(enc, shared_secret);
                    auto encrypt_key = enc.result();

                    auto msg_json = fc::json::to_string(message);
                    auto msg_data = std::vector<char>(msg_json.begin(), msg_json.end());

                    private_message_operation op;

                    op.from              = from;
                    op.from_memo_key     = from_account.memo_key;
                    op.to                = to;
                    op.to_memo_key       = to_account.memo_key;
                    op.nonce             = nonce;
                    op.update            = update;
                    op.encrypted_message = fc::aes_encrypt(encrypt_key, msg_data);
                    op.checksum          = fc::sha256::hash(encrypt_key)._hash[0];

                    private_message_plugin_operation pop = op;

                    custom_json_operation jop;
                    jop.id   = "private_message";
                    jop.json = fc::json::to_string(pop);
                    jop.required_posting_auths.insert(from);

                    signed_transaction trx;
                    trx.operations.push_back(jop);
                    trx.validate();

                    return sign_transaction(trx, broadcast);
                }

                annotated_signed_transaction delete_private_message(
                    const std::string& requester,
                    const std::string& from, const std::string& to, const uint64_t nonce, bool broadcast
                ) {
                    WALLET_CHECK_UNLOCKED();
                    GOLOS_CHECK_PARAM(nonce, GOLOS_CHECK_VALUE(nonce != 0, "You should specify nonce of deleted message"));

                    private_delete_message_operation op;
                    op.requester = requester;
                    op.from = from;
                    op.to = to;
                    op.nonce = nonce;
                    op.start_date = time_point_sec::min();
                    op.stop_date = time_point_sec::min();

                    private_message_plugin_operation pop = op;

                    custom_json_operation jop;
                    jop.id   = "private_message";
                    jop.json = fc::json::to_string(pop);
                    jop.required_posting_auths.insert(requester);

                    signed_transaction trx;
                    trx.operations.push_back(jop);
                    trx.validate();

                    return sign_transaction(trx, broadcast);
                }

                annotated_signed_transaction delete_private_messages(
                    const std::string& requester,
                    const std::string& from, const std::string& to,
                    const std::string& start_date, const std::string& stop_date,
                    bool broadcast
                ) {
                    WALLET_CHECK_UNLOCKED();

                    private_delete_message_operation op;
                    op.requester = requester;
                    op.from = from;
                    op.to = to;
                    op.nonce = 0;
                    op.start_date = time_converter(start_date, time_point::now(), time_point_sec::min()).time();
                    op.stop_date = time_converter(stop_date, time_point::now(), time_point::now()).time();

                    private_message_plugin_operation pop = op;

                    custom_json_operation jop;
                    jop.id   = "private_message";
                    jop.json = fc::json::to_string(pop);
                    jop.required_posting_auths.insert(requester);

                    signed_transaction trx;
                    trx.operations.push_back(jop);
                    trx.validate();

                    return sign_transaction(trx, broadcast);
                }

                string                                  _wallet_filename;
                wallet_data                             _wallet;
                golos::protocol::chain_id_type          steem_chain_id;

                map<public_key_type,string>             _keys;
                fc::sha512                              _checksum;
                fc::api< remote_database_api >          _remote_database_api;
                fc::api< remote_operation_history >     _remote_operation_history;
                fc::api< remote_account_history >       _remote_account_history;
                fc::api< remote_social_network >        _remote_social_network;
                fc::api< remote_network_broadcast_api>  _remote_network_broadcast_api;
                fc::api< remote_follow >                _remote_follow;
                fc::api< remote_market_history >        _remote_market_history;
                fc::api< remote_private_message >       _remote_private_message;
                fc::api< remote_account_by_key >        _remote_account_by_key;
                fc::api< remote_witness_api >           _remote_witness_api;
                uint32_t                                _tx_expiration_seconds = 30;

                flat_map<string, operation>             _prototype_ops;

                static_variant_map _operation_which_map = create_static_variant_map< operation >();

#ifdef __unix__
                mode_t                  _old_umask;
#endif
                const string _wallet_filename_extension = ".wallet";
            };



        } } } // golos::wallet::detail



namespace golos { namespace wallet {

        wallet_api::wallet_api(const wallet_data& initial_data, const golos::protocol::chain_id_type& _steem_chain_id, fc::api_connection& con)
                : my(new detail::wallet_api_impl(*this, initial_data, _steem_chain_id, con))
        {}

        wallet_api::~wallet_api(){}

        bool wallet_api::copy_wallet_file(string destination_filename)
        {
            return my->copy_wallet_file(destination_filename);
        }

        optional<golos::api::annotated_signed_block> wallet_api::get_block(uint32_t num) {
            return my->_remote_database_api->get_block( num );
        }

        vector< golos::plugins::operation_history::applied_operation > wallet_api::get_ops_in_block(uint32_t block_num, bool only_virtual) {
            return my->_remote_operation_history->get_ops_in_block( block_num, only_virtual );
        }

        vector< golos::api::account_api_object > wallet_api::list_my_accounts() {
            WALLET_CHECK_UNLOCKED();
            vector<golos::api::account_api_object> result;

            vector<public_key_type> pub_keys;
            pub_keys.reserve( my->_keys.size() );

            for( const auto& item : my->_keys )
                pub_keys.push_back(item.first);

            auto refs = my->_remote_account_by_key->get_key_references( pub_keys );
            set<string> names;
            for( const auto& item : refs )
                for( const auto& name : item )
                    names.insert( name );


            result.reserve( names.size() );
            for( const auto& name : names )
                result.emplace_back( get_account( name ) );

            return result;
        }

        vector< account_name_type > wallet_api::list_accounts(const string& lowerbound, uint32_t limit) {
            return my->_remote_database_api->lookup_accounts( lowerbound, limit );
        }

        vector< account_name_type > wallet_api::get_active_witnesses()const {
            return my->_remote_witness_api->get_active_witnesses();
        }

        vector<account_name_type> wallet_api::get_miner_queue()const {
            return my->_remote_witness_api->get_miner_queue();
        }

        brain_key_info wallet_api::suggest_brain_key()const {
            brain_key_info result;
            // create a private key for secure entropy
            fc::sha256 sha_entropy1 = fc::ecc::private_key::generate().get_secret();
            fc::sha256 sha_entropy2 = fc::ecc::private_key::generate().get_secret();
            fc::bigint entropy1( sha_entropy1.data(), sha_entropy1.data_size() );
            fc::bigint entropy2( sha_entropy2.data(), sha_entropy2.data_size() );
            fc::bigint entropy(entropy1);
            entropy <<= 8*sha_entropy1.data_size();
            entropy += entropy2;
            string brain_key = "";

            for( int i=0; i<BRAIN_KEY_WORD_COUNT; i++ ) {
                fc::bigint choice = entropy % golos::words::word_list_size;
                entropy /= golos::words::word_list_size;
                if( i > 0 )
                    brain_key += " ";
                brain_key += golos::words::word_list[ choice.to_int64() ];
            }

            brain_key = normalize_brain_key(brain_key);
            fc::ecc::private_key priv_key = detail::derive_private_key( brain_key, 0 );
            result.brain_priv_key = brain_key;
            result.wif_priv_key = key_to_wif( priv_key );
            result.pub_key = priv_key.get_public_key();
            return result;
        }

        string wallet_api::serialize_transaction( signed_transaction tx )const {
            return fc::to_hex(fc::raw::pack(tx));
        }

        string wallet_api::get_wallet_filename() const {
            return my->get_wallet_filename();
        }


        golos::api::account_api_object wallet_api::get_account( string account_name ) const {
            return my->get_account( account_name );
        }

        bool wallet_api::import_key(string wif_key)
        {
            WALLET_CHECK_UNLOCKED();
            // backup wallet
            fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
            if (!optional_private_key)
                FC_THROW("Invalid private key");
//   string shorthash = detail::pubkey_to_shorthash( optional_private_key->get_public_key() );
//   copy_wallet_file( "before-import-key-" + shorthash );

            if( my->import_key(wif_key) )
            {
                save_wallet_file();
                //     copy_wallet_file( "after-import-key-" + shorthash );
                return true;
            }
            return false;
        }

        string wallet_api::normalize_brain_key(string s) const
        {
            return detail::normalize_brain_key( s );
        }

        variant wallet_api::info() const
        {
            return my->info();
        }

        variant_object wallet_api::database_info() const
        {
            return my->database_info();
        }

        variant_object wallet_api::about() const
        {
            return my->about();
        }

/*
fc::ecc::private_key wallet_api::derive_private_key(const std::string& prefix_string, int sequence_number) const
{
   return detail::derive_private_key( prefix_string, sequence_number );
}
*/

        vector< account_name_type > wallet_api::list_witnesses(const string& lowerbound, uint32_t limit)
        {
            return my->_remote_witness_api->lookup_witness_accounts( lowerbound, limit );
        }

        optional< witness_api::witness_api_object > wallet_api::get_witness(string owner_account)
        {
            return my->get_witness(owner_account);
        }

        annotated_signed_transaction wallet_api::set_voting_proxy(string account_to_modify, string voting_account, bool broadcast /* = false */)
        { return my->set_voting_proxy(account_to_modify, voting_account, broadcast); }

        void wallet_api::set_wallet_filename(string wallet_filename) { my->_wallet_filename = wallet_filename; }

        annotated_signed_transaction wallet_api::sign_transaction(signed_transaction tx, bool broadcast /* = false */)
        { try {
                return my->sign_transaction( tx, broadcast);
            } FC_CAPTURE_AND_RETHROW( (tx) ) }

        operation wallet_api::get_prototype_operation(string operation_name) {
            return my->get_prototype_operation( operation_name );
        }

        string wallet_api::help()const
        {
            std::vector<std::string> method_names = my->method_documentation.get_method_names();
            std::stringstream ss;
            for (const std::string method_name : method_names)
            {
                try
                {
                    ss << my->method_documentation.get_brief_description(method_name);
                }
                catch (const fc::key_not_found_exception&)
                {
                    ss << method_name << " (no help available)\n";
                }
            }
            return ss.str();
        }

        string wallet_api::gethelp(const string& method)const
        {
            fc::api<wallet_api> tmp;
            std::stringstream ss;
            ss << "\n";

            std::string doxygenHelpString = my->method_documentation.get_detailed_description(method);
            if (!doxygenHelpString.empty())
                ss << doxygenHelpString;
            else
                ss << "No help defined for method " << method << "\n";

            return ss.str();
        }

        bool wallet_api::load_wallet_file( string wallet_filename )
        {
            return my->load_wallet_file( wallet_filename );
        }

        void wallet_api::save_wallet_file( string wallet_filename )
        {
            my->save_wallet_file( wallet_filename );
        }

        std::map<string,std::function<string(fc::variant,const fc::variants&)> >
        wallet_api::get_result_formatters() const
        {
            return my->get_result_formatters();
        }

        bool wallet_api::is_locked()const
        {
            return my->is_locked();
        }
        bool wallet_api::is_new()const
        {
            return my->_wallet.cipher_keys.size() == 0;
        }

        void wallet_api::encrypt_keys()
        {
            my->encrypt_keys();
        }

        void wallet_api::quit() {
            my->self.quit_command();
        }

        transaction_handle_type wallet_api::begin_builder_transaction() {
            return my->begin_builder_transaction();
        }

        void wallet_api::add_operation_to_builder_transaction(transaction_handle_type handle, const operation& op) {
            my->add_operation_to_builder_transaction(handle, op);
        }
        void wallet_api::add_operation_copy_to_builder_transaction(
            transaction_handle_type src_handle,
            transaction_handle_type dst_handle,
            uint32_t op_index
        ) {
            my->add_operation_copy_to_builder_transaction(src_handle, dst_handle, op_index);
        }

        void wallet_api::replace_operation_in_builder_transaction(
            transaction_handle_type handle, unsigned op_index, const operation& new_op
        ) {
            my->replace_operation_in_builder_transaction(handle, op_index, new_op);
        }

        transaction wallet_api::preview_builder_transaction(transaction_handle_type handle) {
            return my->preview_builder_transaction(handle);
        }

        signed_transaction wallet_api::sign_builder_transaction(transaction_handle_type handle, bool broadcast) {
            return my->sign_builder_transaction(handle, broadcast);
        }

        signed_transaction wallet_api::propose_builder_transaction(
            transaction_handle_type handle,
            std::string author,
            std::string title,
            std::string memo,
            std::string expiration,
            std::string review,
            bool broadcast
        ) {
            return my->propose_builder_transaction(handle, author, title, memo, expiration, review, broadcast);
        }

        void wallet_api::remove_builder_transaction(transaction_handle_type handle) {
            return my->remove_builder_transaction(handle);
        }

        signed_transaction wallet_api::approve_proposal(
            const std::string& author,
            const std::string& title,
            const approval_delta& delta,
            bool broadcast
        ) {
            return my->approve_proposal(author, title, delta, broadcast);
        }

        std::vector<database_api::proposal_api_object> wallet_api::get_proposed_transactions(
            std::string account, uint32_t from, uint32_t limit
        ) {
            return my->get_proposed_transactions(account, from, limit);
        }

        void wallet_api::lock() {
            try {
                WALLET_CHECK_UNLOCKED();
                encrypt_keys();
                for( auto& key : my->_keys )
                    key.second = key_to_wif(fc::ecc::private_key());
                my->_keys.clear();
                my->_checksum = fc::sha512();
                my->self.lock_changed(true);
            } FC_CAPTURE_AND_RETHROW() }

        void wallet_api::unlock(string password) {
            try {
                GOLOS_CHECK_PARAM(password, GOLOS_CHECK_VALUE(password.size() > 0, "Password should be non empty"));
                auto pw = fc::sha512::hash(password.c_str(), password.size());
                vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
                auto pk = fc::raw::unpack<plain_keys>(decrypted);
                my->_keys = std::move(pk.keys);
                my->_checksum = pk.checksum;
                my->self.lock_changed(false);
            } catch (const fc::aes_exception& e) {
                GOLOS_CHECK_PARAM(password, FC_THROW_EXCEPTION(golos::invalid_value, "Invalid password"));
            } FC_CAPTURE_AND_RETHROW() }

        void wallet_api::set_password( string password )
        {
            if( !is_new() )
                WALLET_CHECK_UNLOCKED();
            my->_checksum = fc::sha512::hash( password.c_str(), password.size() );
            lock();
        }

        vector<key_with_data> wallet_api::list_keys(string account)
        {
            WALLET_CHECK_UNLOCKED();

            vector<key_with_data> all_keys;

            vector<golos::api::account_api_object> accounts;
            if (account.empty()) {
                accounts = list_my_accounts();
            } else {
                accounts.push_back(get_account(account));
            }

            for (auto it = accounts.begin(); it != accounts.end(); it++) {
                key_with_data memo_key_data;
                memo_key_data.public_key = std::string(it->memo_key);
                memo_key_data.private_key = get_private_key(it->memo_key);
                memo_key_data.account = std::string(it->name);
                memo_key_data.type = "memo_key";
                all_keys.push_back(memo_key_data);

                auto acc_owner_keys = it->owner.get_keys();
                for (auto it2 = acc_owner_keys.begin(); it2 != acc_owner_keys.end(); it2++) {
                    key_with_data key_data;
                    key_data.public_key = std::string(*it2);
                    key_data.private_key = get_private_key(*it2);
                    key_data.account = std::string(it->name);
                    key_data.type = "owner";
                    all_keys.push_back(key_data);
                }

                auto acc_active_keys = it->active.get_keys();
                for (auto it2 = acc_active_keys.begin(); it2 != acc_active_keys.end(); it2++) {
                    key_with_data key_data;
                    key_data.public_key = std::string(*it2);
                    key_data.private_key = get_private_key(*it2);
                    key_data.account = std::string(it->name);
                    key_data.type = "active";
                    all_keys.push_back(key_data);
                }

                auto acc_posting_keys = it->posting.get_keys();
                for (auto it2 = acc_posting_keys.begin(); it2 != acc_posting_keys.end(); it2++) {
                    key_with_data key_data;
                    key_data.public_key = std::string(*it2);
                    key_data.private_key = get_private_key(*it2);
                    key_data.account = std::string(it->name);
                    key_data.type = "posting";
                    all_keys.push_back(key_data);
                }
            }

            return all_keys;
        }

        string wallet_api::get_private_key( public_key_type pubkey )const
        {
            return key_to_wif( my->get_private_key( pubkey ) );
        }

        pair<public_key_type,string> wallet_api::get_private_key_from_password( string account, string role, string password )const {
            auto seed = account + role + password;
            GOLOS_CHECK_PARAM(account, GOLOS_CHECK_VALUE(seed.size(), "At least one of 'account', 'role', 'password' should be non empty"));
            auto secret = fc::sha256::hash( seed.c_str(), seed.size() );
            auto priv = fc::ecc::private_key::regenerate( secret );
            return std::make_pair( public_key_type( priv.get_public_key() ), key_to_wif( priv ) );
        }

        witness_api::feed_history_api_object wallet_api::get_feed_history()const {
            return my->_remote_witness_api->get_feed_history();
        }

/**
 * This method is used by faucets to create new accounts for other users which must
 * provide their desired keys. The resulting account may not be controllable by this
 * wallet.
 */
        annotated_signed_transaction wallet_api::create_account_with_keys(
            string creator,
            string new_account_name,
            string json_meta,
            asset fee,
            public_key_type owner,
            public_key_type active,
            public_key_type posting,
            public_key_type memo,
            bool broadcast
        ) const {
            try {
                WALLET_CHECK_UNLOCKED();
                account_create_operation op;
                op.creator = creator;
                op.new_account_name = new_account_name;
                op.owner = authority( 1, owner, 1 );
                op.active = authority( 1, active, 1 );
                op.posting = authority( 1, posting, 1 );
                op.memo_key = memo;
                op.json_metadata = json_meta;
                op.fee = fee;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();

                return my->sign_transaction( tx, broadcast );
            } FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta)(owner)(active)(posting)(memo)(broadcast))
        }


        /**
         *  This method will generate new owner, active, posting and memo keys for the new account
         *  which will be controlable by this wallet.
         */
        annotated_signed_transaction wallet_api::create_account_delegated(
            string creator, asset steem_fee, asset delegated_vests, string new_account_name,
            string json_meta, bool broadcast
        ) {
            try {
                WALLET_CHECK_UNLOCKED();
                auto owner = suggest_brain_key();
                auto active = suggest_brain_key();
                auto posting = suggest_brain_key();
                auto memo = suggest_brain_key();
                import_key(owner.wif_priv_key);
                import_key(active.wif_priv_key);
                import_key(posting.wif_priv_key);
                import_key(memo.wif_priv_key);
                return create_account_with_keys_delegated(
                    creator, steem_fee, delegated_vests, new_account_name, json_meta,
                    owner.pub_key, active.pub_key, posting.pub_key, memo.pub_key, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta));
        }

        /**
         * This method is used by faucets to create new accounts for other users which must
         * provide their desired keys. The resulting account may not be controllable by this
         * wallet.
         */
        annotated_signed_transaction wallet_api::create_account_with_keys_delegated(
            string creator,
            asset steem_fee,
            asset delegated_vests,
            string new_account_name,
            string json_meta,
            public_key_type owner,
            public_key_type active,
            public_key_type posting,
            public_key_type memo,
            bool broadcast
        ) const {
            try {
                WALLET_CHECK_UNLOCKED();
                account_create_with_delegation_operation op;
                op.creator = creator;
                op.new_account_name = new_account_name;
                op.owner = authority(1, owner, 1);
                op.active = authority(1, active, 1);
                op.posting = authority(1, posting, 1);
                op.memo_key = memo;
                op.json_metadata = json_meta;
                op.fee = steem_fee;
                op.delegation = delegated_vests;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();
                return my->sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta)(owner)(active)(posting)(memo)(broadcast));
        }

        /**
         *  This method will generate new owner, active, posting and memo keys for the new account
         *  which will be controlable by this wallet. Also it will add the referral duty to the new account.
         */
        annotated_signed_transaction wallet_api::create_account_referral(
            string creator, asset steem_fee, asset delegated_vests, string new_account_name,
            string json_meta, account_referral_options referral_options, bool broadcast
        ) {
            try {
                WALLET_CHECK_UNLOCKED();
                auto owner = suggest_brain_key();
                auto active = suggest_brain_key();
                auto posting = suggest_brain_key();
                auto memo = suggest_brain_key();
                import_key(owner.wif_priv_key);
                import_key(active.wif_priv_key);
                import_key(posting.wif_priv_key);
                import_key(memo.wif_priv_key);

                account_create_with_delegation_operation op;
                op.creator = creator;
                op.new_account_name = new_account_name;
                op.owner = authority(1, owner.pub_key, 1);
                op.active = authority(1, active.pub_key, 1);
                op.posting = authority(1, posting.pub_key, 1);
                op.memo_key = memo.pub_key;
                op.json_metadata = json_meta;
                op.fee = steem_fee;
                op.delegation = delegated_vests;

                op.extensions.insert(referral_options);

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();
                return my->sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta));
        }

        /**
         *  This method pays the break fee to remove the referral duty from an account.
         */
        annotated_signed_transaction wallet_api::break_free_referral(string referral, bool broadcast) {
            try {
                WALLET_CHECK_UNLOCKED();

                break_free_referral_operation op;
                op.referral = referral;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();
                return my->sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((referral));
        }

/**
 * This method is used by faucets to create new accounts for other users which must
 * provide their desired keys. The resulting account may not be controllable by this
 * wallet.
 */

        annotated_signed_transaction wallet_api::request_account_recovery( string recovery_account, string account_to_recover, authority new_authority, bool broadcast ) {
            WALLET_CHECK_UNLOCKED();
            request_account_recovery_operation op;
            op.recovery_account = recovery_account;
            op.account_to_recover = account_to_recover;
            op.new_owner_authority = new_authority;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::recover_account( string account_to_recover, authority recent_authority, authority new_authority, bool broadcast ) {
            WALLET_CHECK_UNLOCKED();

            recover_account_operation op;
            op.account_to_recover = account_to_recover;
            op.new_owner_authority = new_authority;
            op.recent_owner_authority = recent_authority;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::change_recovery_account( string owner, string new_recovery_account, bool broadcast ) {
            WALLET_CHECK_UNLOCKED();

            change_recovery_account_operation op;
            op.account_to_recover = owner;
            op.new_recovery_account = new_recovery_account;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        vector< database_api::owner_authority_history_api_object > wallet_api::get_owner_history( string account )const {
            return my->_remote_database_api->get_owner_history( account );
        }

        annotated_signed_transaction wallet_api::update_account(
                string account_name,
                string json_meta,
                public_key_type owner,
                public_key_type active,
                public_key_type posting,
                public_key_type memo,
                bool broadcast )const
        {
            try
            {
                WALLET_CHECK_UNLOCKED();

                account_update_operation op;
                op.account = account_name;
                op.owner = authority( 1, owner, 1 );
                op.active = authority( 1, active, 1);
                op.posting = authority( 1, posting, 1);
                op.memo_key = memo;
                op.json_metadata = json_meta;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();

                return my->sign_transaction( tx, broadcast );
            }
            FC_CAPTURE_AND_RETHROW( (account_name)(json_meta)(owner)(active)(memo)(broadcast) )
        }

        annotated_signed_transaction wallet_api::update_account_auth_key( string account_name, authority_type type, public_key_type key, weight_type weight, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();

            auto account = get_account(account_name);

            account_update_operation op;
            op.account = account_name;
            op.memo_key = account.memo_key;
            op.json_metadata = account.json_metadata;

            authority new_auth;

            switch( type )
            {
                case( owner ):
                    new_auth = account.owner;
                    break;
                case( active ):
                    new_auth = account.active;
                    break;
                case( posting ):
                    new_auth = account.posting;
                    break;
            }

            if( weight == 0 ) // Remove the key
            {
                new_auth.key_auths.erase( key );
            }
            else
            {
                new_auth.add_authority( key, weight );
            }

            if( new_auth.is_impossible() ) {
                GOLOS_CHECK_LOGIC(type != owner, logic_errors::owner_authority_change_would_render_account_irrecoverable,
                        "Owner authority change would render account irrecoverable." );

                wlog( "Authority is now impossible." );
            }

            switch( type ) {
                case( owner ):
                    op.owner = new_auth;
                    break;
                case( active ):
                    op.active = new_auth;
                    break;
                case( posting ):
                    op.posting = new_auth;
                    break;
            }

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::update_account_auth_account( string account_name, authority_type type, string auth_account, weight_type weight, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();

            auto account = get_account(account_name);

            account_update_operation op;
            op.account = account_name;
            op.memo_key = account.memo_key;
            op.json_metadata = account.json_metadata;

            authority new_auth;

            switch( type )
            {
                case( owner ):
                    new_auth = account.owner;
                    break;
                case( active ):
                    new_auth = account.active;
                    break;
                case( posting ):
                    new_auth = account.posting;
                    break;
            }

            if( weight == 0 ) // Remove the key
            {
                new_auth.account_auths.erase( auth_account );
            }
            else
            {
                new_auth.add_authority( auth_account, weight );
            }

            if( new_auth.is_impossible() )
            {
                GOLOS_CHECK_LOGIC(type != owner, logic_errors::owner_authority_change_would_render_account_irrecoverable,
                        "Owner authority change would render account irrecoverable." );

                wlog( "Authority is now impossible." );
            }

            switch( type )
            {
                case( owner ):
                    op.owner = new_auth;
                    break;
                case( active ):
                    op.active = new_auth;
                    break;
                case( posting ):
                    op.posting = new_auth;
                    break;
            }

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::update_account_auth_threshold( string account_name, authority_type type, uint32_t threshold, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();

            GOLOS_CHECK_PARAM(threshold, GOLOS_CHECK_VALUE(threshold != 0, "Authority is implicitly satisfied"));

            auto account = get_account(account_name);
            account_update_operation op;
            op.account = account_name;
            op.memo_key = account.memo_key;
            op.json_metadata = account.json_metadata;

            authority new_auth;

            switch( type )
            {
                case( owner ):
                    new_auth = account.owner;
                    break;
                case( active ):
                    new_auth = account.active;
                    break;
                case( posting ):
                    new_auth = account.posting;
                    break;
            }

            new_auth.weight_threshold = threshold;

            if( new_auth.is_impossible() )
            {
                GOLOS_CHECK_LOGIC(type != owner, logic_errors::owner_authority_change_would_render_account_irrecoverable,
                        "Owner authority change would render account irrecoverable." );

                wlog( "Authority is now impossible." );
            }

            switch( type )
            {
                case( owner ):
                    op.owner = new_auth;
                    break;
                case( active ):
                    op.active = new_auth;
                    break;
                case( posting ):
                    op.posting = new_auth;
                    break;
            }

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::update_account_meta(string account_name, string json_meta, bool broadcast) {
            WALLET_CHECK_UNLOCKED();

            auto account = get_account(account_name);

            signed_transaction tx;
            auto hf = my->_remote_database_api->get_hardfork_version();
            if (hf < hardfork_version(0, STEEMIT_HARDFORK_0_18)) {
                // TODO: remove this branch after HF 0.18
                account_update_operation op;
                op.account = account_name;
                op.memo_key = account.memo_key;
                op.json_metadata = json_meta;
                tx.operations.push_back(op);
            } else {
                account_metadata_operation op;
                op.account = account_name;
                op.json_metadata = json_meta;
                tx.operations.push_back(op);
            }
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::update_account_memo_key( string account_name, public_key_type key, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();

            auto account = get_account(account_name);

            account_update_operation op;
            op.account = account_name;
            op.memo_key = key;
            op.json_metadata = account.json_metadata;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::delegate_vesting_shares(string delegator, string delegatee, asset vesting_shares, bool broadcast) {
            WALLET_CHECK_UNLOCKED();

            delegate_vesting_shares_operation op;
            op.delegator = delegator;
            op.delegatee = delegatee;
            op.vesting_shares = vesting_shares;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::delegate_vesting_shares_with_interest(string delegator, string delegatee, asset vesting_shares, uint16_t interest_rate, bool broadcast) {
            WALLET_CHECK_UNLOCKED();

            delegate_vesting_shares_with_interest_operation op;
            op.delegator = delegator;
            op.delegatee = delegatee;
            op.vesting_shares = vesting_shares;
            op.interest_rate = interest_rate;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

/**
 *  This method will generate new owner, active, posting and memo keys for the new account
 *  which will be controlable by this wallet.
 */
        annotated_signed_transaction wallet_api::create_account(
            string creator, string new_account_name, string json_meta, asset fee, bool broadcast
        )
        { try {
                WALLET_CHECK_UNLOCKED();
                auto owner = suggest_brain_key();
                auto active = suggest_brain_key();
                auto posting = suggest_brain_key();
                auto memo = suggest_brain_key();
                import_key( owner.wif_priv_key );
                import_key( active.wif_priv_key );
                import_key( posting.wif_priv_key );
                import_key( memo.wif_priv_key );
                if (!fee.amount.value) {
                    auto prop = my->_remote_database_api->get_chain_properties();
                    auto hf = my->_remote_database_api->get_hardfork_version();
                    fee = prop.account_creation_fee;
                }
                return create_account_with_keys(
                    creator, new_account_name, json_meta, fee,
                    owner.pub_key, active.pub_key, posting.pub_key, memo.pub_key, broadcast
                );
            } FC_CAPTURE_AND_RETHROW( (creator)(new_account_name)(json_meta) ) }

/**
 *  This method will generate new owner, active, and memo keys for the new account which
 *  will be controlable by this wallet.
 */

        annotated_signed_transaction wallet_api::update_witness(
            string witness_account_name,
            string url,
            public_key_type block_signing_key,
            bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            signed_transaction tx;
            witness_update_operation op;
            if (url.empty()) {
                auto wit = my->_remote_witness_api->get_witness_by_account(witness_account_name);
                if (wit.valid()) {
                    FC_ASSERT(wit->owner == witness_account_name);
                    url = wit->url;
                }
            }
            op.url = url;
            op.owner = witness_account_name;
            op.block_signing_key = block_signing_key;

            tx.operations.push_back(op);

            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::update_chain_properties(
            string witness_account_name,
            const optional_chain_props& props,
            bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            signed_transaction tx;
            chain_properties_update_operation op;
            chain_api_properties ap;
            chain_properties_19 p;

            // copy defaults in case of missing witness object
            ap.account_creation_fee = p.account_creation_fee;
            ap.maximum_block_size = p.maximum_block_size;
            ap.sbd_interest_rate = p.sbd_interest_rate;

            auto wit = my->_remote_witness_api->get_witness_by_account(witness_account_name);
            if (wit.valid()) {
                FC_ASSERT(wit->owner == witness_account_name);
                ap = wit->props;
            }
#define SET_PROP(cp, X) {cp.X = !!props.X ? *(props.X) : ap.X;}
            SET_PROP(p, account_creation_fee);
            SET_PROP(p, maximum_block_size);
            SET_PROP(p, sbd_interest_rate);
#undef SET_PROP
#define SET_PROP(cp, X) {if (!!props.X) cp.X = *(props.X); else if (!!ap.X) cp.X = *(ap.X);}
            SET_PROP(p, create_account_min_golos_fee);
            SET_PROP(p, create_account_min_delegation);
            SET_PROP(p, create_account_delegation_time);
            SET_PROP(p, min_delegation);
            SET_PROP(p, max_referral_interest_rate);
            SET_PROP(p, max_referral_term_sec);
            SET_PROP(p, min_referral_break_fee);
            SET_PROP(p, max_referral_break_fee);
            SET_PROP(p, posts_window);
            SET_PROP(p, posts_per_window);
            SET_PROP(p, comments_window);
            SET_PROP(p, comments_per_window);
            SET_PROP(p, votes_window);
            SET_PROP(p, votes_per_window);
            SET_PROP(p, auction_window_size);
            SET_PROP(p, max_delegated_vesting_interest_rate);
            SET_PROP(p, custom_ops_bandwidth_multiplier);
            SET_PROP(p, min_curation_percent);
            SET_PROP(p, max_curation_percent);
            SET_PROP(p, curation_reward_curve);
            SET_PROP(p, allow_distribute_auction_reward);
            SET_PROP(p, allow_return_auction_reward_to_fund);
            op.props = p;
            auto hf = my->_remote_database_api->get_hardfork_version();
            if (hf >= hardfork_version(0, STEEMIT_HARDFORK_0_21) || !!props.worker_from_content_fund_percent
                    || !!props.worker_from_vesting_fund_percent || !!props.worker_from_witness_fund_percent
                    || !!props.worker_techspec_approve_term_sec || !!props.worker_result_approve_term_sec) {
                chain_properties_21 p21;
                p21 = p;
                SET_PROP(p21, worker_from_content_fund_percent);
                SET_PROP(p21, worker_from_vesting_fund_percent);
                SET_PROP(p21, worker_from_witness_fund_percent);
                SET_PROP(p21, worker_techspec_approve_term_sec);
                SET_PROP(p21, worker_result_approve_term_sec);
                op.props = p21;
            }
#undef SET_PROP

            op.owner = witness_account_name;
            tx.operations.push_back(op);

            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::vote_for_witness(string voting_account, string witness_to_vote_for, bool approve, bool broadcast )
        { try {
                WALLET_CHECK_UNLOCKED();
                account_witness_vote_operation op;
                op.account = voting_account;
                op.witness = witness_to_vote_for;
                op.approve = approve;

                signed_transaction tx;
                tx.operations.push_back( op );
                tx.validate();

                return my->sign_transaction( tx, broadcast );
            } FC_CAPTURE_AND_RETHROW( (voting_account)(witness_to_vote_for)(approve)(broadcast) ) }

        void wallet_api::check_memo( const string& memo, const golos::api::account_api_object& account )const
        {
            vector< public_key_type > keys;

            try
            {
                // Check if memo is a private key
                keys.push_back( fc::ecc::extended_private_key::from_base58( memo ).get_public_key() );
            }
            catch( fc::parse_error_exception& ) {}
            catch( fc::assert_exception& ) {}

            // Get possible keys if memo was an account password
            string owner_seed = account.name + "owner" + memo;
            auto owner_secret = fc::sha256::hash( owner_seed.c_str(), owner_seed.size() );
            keys.push_back( fc::ecc::private_key::regenerate( owner_secret ).get_public_key() );

            string active_seed = account.name + "active" + memo;
            auto active_secret = fc::sha256::hash( active_seed.c_str(), active_seed.size() );
            keys.push_back( fc::ecc::private_key::regenerate( active_secret ).get_public_key() );

            string posting_seed = account.name + "posting" + memo;
            auto posting_secret = fc::sha256::hash( posting_seed.c_str(), posting_seed.size() );
            keys.push_back( fc::ecc::private_key::regenerate( posting_secret ).get_public_key() );

            // Check keys against public keys in authorites
            for( auto& key_weight_pair : account.owner.key_auths )
            {
                for( auto& key : keys )
                    GOLOS_CHECK_LOGIC(key_weight_pair.first != key,
                            logic_errors::detected_private_key_in_memo,
                            "Detected ${type} private key in memo field",
                            ("type","owner"));
            }

            for( auto& key_weight_pair : account.active.key_auths )
            {
                for( auto& key : keys )
                    GOLOS_CHECK_LOGIC(key_weight_pair.first != key,
                            logic_errors::detected_private_key_in_memo,
                            "Detected ${type} private key in memo field",
                            ("type","active"));
            }

            for( auto& key_weight_pair : account.posting.key_auths )
            {
                for( auto& key : keys )
                    GOLOS_CHECK_LOGIC(key_weight_pair.first != key,
                            logic_errors::detected_private_key_in_memo,
                            "Detected ${type} private key in memo field",
                            ("type","posting"));
            }

            const auto& memo_key = account.memo_key;
            for( auto& key : keys )
                GOLOS_CHECK_LOGIC(memo_key != key,
                        logic_errors::detected_private_key_in_memo,
                        "Detected ${type} private key in memo field",
                        ("type","memo"));

            // Check against imported keys
            for( auto& key_pair : my->_keys )
            {
                for( auto& key : keys )
                    GOLOS_CHECK_LOGIC(key_pair.first != key,
                            logic_errors::detected_private_key_in_memo,
                            "Detected ${type} private key in memo field",
                            ("type","imported"));
            }
        }

        string wallet_api::get_encrypted_memo( string from, string to, string memo ) {

            if( memo.size() > 0 && memo[0] == '#' ) {
                memo_data m;

                auto from_account = get_account( from );
                auto to_account   = get_account( to );

                m.from            = from_account.memo_key;
                m.to              = to_account.memo_key;
                m.nonce = fc::time_point::now().time_since_epoch().count();

                auto from_priv = my->get_private_key( m.from );
                auto shared_secret = from_priv.get_shared_secret( m.to );

                fc::sha512::encoder enc;
                fc::raw::pack( enc, m.nonce );
                fc::raw::pack( enc, shared_secret );
                auto encrypt_key = enc.result();

                m.encrypted = fc::aes_encrypt( encrypt_key, fc::raw::pack(memo.substr(1)) );
                m.check = fc::sha256::hash( encrypt_key )._hash[0];
                return m;
            } else {
                return memo;
            }
        }

        annotated_signed_transaction wallet_api::transfer(string from, string to, asset amount, string memo, bool broadcast)
        { try {
                WALLET_CHECK_UNLOCKED();
                check_memo( memo, get_account( from ) );
                transfer_operation op;
                op.from = from;
                op.to = to;
                op.amount = amount;

                op.memo = get_encrypted_memo( from, to, memo );

                signed_transaction tx;
                tx.operations.push_back( op );
                tx.validate();

                return my->sign_transaction( tx, broadcast );
            } FC_CAPTURE_AND_RETHROW( (from)(to)(amount)(memo)(broadcast) ) }

        annotated_signed_transaction wallet_api::escrow_transfer(
                string from,
                string to,
                string agent,
                uint32_t escrow_id,
                asset sbd_amount,
                asset steem_amount,
                asset fee,
                time_point_sec ratification_deadline,
                time_point_sec escrow_expiration,
                string json_meta,
                bool broadcast
        )
        {
            WALLET_CHECK_UNLOCKED();
            escrow_transfer_operation op;
            op.from = from;
            op.to = to;
            op.agent = agent;
            op.escrow_id = escrow_id;
            op.sbd_amount = sbd_amount;
            op.steem_amount = steem_amount;
            op.fee = fee;
            op.ratification_deadline = ratification_deadline;
            op.escrow_expiration = escrow_expiration;
            op.json_meta = json_meta;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::escrow_approve(
                string from,
                string to,
                string agent,
                string who,
                uint32_t escrow_id,
                bool approve,
                bool broadcast
        )
        {
            WALLET_CHECK_UNLOCKED();
            escrow_approve_operation op;
            op.from = from;
            op.to = to;
            op.agent = agent;
            op.who = who;
            op.escrow_id = escrow_id;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::escrow_dispute(
                string from,
                string to,
                string agent,
                string who,
                uint32_t escrow_id,
                bool broadcast
        )
        {
            WALLET_CHECK_UNLOCKED();
            escrow_dispute_operation op;
            op.from = from;
            op.to = to;
            op.agent = agent;
            op.who = who;
            op.escrow_id = escrow_id;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::escrow_release(
                string from,
                string to,
                string agent,
                string who,
                string receiver,
                uint32_t escrow_id,
                asset sbd_amount,
                asset steem_amount,
                bool broadcast
        )
        {
            WALLET_CHECK_UNLOCKED();
            escrow_release_operation op;
            op.from = from;
            op.to = to;
            op.agent = agent;
            op.who = who;
            op.receiver = receiver;
            op.escrow_id = escrow_id;
            op.sbd_amount = sbd_amount;
            op.steem_amount = steem_amount;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();
            return my->sign_transaction( tx, broadcast );
        }

/**
 *  Transfers into savings happen immediately, transfers from savings take 72 hours
 */
        annotated_signed_transaction wallet_api::transfer_to_savings( string from, string to, asset amount, string memo, bool broadcast)
        {
            WALLET_CHECK_UNLOCKED();
            check_memo( memo, get_account( from ) );
            transfer_to_savings_operation op;
            op.from = from;
            op.to   = to;
            op.memo = get_encrypted_memo( from, to, memo );
            op.amount = amount;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

/**
 * @param request_id - an unique ID assigned by from account, the id is used to cancel the operation and can be reused after the transfer completes
 */
        annotated_signed_transaction wallet_api::transfer_from_savings( string from, uint32_t request_id, string to, asset amount, string memo, bool broadcast)
        {
            WALLET_CHECK_UNLOCKED();
            check_memo( memo, get_account( from ) );
            transfer_from_savings_operation op;
            op.from = from;
            op.request_id = request_id;
            op.to = to;
            op.amount = amount;
            op.memo = get_encrypted_memo( from, to, memo );

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

/**
 *  @param request_id the id used in transfer_from_savings
 *  @param from the account that initiated the transfer
 */
        annotated_signed_transaction wallet_api::cancel_transfer_from_savings( string from, uint32_t request_id, bool broadcast)
        {
            WALLET_CHECK_UNLOCKED();
            cancel_transfer_from_savings_operation op;
            op.from = from;
            op.request_id = request_id;
            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::transfer_to_vesting(string from, string to, asset amount, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();
            transfer_to_vesting_operation op;
            op.from = from;
            op.to = (to == from ? "" : to);
            op.amount = amount;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::withdraw_vesting(string from, asset vesting_shares, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();
            withdraw_vesting_operation op;
            op.account = from;
            op.vesting_shares = vesting_shares;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::set_withdraw_vesting_route( string from, string to, uint16_t percent, bool auto_vest, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();
            set_withdraw_vesting_route_operation op;
            op.from_account = from;
            op.to_account = to;
            op.percent = percent;
            op.auto_vest = auto_vest;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::convert_sbd(string from, asset amount, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();
            convert_operation op;
            op.owner = from;
            op.requestid = fc::time_point::now().sec_since_epoch();
            op.amount = amount;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::publish_feed(string witness, price exchange_rate, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();
            feed_publish_operation op;
            op.publisher     = witness;
            op.exchange_rate = exchange_rate;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        vector< database_api::convert_request_api_object > wallet_api::get_conversion_requests( string owner_account ) {
            return my->_remote_database_api->get_conversion_requests( owner_account );
        }

        string wallet_api::decrypt_memo( string encrypted_memo ) {
            if( is_locked() ) return encrypted_memo;

            if( encrypted_memo.size() && encrypted_memo[0] == '#' ) {
                auto m = memo_data::from_string( encrypted_memo );
                if( m ) {
                    fc::sha512 shared_secret;
                    auto from_key = my->try_get_private_key( m->from );
                    if( !from_key ) {
                        auto to_key = my->try_get_private_key( m->to );
                        if( !to_key ) return encrypted_memo;
                        shared_secret = to_key->get_shared_secret( m->from );
                    } else {
                        shared_secret = from_key->get_shared_secret( m->to );
                    }
                    fc::sha512::encoder enc;
                    fc::raw::pack( enc, m->nonce );
                    fc::raw::pack( enc, shared_secret );
                    auto encryption_key = enc.result();

                    uint32_t check = fc::sha256::hash( encryption_key )._hash[0];
                    if( check != m->check ) return encrypted_memo;

                    try {
                        vector<char> decrypted = fc::aes_decrypt( encryption_key, m->encrypted );
                        return fc::raw::unpack<std::string>( decrypted );
                    } catch ( ... ){}
                }
            }
            return encrypted_memo;
        }

        annotated_signed_transaction wallet_api::decline_voting_rights( string account, bool decline, bool broadcast )
        {
            WALLET_CHECK_UNLOCKED();
            decline_voting_rights_operation op;
            op.account = account;
            op.decline = decline;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        history_operations wallet_api::get_account_history(string account, uint32_t from, uint32_t limit) {
            auto result = my->_remote_account_history->get_account_history(account, from, limit, account_history_query());
            decrypt_history_memos(result);
            return result;
        }
        history_operations wallet_api::filter_account_history(string account, uint32_t from, uint32_t limit, account_history_query q) {
            auto result = my->_remote_account_history->get_account_history(account, from, limit, q);
            decrypt_history_memos(result);
            return result;
        }

        void wallet_api::decrypt_history_memos(history_operations& result) {
            if (!is_locked()) {
                for (auto& item : result) {
                    if (item.second.op.which() == operation::tag<transfer_operation>::value) {
                        auto& top = item.second.op.get<transfer_operation>();
                        top.memo = decrypt_memo(top.memo);
                    }
                    else if (item.second.op.which() == operation::tag<transfer_from_savings_operation>::value) {
                        auto& top = item.second.op.get<transfer_from_savings_operation>();
                        top.memo = decrypt_memo(top.memo);
                    }
                    else if (item.second.op.which() == operation::tag<transfer_to_savings_operation>::value) {
                        auto& top = item.second.op.get<transfer_to_savings_operation>();
                        top.memo = decrypt_memo(top.memo);
                    }
                }
            }
        }

        vector< database_api::withdraw_vesting_route_api_object > wallet_api::get_withdraw_routes( string account, database_api::withdraw_route_type type )const {
            return my->_remote_database_api->get_withdraw_routes( account, type );
        }

        market_history::order_book wallet_api::get_order_book( uint32_t limit ) {
            return my->_remote_market_history->get_order_book( limit );
        }

        vector< market_history::limit_order > wallet_api::get_open_orders( string owner ) {
            return my->_remote_market_history->get_open_orders( owner );
        }

        annotated_signed_transaction wallet_api::create_order(string owner, uint32_t order_id, asset amount_to_sell, asset min_to_receive, bool fill_or_kill, uint32_t expiration_sec, bool broadcast) {
            WALLET_CHECK_UNLOCKED();
            limit_order_create_operation op;
            op.owner = owner;
            op.orderid = order_id;
            op.amount_to_sell = amount_to_sell;
            op.min_to_receive = min_to_receive;
            op.fill_or_kill = fill_or_kill;
            op.expiration = expiration_sec ? (fc::time_point::now() + fc::seconds(expiration_sec)) : fc::time_point::maximum();

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::cancel_order( string owner, uint32_t orderid, bool broadcast ) {
            WALLET_CHECK_UNLOCKED();
            limit_order_cancel_operation op;
            op.owner = owner;
            op.orderid = orderid;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::post_comment( string author, string permlink, string parent_author, string parent_permlink, string title, string body, string json, bool broadcast ) {
            WALLET_CHECK_UNLOCKED();
            comment_operation op;
            op.parent_author = parent_author;
            op.parent_permlink = parent_permlink;
            op.author = author;
            op.permlink = permlink;
            op.title = title;
            op.body = body;
            op.json_metadata = json;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        annotated_signed_transaction wallet_api::vote( string voter, string author, string permlink, int16_t weight, bool broadcast ) {
            WALLET_CHECK_UNLOCKED();
            GOLOS_CHECK_PARAM(weight, GOLOS_CHECK_VALUE(abs(weight) <= 100, "Weight must be between -100 and 100"));

            vote_operation op;
            op.voter = voter;
            op.author = author;
            op.permlink = permlink;
            op.weight = weight * STEEMIT_1_PERCENT;

            signed_transaction tx;
            tx.operations.push_back( op );
            tx.validate();

            return my->sign_transaction( tx, broadcast );
        }

        void wallet_api::set_transaction_expiration(uint32_t seconds) {
            GOLOS_CHECK_PARAM(seconds,
                my->set_transaction_expiration(seconds));
        }

        annotated_signed_transaction wallet_api::get_transaction( transaction_id_type id )const {
            return my->_remote_operation_history->get_transaction( id );
        }

        message_box_query get_message_box_query(const optional_private_box_query& query_template) {
            message_box_query query;
            query.newest_date = time_converter(query_template.newest_date, time_point::now(), time_point::now()).time();
            query.select_accounts = query_template.select_accounts;
            query.filter_accounts = query_template.filter_accounts;
            if (query_template.unread_only) {
                query.unread_only = *query_template.unread_only;
            }
            if (query_template.limit) {
                query.limit = *query_template.limit;
            }
            if (query_template.offset) {
                query.offset = *query_template.offset;
            }
            return query;
        }

        vector<extended_message_object> wallet_api::get_private_inbox(
            const std::string& to, const optional_private_box_query& query
        ) {
            WALLET_CHECK_UNLOCKED();
            return my->decrypt_private_messages(
                my->_remote_private_message->get_inbox(
                    to, get_message_box_query(query)));
        }

        vector<extended_message_object> wallet_api::get_private_outbox(
            const std::string& from, const optional_private_box_query& query
        ) {
            WALLET_CHECK_UNLOCKED();
            return my->decrypt_private_messages(
                my->_remote_private_message->get_outbox(
                     from, get_message_box_query(query)));
        }

        vector<extended_message_object> wallet_api::get_private_thread(
            const std::string& from, const std::string& to, const optional_private_thread_query& query_template
        ) {
            WALLET_CHECK_UNLOCKED();
            std::vector<extended_message_object> result;
            message_thread_query query;
            query.newest_date = time_converter(query_template.newest_date, time_point::now(), time_point::now()).time();
            if (query_template.unread_only) {
                query.unread_only = *query_template.unread_only;
            }
            if (query_template.limit) {
                query.limit = *query_template.limit;
            }
            if (query_template.offset) {
                query.offset = *query_template.offset;
            }
            auto remote_result = my->_remote_private_message->get_thread(from, to, query);
            result.reserve(remote_result.size());
            for (const auto& item : remote_result) {
                result.emplace_back(item);
                message_body tmp = my->try_decrypt_private_message(item);
                result.back().message = std::move(tmp);
            }
            return result;
        }

        annotated_signed_transaction wallet_api::set_private_settings(
            const std::string& owner, const settings_api_object& s, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();
            private_settings_operation op;

            op.owner = owner;
            op.ignore_messages_from_unknown_contact = s.ignore_messages_from_unknown_contact;

            private_message_plugin_operation pop = op;

            custom_json_operation jop;
            jop.id   = "private_message";
            jop.json = fc::json::to_string(pop);
            jop.required_posting_auths.insert(owner);

            signed_transaction trx;
            trx.operations.push_back(jop);
            trx.validate();

            return my->sign_transaction(trx, broadcast);
        }

        settings_api_object wallet_api::get_private_settings(const std::string& owner) {
            return my->_remote_private_message->get_settings(owner);
        }

        annotated_signed_transaction wallet_api::add_private_contact(
            const std::string& owner, const std::string& contact,
            private_contact_type type, fc::optional<std::string> json_metadata, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            private_contact_operation op;

            op.owner = owner;
            op.contact = contact;
            op.type = type;

            if (type == golos::plugins::private_message::unknown) {
                // op.json_metadata.clear();
            } else if (!json_metadata) {
                op.json_metadata = my->_remote_private_message->get_contact_info(owner, contact).json_metadata;
            } else {
                op.json_metadata = *json_metadata;
            }

            private_message_plugin_operation pop = op;

            custom_json_operation jop;
            jop.id   = "private_message";
            jop.json = fc::json::to_string(pop);
            jop.required_posting_auths.insert(owner);

            signed_transaction trx;
            trx.operations.push_back(jop);
            trx.validate();

            return my->sign_transaction(trx, broadcast);
        }

        vector<contact_api_object> wallet_api::get_private_contacts(
            const std::string& owner, private_contact_type type, uint16_t limit, uint32_t offset
        ) {
            return my->_remote_private_message->get_contacts(owner, type, limit, offset);
        }

        contact_api_object wallet_api::get_private_contact(
            const std::string& owner, const std::string& contact
        ) {
            return my->_remote_private_message->get_contact_info(owner, contact);
        }

        annotated_signed_transaction wallet_api::edit_private_message(
            const std::string& from, const std::string& to, const uint64_t nonce,
            const message_body& message, bool broadcast
        ) {
            return my->send_private_message(from, to, nonce, true, message, broadcast);
        }

        annotated_signed_transaction wallet_api::send_private_message(
            const std::string& from, const std::string& to, const message_body& message, bool broadcast
        ) {
            auto nonce = fc::time_point::now().time_since_epoch().count();
            return my->send_private_message(from, to, nonce, false, message, broadcast);
        }

        annotated_signed_transaction wallet_api::delete_inbox_private_message(
            const std::string& from, const std::string& to, const uint64_t nonce, bool broadcast
        ) {
            return my->delete_private_message(to, from, to, nonce, broadcast);
        }

        annotated_signed_transaction wallet_api::delete_inbox_private_messages(
            const std::string& from, const std::string& to,
            const std::string& start_date, const std::string& stop_date,
            bool broadcast
        ) {
            return my->delete_private_messages(to, from, to, start_date, stop_date, broadcast);
        }

        annotated_signed_transaction wallet_api::delete_outbox_private_message(
            const std::string& from, const std::string& to, const uint64_t nonce, bool broadcast
        ) {
            return my->delete_private_message(from, from, to, nonce, broadcast);
        }

        annotated_signed_transaction wallet_api::delete_outbox_private_messages(
            const std::string& from, const std::string& to,
            const std::string& start_date, const std::string& stop_date,
            bool broadcast
        ) {
            return my->delete_private_messages(from, from, to, start_date, stop_date, broadcast);
        }

        annotated_signed_transaction wallet_api::mark_private_message(
            const std::string& from, const std::string& to, const uint64_t nonce, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();
            GOLOS_CHECK_PARAM(nonce, GOLOS_CHECK_VALUE(nonce != 0, "You should specify nonce of marked message"));

            private_mark_message_operation op;
            op.from = from;
            op.to = to;
            op.nonce = nonce;
            op.start_date = time_point_sec::min();
            op.stop_date = time_point_sec::min();

            private_message_plugin_operation pop = op;

            custom_json_operation jop;
            jop.id   = "private_message";
            jop.json = fc::json::to_string(pop);
            jop.required_posting_auths.insert(to);

            signed_transaction trx;
            trx.operations.push_back(jop);
            trx.validate();

            return my->sign_transaction(trx, broadcast);
        }

        annotated_signed_transaction wallet_api::mark_private_messages(
            const std::string& from, const std::string& to,
            const std::string& start_date, const std::string& stop_date,
            bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            private_mark_message_operation op;
            op.from = from;
            op.to = to;
            op.nonce = 0;
            op.start_date = time_converter(start_date, time_point::now(), time_point_sec::min()).time();
            op.stop_date = time_converter(stop_date, time_point::now(), time_point::now()).time();

            private_message_plugin_operation pop = op;

            custom_json_operation jop;
            jop.id   = "private_message";
            jop.json = fc::json::to_string(pop);
            jop.required_posting_auths.insert(to);

            signed_transaction trx;
            trx.operations.push_back(jop);
            trx.validate();

            return my->sign_transaction(trx, broadcast);
        }

        annotated_signed_transaction wallet_api::follow(
                const string& follower,
                const string& following,
                const set<string>& what,
                const bool broadcast) {

            GOLOS_CHECK_PARAM(following, GOLOS_CHECK_VALUE(following.size() > 0, "Empty string is not allowed"));
            string _following = following;

            auto follwer_account = get_account( follower );
            if( _following[0] != '@' || _following[0] != '#' ) {
                _following = '@' + _following;
            }
            if( _following[0] == '@' ) {
                get_account( _following.substr(1) );
            }

            follow::follow_operation fop;
            fop.follower = follower;
            fop.following = _following;
            fop.what = what;
            follow::follow_plugin_operation op = fop;

            custom_json_operation jop;
            jop.id = "follow";
            jop.json = fc::json::to_string(op);
            jop.required_posting_auths.insert(follower);

            signed_transaction trx;
            trx.operations.push_back( jop );
            trx.validate();

            return my->sign_transaction( trx, broadcast );
        }

        annotated_signed_transaction wallet_api::worker_proposal(
                const std::string& author, const std::string& permlink, worker_proposal_type type, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_proposal_operation op;
            op.author = author;
            op.permlink = permlink;
            op.type = type;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::delete_worker_proposal(
                const std::string& author, const std::string& permlink, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_proposal_delete_operation op;
            op.author = author;
            op.permlink = permlink;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::worker_techspec(
                const std::string& author, const std::string& permlink,
                const std::string& worker_proposal_author, const std::string& worker_proposal_permlink,
                const asset& specification_cost, const asset& development_cost, const std::string& worker,
                uint16_t payments_count, uint32_t payments_interval, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_techspec_operation op;
            op.author = author;
            op.permlink = permlink;
            op.worker_proposal_author = worker_proposal_author;
            op.worker_proposal_permlink = worker_proposal_permlink;
            op.specification_cost = specification_cost;
            op.development_cost = development_cost;
            op.worker = worker;
            op.payments_count = payments_count;
            op.payments_interval = payments_interval;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::delete_worker_techspec(
                const std::string& author, const std::string& permlink, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_techspec_delete_operation op;
            op.author = author;
            op.permlink = permlink;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::approve_worker_techspec(
                const std::string& approver, const std::string& author, const std::string& permlink,
                worker_techspec_approve_state state, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_techspec_approve_operation op;
            op.approver = approver;
            op.author = author;
            op.permlink = permlink;
            op.state = state;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::assign_worker(
                const std::string& assigner, const std::string& worker_techspec_author,
                const std::string& worker_techspec_permlink, const std::string& worker, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_assign_operation op;
            op.assigner = assigner;
            op.worker_techspec_author = worker_techspec_author;
            op.worker_techspec_permlink = worker_techspec_permlink;
            op.worker = worker;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::worker_result(
                const std::string& author, const std::string& permlink, const std::string& worker_techspec_permlink, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_result_operation op;
            op.author = author;
            op.permlink = permlink;
            op.worker_techspec_permlink = worker_techspec_permlink;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::delete_worker_result(
                const std::string& author, const std::string& permlink, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_result_delete_operation op;
            op.author = author;
            op.permlink = permlink;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::approve_worker_payment(
                const std::string& approver, const std::string& worker_techspec_author, const std::string& worker_techspec_permlink,
                worker_techspec_approve_state state, bool broadcast
        ) {
            WALLET_CHECK_UNLOCKED();

            worker_payment_approve_operation op;
            op.approver = approver;
            op.worker_techspec_author = worker_techspec_author;
            op.worker_techspec_permlink = worker_techspec_permlink;
            op.state = state;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

} } // golos::wallet

FC_REFLECT_ENUM(golos::wallet::logic_errors::types,
        (detected_private_key_in_memo)
        (owner_authority_change_would_render_account_irrecoverable)
        (private_key_not_available)
        (no_account_in_lut)
        (malformed_private_key)
);
