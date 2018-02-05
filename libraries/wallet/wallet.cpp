#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
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
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <fc/container/deque.hpp>
#include <fc/git_revision.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/smart_ref_impl.hpp>

#include <golos/utilities/git_revision.hpp>
#include <golos/utilities/key_conversion.hpp>
#include <golos/utilities/words.hpp>

#include <golos/application/api.hpp>
#include <golos/follow/follow_operations.hpp>
#include <golos/private_message/private_message_operations.hpp>
#include <golos/wallet/wallet.hpp>
#include <golos/wallet/api_documentation.hpp>
#include <golos/wallet/reflect_utilities.hpp>

#include <golos/account_by_key/account_by_key_api.hpp>

#ifndef WIN32

#endif

#define BRAIN_KEY_WORD_COUNT 16

namespace golos {
    namespace wallet {

        namespace detail {

            template<class T>
            optional<T> maybe_id(const string &name_or_id) {
                if (std::isdigit(name_or_id.front())) {
                    try {
                        return fc::variant(name_or_id).as<T>();
                    } catch (const fc::exception &) {
                    }
                }
                return optional<T>();
            }

            string pubkey_to_shorthash(const public_key_type &key) {
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
                result += hd[(x) & 0x0f];

                return result;
            }


            fc::ecc::private_key derive_private_key(const std::string &prefix_string, int sequence_number) {
                std::string sequence_string = std::to_string(sequence_number);
                fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
                fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
                return derived_key;
            }

            string normalize_brain_key(string s) {
                size_t i = 0, n = s.length();
                std::string result;
                char c;
                result.reserve(n);

                bool preceded_by_whitespace = false;
                bool non_empty = false;
                while (i < n) {
                    c = s[i++];
                    switch (c) {
                        case ' ':
                        case '\t':
                        case '\r':
                        case '\n':
                        case '\v':
                        case '\f':
                            preceded_by_whitespace = true;
                            continue;

                        case 'a':
                            c = 'A';
                            break;
                        case 'b':
                            c = 'B';
                            break;
                        case 'c':
                            c = 'C';
                            break;
                        case 'd':
                            c = 'D';
                            break;
                        case 'e':
                            c = 'E';
                            break;
                        case 'f':
                            c = 'F';
                            break;
                        case 'g':
                            c = 'G';
                            break;
                        case 'h':
                            c = 'H';
                            break;
                        case 'i':
                            c = 'I';
                            break;
                        case 'j':
                            c = 'J';
                            break;
                        case 'k':
                            c = 'K';
                            break;
                        case 'l':
                            c = 'L';
                            break;
                        case 'm':
                            c = 'M';
                            break;
                        case 'n':
                            c = 'N';
                            break;
                        case 'o':
                            c = 'O';
                            break;
                        case 'p':
                            c = 'P';
                            break;
                        case 'q':
                            c = 'Q';
                            break;
                        case 'r':
                            c = 'R';
                            break;
                        case 's':
                            c = 'S';
                            break;
                        case 't':
                            c = 'T';
                            break;
                        case 'u':
                            c = 'U';
                            break;
                        case 'v':
                            c = 'V';
                            break;
                        case 'w':
                            c = 'W';
                            break;
                        case 'x':
                            c = 'X';
                            break;
                        case 'y':
                            c = 'Y';
                            break;
                        case 'z':
                            c = 'Z';
                            break;

                        default:
                            break;
                    }
                    if (preceded_by_whitespace && non_empty) {
                        result.push_back(' ');
                    }
                    result.push_back(c);
                    preceded_by_whitespace = false;
                    non_empty = true;
                }
                return result;
            }

            struct op_prototype_visitor {
                typedef void result_type;

                int t = 0;
                flat_map<std::string, operation> &name2op;

                op_prototype_visitor(int _t, flat_map<std::string, operation> &_prototype_ops) : t(_t),
                        name2op(_prototype_ops) {
                }

                template<typename Type>
                result_type operator()(const Type &op) const {
                    string name = fc::get_typename<Type>::name();
                    size_t p = name.rfind(':');
                    if (p != string::npos) {
                        name = name.substr(p + 1);
                    }
                    name2op[name] = Type();
                }
            };

            class wallet_api_impl {
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
                    for (int t = 0; t < op.count(); t++) {
                        op.set_which(t);
                        op.visit(op_prototype_visitor(t, _prototype_ops));
                    }
                    return;
                }

            public:
                wallet_api &self;

                wallet_api_impl(wallet_api &s, const wallet_data &initial_data, fc::api<login_api> rapi) : self(s),
                        _remote_api(rapi), _remote_db(rapi->get_api_by_name("database_api")->as<database_api>()),
                        _remote_net_broadcast(
                                rapi->get_api_by_name("network_broadcast_api")->as<network_broadcast_api>()) {
                    init_prototype_ops();

                    _wallet.ws_server = initial_data.ws_server;
                    _wallet.ws_user = initial_data.ws_user;
                    _wallet.ws_password = initial_data.ws_password;
                }

                virtual ~wallet_api_impl() {
                }

                void encrypt_keys() {
                    if (!is_locked()) {
                        plain_keys data;
                        data.keys = _keys;
                        data.checksum = _checksum;
                        auto plain_txt = fc::raw::pack(data);
                        _wallet.cipher_keys = fc::aes_encrypt(data.checksum, plain_txt);
                    }
                }

                bool copy_wallet_file(string destination_filename) {
                    fc::path src_path = get_wallet_filename();
                    if (!fc::exists(src_path)) {
                        return false;
                    }
                    fc::path dest_path = destination_filename + _wallet_filename_extension;
                    int suffix = 0;
                    while (fc::exists(dest_path)) {
                        ++suffix;
                        dest_path = destination_filename + "-" + std::to_string(suffix) + _wallet_filename_extension;
                    }
                    wlog("backing up wallet ${src} to ${dest}", ("src", src_path)("dest", dest_path));

                    fc::path dest_parent = fc::absolute(dest_path).parent_path();
                    try {
                        enable_umask_protection();
                        if (!fc::exists(dest_parent)) {
                            fc::create_directories(dest_parent);
                        }
                        fc::copy(src_path, dest_path);
                        disable_umask_protection();
                    } catch (...) {
                        disable_umask_protection();
                        throw;
                    }
                    return true;
                }

                bool is_locked() const {
                    return _checksum == fc::sha512();
                }

                variant info() const {
                    auto dynamic_props = _remote_db->get_dynamic_global_properties();
                    fc::mutable_variant_object result(fc::variant(dynamic_props).get_object());
                    result["witness_majority_version"] = fc::string(
                            _remote_db->get_witness_schedule().majority_version);
                    result["hardfork_version"] = fc::string(_remote_db->get_hardfork_version());
                    result["head_block_num"] = dynamic_props.head_block_number;
                    result["head_block_id"] = dynamic_props.head_block_id;
                    result["head_block_age"] = fc::get_approximate_relative_time_string(dynamic_props.time,
                                                                                        time_point_sec(
                                                                                                time_point::now()),
                                                                                        " old");
                    result["participation"] = (100 * dynamic_props.recent_slots_filled.popcount()) / 128.0;
                    result["median_sbd_price"] = _remote_db->get_current_median_history_price();
                    result["account_creation_fee"] = _remote_db->get_chain_properties().account_creation_fee;
                    result["post_reward_fund"] = fc::variant(
                            _remote_db->get_reward_fund(STEEMIT_POST_REWARD_FUND_NAME)).get_object();
                    result["comment_reward_fund"] = fc::variant(
                            _remote_db->get_reward_fund(STEEMIT_COMMENT_REWARD_FUND_NAME)).get_object();
                    return result;
                }

                variant_object about() const {
                    string client_version(graphene::utilities::git_revision_description);
                    const size_t pos = client_version.find('/');
                    if (pos != string::npos && client_version.size() > pos) {
                        client_version = client_version.substr(pos + 1);
                    }

                    fc::mutable_variant_object result;
                    result["blockchain_name"] = BLOCKCHAIN_NAME;
                    result["chain_id"] = STEEMIT_CHAIN_ID;
                    result["blockchain_version"] = STEEMIT_BLOCKCHAIN_VERSION;
                    result["address_prefix"] = STEEMIT_ADDRESS_PREFIX;
                    result["client_version"] = client_version;
                    result["steem_revision"] = graphene::utilities::git_revision_sha;
                    result["steem_revision_age"] = fc::get_approximate_relative_time_string(
                            fc::time_point_sec(graphene::utilities::git_revision_unix_timestamp));
                    result["fc_revision"] = fc::git_revision_sha;
                    result["fc_revision_age"] = fc::get_approximate_relative_time_string(
                            fc::time_point_sec(fc::git_revision_unix_timestamp));
                    result["compile_date"] = "compiled on " __DATE__ " at " __TIME__;
                    result["boost_version"] = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
                    result["openssl_version"] = OPENSSL_VERSION_TEXT;

                    std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int *)) + "-bit";
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
                        auto v = _remote_api->get_version();
                        result["server_blockchain_version"] = v.blockchain_version;
                        result["server_steem_revision"] = v.steem_revision;
                        result["server_fc_revision"] = v.fc_revision;
                    } catch (fc::exception &) {
                        result["server"] = "could not retrieve server version information";
                    }

                    return result;
                }

                account_api_obj get_account(string account_name) const {
                    auto accounts = _remote_db->get_accounts({account_name});
                    FC_ASSERT(!accounts.empty(), "Unknown account");
                    return accounts.front();
                }

                proposal_object get_proposal(string account_name, integral_id_type id) const {
                    auto proposed_transactions = _remote_db->get_proposed_transactions(account_name);
                    FC_ASSERT(!proposed_transactions.empty(), "No proposed transactions referred to this account");
                    return *std::find_if(proposed_transactions.begin(), proposed_transactions.end(),
                                         [&](const vector<proposal_object>::value_type &iterator) -> bool {
                                             return iterator.proposal_id == id;
                                         });
                }

                optional<asset_object> find_asset(asset_symbol_type asset_symbol) const {
                    // It's a symbol
                    optional<asset_object> rec = _remote_db->get_assets(
                            {asset<0, 17, 0>(0, asset_symbol).symbol}).front();
                    if (rec) {
                        if (rec->asset_name != asset<0, 17, 0>(0, asset_symbol).symbol_name()) {
                            return optional<asset_object>();
                        }

                        _asset_cache[rec->asset_name] = *rec;
                    }
                    return rec;
                }

                optional<asset_object> find_asset(string asset_symbol) const {
                    FC_ASSERT(asset_symbol.size() > 0);

                    // It's a symbol
                    optional<asset_object> rec = _remote_db->get_assets({asset_name_type(asset_symbol)}).front();
                    if (rec) {
                        if (rec->asset_name != asset_symbol) {
                            return optional<asset_object>();
                        }

                        _asset_cache[rec->asset_name] = *rec;
                    }
                    return rec;
                }

                asset_object get_asset(string asset_symbol) const {
                    auto opt = find_asset(asset_symbol);
                    FC_ASSERT(opt);
                    return *opt;
                }

                asset_bitasset_data_object get_bitasset_data(string asset_symbol) const {
                    auto asset = get_asset(asset_symbol);
                    FC_ASSERT(asset.is_market_issued());
                    fc::optional<asset_bitasset_data_object> b = _remote_db->get_bitassets_data({asset_symbol}).front();

                    FC_ASSERT(b);
                    return *b;
                }

                string get_wallet_filename() const {
                    return _wallet_filename;
                }

                optional<fc::ecc::private_key> try_get_private_key(const public_key_type &id) const {
                    auto it = _keys.find(id);
                    if (it != _keys.end()) {
                        return wif_to_key(it->second);
                    }
                    return optional<fc::ecc::private_key>();
                }

                fc::ecc::private_key get_private_key(const public_key_type &id) const {
                    auto has_key = try_get_private_key(id);
                    FC_ASSERT(has_key);
                    return *has_key;
                }


                fc::ecc::private_key get_private_key_for_account(const account_api_obj &account) const {
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
                    // TODO:  Merge imported wallet with existing wallet,
                    //        instead of replacing it
                    if (wallet_filename == "") {
                        wallet_filename = _wallet_filename;
                    }

                    if (!fc::exists(wallet_filename)) {
                        return false;
                    }

                    _wallet = fc::json::from_file(wallet_filename).as<wallet_data>();

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

                    if (wallet_filename == "") {
                        wallet_filename = _wallet_filename;
                    }

                    wlog("saving wallet to file ${fn}", ("fn", wallet_filename));

                    string data = fc::json::to_pretty_string(_wallet);
                    try {
                        enable_umask_protection();
                        //
                        // Parentheses on the following declaration fails to compile,
                        // due to the Most Vexing Parse.  Thanks, C++
                        //
                        // http://en.wikipedia.org/wiki/Most_vexing_parse
                        //
                        fc::ofstream outfile{fc::path(wallet_filename)};
                        outfile.write(data.c_str(), data.length());
                        outfile.flush();
                        outfile.close();
                        disable_umask_protection();
                    } catch (...) {
                        disable_umask_protection();
                        throw;
                    }
                }

                // This function generates derived keys starting with index 0 and keeps incrementing
                // the index until it finds a key that isn't registered in the block chain.  To be
                // safer, it continues checking for a few more keys to make sure there wasn't a short gap
                // caused by a failed registration or the like.
                int find_first_unused_derived_key_index(const fc::ecc::private_key &parent_key) {
                    int first_unused_index = 0;
                    int number_of_consecutive_unused_keys = 0;
                    for (int key_index = 0;; ++key_index) {
                        fc::ecc::private_key derived_private_key = derive_private_key(key_to_wif(parent_key),
                                                                                      key_index);
                        golos::chain::public_key_type derived_public_key = derived_private_key.get_public_key();
                        if (_keys.find(derived_public_key) == _keys.end()) {
                            if (number_of_consecutive_unused_keys) {
                                ++number_of_consecutive_unused_keys;
                                if (number_of_consecutive_unused_keys > 5) {
                                    return first_unused_index;
                                }
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
                                                                   string account_name, string creator_account_name,
                                                                   bool broadcast = false, bool save_wallet = true) {
                    try {
                        int active_key_index = find_first_unused_derived_key_index(owner_privkey);
                        fc::ecc::private_key active_privkey = derive_private_key(key_to_wif(owner_privkey),
                                                                                 active_key_index);

                        int memo_key_index = find_first_unused_derived_key_index(active_privkey);
                        fc::ecc::private_key memo_privkey = derive_private_key(key_to_wif(active_privkey),
                                                                               memo_key_index);

                        golos::chain::public_key_type owner_pubkey = owner_privkey.get_public_key();
                        golos::chain::public_key_type active_pubkey = active_privkey.get_public_key();
                        golos::chain::public_key_type memo_pubkey = memo_privkey.get_public_key();

                        account_create_operation<0, 17, 0> account_create_op;

                        account_create_op.creator = creator_account_name;
                        account_create_op.new_account_name = account_name;
                        account_create_op.fee = _remote_db->get_chain_properties().account_creation_fee;
                        account_create_op.owner = authority(1, owner_pubkey, 1);
                        account_create_op.active = authority(1, active_pubkey, 1);
                        account_create_op.memo_key = memo_pubkey;

                        signed_transaction tx;

                        tx.operations.push_back(account_create_op);
                        tx.validate();

                        if (save_wallet) {
                            save_wallet_file();
                        }
                        if (broadcast) {
                            //_remote_net_broadcast->broadcast_transaction( tx );
                            auto result = _remote_net_broadcast->broadcast_transaction_synchronous(tx);
                        }
                        return tx;
                    } FC_CAPTURE_AND_RETHROW((account_name)(creator_account_name)(broadcast))
                }

                signed_transaction set_voting_proxy(string account_to_modify, string proxy,
                                                    bool broadcast /* = false */) {
                    try {
                        account_witness_proxy_operation<0, 17, 0> op;
                        op.account = account_to_modify;
                        op.proxy = proxy;

                        signed_transaction tx;
                        tx.operations.push_back(op);
                        tx.validate();

                        return sign_transaction(tx, broadcast);
                    } FC_CAPTURE_AND_RETHROW((account_to_modify)(proxy)(broadcast))
                }

                optional<witness_api_obj> get_witness(string owner_account) {
                    return _remote_db->get_witness_by_account(owner_account);
                }

                void set_transaction_expiration(uint32_t tx_expiration_seconds) {
                    FC_ASSERT(tx_expiration_seconds < STEEMIT_MAX_TIME_UNTIL_EXPIRATION);
                    _tx_expiration_seconds = tx_expiration_seconds;
                }

                annotated_signed_transaction sign_transaction(signed_transaction tx, bool broadcast = false) {
                    flat_set<account_name_type> req_active_approvals;
                    flat_set<account_name_type> req_owner_approvals;
                    flat_set<account_name_type> req_posting_approvals;
                    vector<authority> other_auths;

                    tx.get_required_authorities(req_active_approvals, req_owner_approvals, req_posting_approvals,
                                                other_auths);

                    for (const auto &auth : other_auths) {
                        for (const auto &a : auth.account_auths) {
                            req_active_approvals.insert(a.first);
                        }
                    }

                    // std::merge lets us de-duplicate account_name's that occur in both
                    //   sets, and dump them into a vector (as required by remote_db api)
                    //   at the same time
                    vector<string> v_approving_account_names;
                    std::merge(req_active_approvals.begin(), req_active_approvals.end(), req_owner_approvals.begin(),
                               req_owner_approvals.end(), std::back_inserter(v_approving_account_names));

                    for (const auto &a : req_posting_approvals) {
                        v_approving_account_names.push_back(a);
                    }

                    /// TODO: fetch the accounts specified via other_auths as well.

                    auto approving_account_objects = _remote_db->get_accounts(v_approving_account_names);

                    /// TODO: recursively check one layer deeper in the authority tree for keys

                    FC_ASSERT(approving_account_objects.size() == v_approving_account_names.size(), "",
                              ("aco.size:", approving_account_objects.size())("acn", v_approving_account_names.size()));

                    flat_map<string, account_api_obj> approving_account_lut;
                    size_t i = 0;
                    for (const optional<account_api_obj> &approving_acct : approving_account_objects) {
                        if (!approving_acct.valid()) {
                            wlog("operation_get_required_auths said approval of non-existing account ${name} was needed",
                                 ("name", v_approving_account_names[i]));
                            i++;
                            continue;
                        }
                        approving_account_lut[approving_acct->name] = *approving_acct;
                        i++;
                    }
                    auto get_account_from_lut = [&](const std::string &name) -> const account_api_obj & {
                        auto it = approving_account_lut.find(name);
                        FC_ASSERT(it != approving_account_lut.end());
                        return it->second;
                    };

                    flat_set<public_key_type> approving_key_set;
                    for (account_name_type &acct_name : req_active_approvals) {
                        const auto it = approving_account_lut.find(acct_name);
                        if (it == approving_account_lut.end()) {
                            continue;
                        }
                        const account_api_obj &acct = it->second;
                        vector<public_key_type> v_approving_keys = acct.active.get_keys();
                        wdump((v_approving_keys));
                        for (const public_key_type &approving_key : v_approving_keys) {
                            wdump((approving_key));
                            approving_key_set.insert(approving_key);
                        }
                    }

                    for (account_name_type &acct_name : req_posting_approvals) {
                        const auto it = approving_account_lut.find(acct_name);
                        if (it == approving_account_lut.end()) {
                            continue;
                        }
                        const account_api_obj &acct = it->second;
                        vector<public_key_type> v_approving_keys = acct.posting.get_keys();
                        wdump((v_approving_keys));
                        for (const public_key_type &approving_key : v_approving_keys) {
                            wdump((approving_key));
                            approving_key_set.insert(approving_key);
                        }
                    }

                    for (const account_name_type &acct_name : req_owner_approvals) {
                        const auto it = approving_account_lut.find(acct_name);
                        if (it == approving_account_lut.end()) {
                            continue;
                        }
                        const account_api_obj &acct = it->second;
                        vector<public_key_type> v_approving_keys = acct.owner.get_keys();
                        for (const public_key_type &approving_key : v_approving_keys) {
                            wdump((approving_key));
                            approving_key_set.insert(approving_key);
                        }
                    }
                    for (const authority &a : other_auths) {
                        for (const auto &k : a.key_auths) {
                            wdump((k.first));
                            approving_key_set.insert(k.first);
                        }
                    }

                    auto dyn_props = _remote_db->get_dynamic_global_properties();
                    tx.set_reference_block(dyn_props.head_block_id);
                    tx.set_expiration(dyn_props.time + fc::seconds(_tx_expiration_seconds));
                    tx.signatures.clear();

                    //idump((_keys));
                    flat_set<public_key_type> available_keys;
                    flat_map<public_key_type, fc::ecc::private_key> available_private_keys;
                    for (const public_key_type &key : approving_key_set) {
                        auto it = _keys.find(key);
                        if (it != _keys.end()) {
                            fc::optional<fc::ecc::private_key> privkey = wif_to_key(it->second);
                            FC_ASSERT(privkey.valid(), "Malformed private key in _keys");
                            available_keys.insert(key);
                            available_private_keys[key] = *privkey;
                        }
                    }

                    auto minimal_signing_keys = tx.minimize_required_signatures(STEEMIT_CHAIN_ID, available_keys,
                                                                                [&](const string &account_name) -> const authority & {
                                                                                    return (get_account_from_lut(
                                                                                            account_name).active);
                                                                                },
                                                                                [&](const string &account_name) -> const authority & {
                                                                                    return (get_account_from_lut(
                                                                                            account_name).owner);
                                                                                },
                                                                                [&](const string &account_name) -> const authority & {
                                                                                    return (get_account_from_lut(
                                                                                            account_name).posting);
                                                                                }, STEEMIT_MAX_SIG_CHECK_DEPTH);

                    for (const public_key_type &k : minimal_signing_keys) {
                        auto it = available_private_keys.find(k);
                        FC_ASSERT(it != available_private_keys.end());
                        tx.sign(it->second, STEEMIT_CHAIN_ID);
                    }

                    if (broadcast) {
                        try {
                            auto result = _remote_net_broadcast->broadcast_transaction_synchronous(tx);
                            annotated_signed_transaction rtrx(tx);
                            rtrx.block_num = result.get_object()["block_num"].as_uint64();
                            rtrx.transaction_num = result.get_object()["trx_num"].as_uint64();
                            return rtrx;
                        } catch (const fc::exception &e) {
                            elog("Caught exception while broadcasting tx ${id}:  ${e}",
                                 ("id", tx.id().str())("e", e.to_detail_string()));
                            throw;
                        }
                    }
                    return tx;
                }

                std::map<string,
                        std::function<string(fc::variant, const fc::variants &)>> get_result_formatters() const {
                    std::map<string, std::function<string(fc::variant, const fc::variants &)>> m;
                    m["help"] = [](variant result, const fc::variants &a) {
                        return result.get_string();
                    };

                    m["get_help"] = [](variant result, const fc::variants &a) {
                        return result.get_string();
                    };

                    m["list_my_accounts"] = [](variant result, const fc::variants &a) {
                        std::stringstream out;

                        auto accounts = result.as<vector<account_api_obj>>();
                        asset<0, 17, 0> total_steem;
                        asset<0, 17, 0> total_vest(0, VESTS_SYMBOL);
                        asset<0, 17, 0> total_sbd(0, SBD_SYMBOL_NAME);
                        for (const auto &a : accounts) {
                            total_steem += a.balance;
                            total_vest += a.vesting_shares;
                            total_sbd += a.sbd_balance;
                            out << std::left << std::setw(17) << std::string(a.name) << std::right << std::setw(20)
                                << fc::variant(a.balance).as_string() << " " << std::right << std::setw(20)
                                << fc::variant(a.vesting_shares).as_string() << " " << std::right << std::setw(20)
                                << fc::variant(a.sbd_balance).as_string() << "\n";
                        }
                        out << "-------------------------------------------------------------------------\n";
                        out << std::left << std::setw(17) << "TOTAL" << std::right << std::setw(20)
                            << fc::variant(total_steem).as_string() << " " << std::right << std::setw(20)
                            << fc::variant(total_vest).as_string() << " " << std::right << std::setw(20)
                            << fc::variant(total_sbd).as_string() << "\n";
                        return out.str();
                    };
                    m["get_account_history"] = [](variant result, const fc::variants &a) {
                        std::stringstream ss;
                        ss << std::left << std::setw(5) << "#" << " ";
                        ss << std::left << std::setw(10) << "BLOCK #" << " ";
                        ss << std::left << std::setw(15) << "TRX ID" << " ";
                        ss << std::left << std::setw(20) << "OPERATION" << " ";
                        ss << std::left << std::setw(50) << "DETAILS" << "\n";
                        ss << "-------------------------------------------------------------------------------\n";
                        const auto &results = result.get_array();
                        for (const auto &item : results) {
                            ss << std::left << std::setw(5) << item.get_array()[0].as_string() << " ";
                            const auto &op = item.get_array()[1].get_object();
                            ss << std::left << std::setw(10) << op["block"].as_string() << " ";
                            ss << std::left << std::setw(15) << op["trx_id"].as_string() << " ";
                            const auto &opop = op["op"].get_array();
                            ss << std::left << std::setw(20) << opop[0].as_string() << " ";
                            ss << std::left << std::setw(50) << fc::json::to_string(opop[1]) << "\n ";
                        }
                        return ss.str();
                    };
                    m["get_limit_orders_by_owner"] = [](variant result, const fc::variants &a) {
                        auto orders = result.as<vector<extended_limit_order>>();

                        std::stringstream ss;

                        ss << setiosflags(ios::fixed) << setiosflags(ios::left);
                        ss << ' ' << setw(10) << "Order #";
                        ss << ' ' << setw(10) << "Price";
                        ss << ' ' << setw(10) << "Quantity";
                        ss << ' ' << setw(10) << "Type";
                        ss
                                << "\n=====================================================================================================\n";
                        for (const auto &o : orders) {
                            ss << ' ' << setw(10) << o.order_id;
                            ss << ' ' << setw(10) << o.real_price;
                            ss << ' ' << setw(10)
                               << fc::variant(asset<0, 17, 0>(o.for_sale, o.sell_price.base.symbol)).as_string();
                            ss << ' ' << setw(10) << (o.sell_price.base.symbol == STEEM_SYMBOL_NAME ? "SELL" : "BUY");
                            ss << "\n";
                        }
                        return ss.str();
                    };
                    m["get_order_book"] = [this](variant result, const fc::variants &a) {
                        auto orders = result.as<market_history::order_book>();
                        auto bids = orders.bids;
                        auto asks = orders.asks;
                        std::stringstream ss;
                        std::stringstream sum_stream;
                        sum_stream << "Sum(" << orders.base << ')';
                        double bid_sum = 0;
                        double ask_sum = 0;
                        const int spacing = 20;

                        auto prettify_num = [&](double n) {
                            //ss << n;
                            if (abs(round(n) - n) < 0.00000000001) {
                                //ss << setiosflags( !ios::fixed ) << (int) n;     // doesn't compile on Linux with gcc
                                ss << (int) n;
                            } else if (n - floor(n) < 0.000001) {
                                ss << setiosflags(ios::fixed) << setprecision(10) << n;
                            } else {
                                ss << setiosflags(ios::fixed) << setprecision(6) << n;
                            }
                        };

                        ss << setprecision(8) << setiosflags(ios::fixed) << setiosflags(ios::left);

                        ss << ' ' << setw((spacing * 4) + 6) << "BUY ORDERS" << "SELL ORDERS\n" << ' '
                           << setw(spacing + 1) << "Price" << setw(spacing) << orders.quote << ' ' << setw(spacing)
                           << orders.base << ' ' << setw(spacing) << sum_stream.str() << "   " << setw(spacing + 1)
                           << "Price" << setw(spacing) << orders.quote << ' ' << setw(spacing) << orders.base << ' '
                           << setw(spacing) << sum_stream.str()
                           << "\n====================================================================================="
                           << "|=====================================================================================\n";

                        for (int i = 0; i < bids.size() || i < asks.size(); i++) {
                            if (i < bids.size()) {
                                bid_sum += bids[i].base;
                                ss << ' ' << setw(spacing);
                                prettify_num(bids[i].price);
                                ss << ' ' << setw(spacing);
                                prettify_num(bids[i].quote);
                                ss << ' ' << setw(spacing);
                                prettify_num(bids[i].base);
                                ss << ' ' << setw(spacing);
                                prettify_num(bid_sum);
                                ss << ' ';
                            } else {
                                ss << setw((spacing * 4) + 5) << ' ';
                            }

                            ss << '|';

                            if (i < asks.size()) {
                                ask_sum += asks[i].base;
                                ss << ' ' << setw(spacing);
                                prettify_num(asks[i].price);
                                ss << ' ' << setw(spacing);
                                prettify_num(asks[i].quote);
                                ss << ' ' << setw(spacing);
                                prettify_num(asks[i].base);
                                ss << ' ' << setw(spacing);
                                prettify_num(ask_sum);
                            }

                            ss << '\n';
                        }

                        ss << endl << "Buy Total:  " << bid_sum << ' ' << orders.base << endl << "Sell Total: "
                           << ask_sum << ' ' << orders.base << endl;

                        return ss.str();
                    };
                    m["get_withdraw_routes"] = [](variant result, const fc::variants &a) {
                        auto routes = result.as<vector<withdraw_route>>();
                        std::stringstream ss;

                        ss << ' ' << std::left << std::setw(20) << "From";
                        ss << ' ' << std::left << std::setw(20) << "To";
                        ss << ' ' << std::right << std::setw(8) << "Percent";
                        ss << ' ' << std::right << std::setw(9) << "Auto-Vest";
                        ss << "\n==============================================================\n";

                        for (auto r : routes) {
                            ss << ' ' << std::left << std::setw(20) << r.from_account;
                            ss << ' ' << std::left << std::setw(20) << r.to_account;
                            ss << ' ' << std::right << std::setw(8) << std::setprecision(2) << std::fixed
                               << double(r.percent) / 100;
                            ss << ' ' << std::right << std::setw(9) << (r.auto_vest ? "true" : "false") << std::endl;
                        }

                        return ss.str();
                    };

                    return m;
                }

                void use_network_node_api() {
                    if (_remote_net_node) {
                        return;
                    }
                    try {
                        _remote_net_node = _remote_api->get_api_by_name("network_node_api")->as<network_node_api>();
                    } catch (const fc::exception &e) {
                        elog("Couldn't get network node API");
                        throw (e);
                    }
                }

                void use_remote_message_api() {
                    if (_remote_message_api.valid()) {
                        return;
                    }

                    try {
                        _remote_message_api = _remote_api->get_api_by_name("private_message_api")->as<
                                private_message_api>();
                    } catch (const fc::exception &e) {
                        elog("Couldn't get private message API");
                        throw (e);
                    }
                }

                void use_follow_api() {
                    if (_remote_follow_api.valid()) {
                        return;
                    }

                    try {
                        _remote_follow_api = _remote_api->get_api_by_name("follow_api")->as<follow::follow_api>();
                    } catch (const fc::exception &e) {
                        elog("Couldn't get follow API");
                        throw (e);
                    }
                }

                void use_remote_account_by_key_api() {
                    if (_remote_account_by_key_api.valid()) {
                        return;
                    }

                    try {
                        _remote_account_by_key_api = _remote_api->get_api_by_name("account_by_key_api")->as<
                                account_by_key::account_by_key_api>();
                    } catch (const fc::exception &e) {
                        elog("Couldn't get account_by_key API");
                        throw (e);
                    }
                }

                void use_remote_market_history_api() {
                    if (_remote_market_history_api.valid()) {
                        return;
                    }

                    try {
                        _remote_market_history_api = _remote_api->get_api_by_name("market_history_api")->as<
                                market_history::market_history_api>();
                    } catch (const fc::exception &e) {
                        elog("Couldn't get market_history API");
                        throw (e);
                    }
                }

                void network_add_nodes(const vector<string> &nodes) {
                    use_network_node_api();
                    for (const string &node_address : nodes) {
                        (*_remote_net_node)->add_node(fc::ip::endpoint::from_string(node_address));
                    }
                }

                vector<variant> network_get_connected_peers() {
                    use_network_node_api();
                    const auto peers = (*_remote_net_node)->get_connected_peers();
                    vector<variant> result;
                    result.reserve(peers.size());
                    for (const auto &peer : peers) {
                        variant v;
                        fc::to_variant(peer, v);
                        result.push_back(v);
                    }
                    return result;
                }

                operation get_prototype_operation(string operation_name) {
                    auto it = _prototype_ops.find(operation_name);
                    if (it == _prototype_ops.end())
                        FC_THROW("Unsupported operation: \"${operation_name}\"", ("operation_name", operation_name));
                    return it->second;
                }

                transaction_handle_type begin_builder_transaction() {
                    int trx_handle = _builder_transactions.empty() ? 0 : (--_builder_transactions.end())->first + 1;
                    _builder_transactions[trx_handle];
                    return trx_handle;
                }

                void add_operation_to_builder_transaction(transaction_handle_type transaction_handle,
                                                          const operation &op) {
                    FC_ASSERT(_builder_transactions.count(transaction_handle));
                    _builder_transactions[transaction_handle].operations.emplace_back(op);
                }

                void replace_operation_in_builder_transaction(transaction_handle_type handle, uint32_t operation_index,
                                                              const operation &new_op) {
                    FC_ASSERT(_builder_transactions.count(handle));
                    signed_transaction &trx = _builder_transactions[handle];
                    FC_ASSERT(operation_index < trx.operations.size());
                    trx.operations[operation_index] = new_op;
                }

                transaction preview_builder_transaction(transaction_handle_type handle) {
                    FC_ASSERT(_builder_transactions.count(handle));
                    return _builder_transactions[handle];
                }

                signed_transaction sign_builder_transaction(transaction_handle_type transaction_handle,
                                                            bool broadcast = true) {
                    FC_ASSERT(_builder_transactions.count(transaction_handle));

                    return _builder_transactions[transaction_handle] = sign_transaction(
                            _builder_transactions[transaction_handle], broadcast);
                }

                signed_transaction propose_builder_transaction(transaction_handle_type handle,
                                                               time_point_sec expiration = time_point::now() +
                                                                                           fc::minutes(1),
                                                               uint32_t review_period_seconds = 0,
                                                               bool broadcast = true) {
                    FC_ASSERT(_builder_transactions.count(handle));
                    proposal_create_operation<0, 17, 0> op;
                    op.expiration_time = expiration;
                    signed_transaction &trx = _builder_transactions[handle];
                    std::transform(trx.operations.begin(), trx.operations.end(),
                                   std::back_inserter(op.proposed_operations),
                                   [](const operation &op) -> operation_wrapper {
                                       return op;
                                   });
                    if (review_period_seconds) {
                        op.review_period_seconds = review_period_seconds;
                    }
                    trx.operations = {op};

                    return trx = sign_transaction(trx, broadcast);
                }

                signed_transaction propose_builder_transaction2(transaction_handle_type handle,
                                                                string account_name_or_id,
                                                                time_point_sec expiration = time_point::now() +
                                                                                            fc::minutes(1),
                                                                uint32_t review_period_seconds = 0,
                                                                bool broadcast = true) {
                    FC_ASSERT(_builder_transactions.count(handle));
                    proposal_create_operation<0, 17, 0> op;
                    op.owner = get_account(account_name_or_id).name;
                    op.expiration_time = expiration;
                    signed_transaction &trx = _builder_transactions[handle];
                    std::transform(trx.operations.begin(), trx.operations.end(),
                                   std::back_inserter(op.proposed_operations),
                                   [](const operation &op) -> operation_wrapper {
                                       return op;
                                   });
                    if (review_period_seconds) {
                        op.review_period_seconds = review_period_seconds;
                    }
                    trx.operations = {op};
                    return trx = sign_transaction(trx, broadcast);
                }

                void remove_builder_transaction(transaction_handle_type handle) {
                    _builder_transactions.erase(handle);
                }

                signed_transaction bid_collateral(string bidder_name, string debt_amount, string debt_symbol,
                                                  string additional_collateral, bool broadcast) {
                    try {
                        optional<asset_object> debt_asset = find_asset(debt_symbol);
                        if (!debt_asset)
                            FC_THROW("No asset with that symbol exists!");
                        const asset_object &collateral = get_asset(
                                get_bitasset_data(debt_asset->asset_name).options.short_backing_asset);

                        bid_collateral_operation<0, 17, 0> op;
                        op.bidder = get_account(bidder_name).name;
                        op.debt_covered = debt_asset->amount_from_string(debt_amount);
                        op.additional_collateral = collateral.amount_from_string(additional_collateral);

                        signed_transaction tx;
                        tx.operations.push_back(op);
                        tx.validate();

                        return sign_transaction(tx, broadcast);
                    } FC_CAPTURE_AND_RETHROW((bidder_name)(debt_amount)(debt_symbol)(additional_collateral)(broadcast))
                }

                string _wallet_filename;
                wallet_data _wallet;

                map<public_key_type, string> _keys;
                map<transaction_handle_type, signed_transaction> _builder_transactions;

                fc::sha512 _checksum;
                fc::api<login_api> _remote_api;
                fc::api<database_api> _remote_db;
                fc::api<network_broadcast_api> _remote_net_broadcast;
                optional<fc::api<network_node_api>> _remote_net_node;
                optional<fc::api<account_by_key::account_by_key_api>> _remote_account_by_key_api;
                optional<fc::api<market_history::market_history_api>> _remote_market_history_api;
                optional<fc::api<private_message_api>> _remote_message_api;
                optional<fc::api<follow::follow_api>> _remote_follow_api;
                uint32_t _tx_expiration_seconds = 30;

                flat_map<string, operation> _prototype_ops;

                static_variant_map _operation_which_map = create_static_variant_map<operation>();

#ifdef __unix__
                mode_t                  _old_umask;
#endif
                const string _wallet_filename_extension = ".wallet";

                mutable map<asset_name_type, asset_object> _asset_cache;
            };
        }
    }
} // golos::wallet::detail



namespace golos {
    namespace wallet {

        wallet_api::wallet_api(const wallet_data &initial_data, fc::api<login_api> rapi) : my(
                new detail::wallet_api_impl(*this, initial_data, rapi)) {
        }

        wallet_api::~wallet_api() {
        }

        bool wallet_api::copy_wallet_file(string destination_filename) {
            return my->copy_wallet_file(destination_filename);
        }

        optional<signed_block_with_info> wallet_api::get_block(uint32_t num) {
            return my->_remote_db->get_block(num);
        }

        vector<applied_operation> wallet_api::get_ops_in_block(uint32_t block_num, bool only_virtual) {
            return my->_remote_db->get_ops_in_block(block_num, only_virtual);
        }

        string wallet_api::get_account_count() const {
            return std::to_string(my->_remote_db->get_account_count());
        }

        string wallet_api::get_steem_per_mvests() const {
            auto dynamic_props = my->_remote_db->get_dynamic_global_properties();
            auto price = (dynamic_props.total_vesting_fund_steem / dynamic_props.total_vesting_shares);
            return std::to_string(price.to_real() * 1000000);
        }

        vector<account_api_obj> wallet_api::list_my_accounts() const {
            FC_ASSERT(!is_locked(), "Wallet must be unlocked to list accounts");
            vector<account_api_obj> result;

            try {
                my->use_remote_account_by_key_api();
            } catch (fc::exception &e) {
                elog("Connected node needs to enable account_by_key_api");
                return result;
            }

            vector<public_key_type> pub_keys;
            pub_keys.reserve(my->_keys.size());

            for (const auto &item : my->_keys) {
                pub_keys.push_back(item.first);
            }

            auto refs = (*my->_remote_account_by_key_api)->get_key_references(pub_keys);
            set<string> names;
            for (const auto &item : refs) {
                for (const auto &name : item) {
                    names.insert(name);
                }
            }


            result.reserve(names.size());
            for (const auto &name : names) {
                result.emplace_back(get_account(name));
            }

            return result;
        }

        set<string> wallet_api::list_accounts(const string &lowerbound, uint32_t limit) {
            return my->_remote_db->lookup_accounts(lowerbound, limit);
        }

        vector<asset<0, 17, 0>> wallet_api::list_account_balances(const account_name_type &account_name) {
            return my->_remote_db->get_account_balances(get_account(account_name).name, flat_set<asset_name_type>());
        }

        vector<asset_object> wallet_api::list_assets(const string &lowerbound, uint32_t limit) const {
            return my->_remote_db->list_assets(lowerbound, limit);
        }

        dynamic_global_property_object wallet_api::get_dynamic_global_properties() const {
            return my->_remote_db->get_dynamic_global_properties();
        }

        vector<account_name_type> wallet_api::get_miner_queue() const {
            return my->_remote_db->get_miner_queue();
        }

        std::vector<account_name_type> wallet_api::get_active_witnesses() const {
            return my->_remote_db->get_active_witnesses();
        }

        vector<collateral_bid_object> wallet_api::get_collateral_bids(string asset, uint32_t limit,
                                                                      uint32_t start) const {
            try {
                my->use_remote_market_history_api();
            } catch (fc::exception &e) {
                elog("Connected node needs to enable market_history");
                return {};
            }

            return (*my->_remote_market_history_api)->get_collateral_bids(get_asset(asset).asset_name, limit, start);
        }

        signed_transaction wallet_api::bid_collateral(string bidder_name, string debt_amount, string debt_symbol,
                                                      string additional_collateral, bool broadcast) {
            return my->bid_collateral(bidder_name, debt_amount, debt_symbol, additional_collateral, broadcast);
        }

        brain_key_info wallet_api::suggest_brain_key() const {
            brain_key_info result;
            // create a private key for secure entropy
            fc::sha256 sha_entropy1 = fc::ecc::private_key::generate().get_secret();
            fc::sha256 sha_entropy2 = fc::ecc::private_key::generate().get_secret();
            fc::bigint entropy1(sha_entropy1.data(), sha_entropy1.data_size());
            fc::bigint entropy2(sha_entropy2.data(), sha_entropy2.data_size());
            fc::bigint entropy(entropy1);
            entropy <<= 8 * sha_entropy1.data_size();
            entropy += entropy2;
            string brain_key = "";

            for (int i = 0; i < BRAIN_KEY_WORD_COUNT; i++) {
                fc::bigint choice = entropy % graphene::words::word_list_size;
                entropy /= graphene::words::word_list_size;
                if (i > 0) {
                    brain_key += " ";
                }
                brain_key += graphene::words::word_list[choice.to_int64()];
            }

            brain_key = normalize_brain_key(brain_key);
            fc::ecc::private_key priv_key = detail::derive_private_key(brain_key, 0);
            result.brain_priv_key = brain_key;
            result.wif_priv_key = key_to_wif(priv_key);
            result.pub_key = priv_key.get_public_key();
            return result;
        }

        string wallet_api::serialize_transaction(signed_transaction tx) const {
            return fc::to_hex(fc::raw::pack(tx));
        }

        string wallet_api::get_wallet_filename() const {
            return my->get_wallet_filename();
        }

        transaction_handle_type wallet_api::begin_builder_transaction() {
            return my->begin_builder_transaction();
        }

        void wallet_api::add_operation_to_builder_transaction(transaction_handle_type transaction_handle,
                                                              const operation &op) {
            my->add_operation_to_builder_transaction(transaction_handle, op);
        }

        void wallet_api::replace_operation_in_builder_transaction(transaction_handle_type handle,
                                                                  unsigned operation_index, const operation &new_op) {
            my->replace_operation_in_builder_transaction(handle, operation_index, new_op);
        }

        transaction wallet_api::preview_builder_transaction(transaction_handle_type handle) {
            return my->preview_builder_transaction(handle);
        }

        signed_transaction wallet_api::sign_builder_transaction(transaction_handle_type transaction_handle,
                                                                bool broadcast) {
            return my->sign_builder_transaction(transaction_handle, broadcast);
        }

        signed_transaction wallet_api::propose_builder_transaction(transaction_handle_type handle,
                                                                   time_point_sec expiration,
                                                                   uint32_t review_period_seconds, bool broadcast) {
            return my->propose_builder_transaction(handle, expiration, review_period_seconds, broadcast);
        }

        signed_transaction wallet_api::propose_builder_transaction2(transaction_handle_type handle,
                                                                    string account_name_or_id,
                                                                    time_point_sec expiration,
                                                                    uint32_t review_period_seconds, bool broadcast) {
            return my->propose_builder_transaction2(handle, account_name_or_id, expiration, review_period_seconds,
                                                    broadcast);
        }

        void wallet_api::remove_builder_transaction(transaction_handle_type handle) {
            return my->remove_builder_transaction(handle);
        }

        account_api_obj wallet_api::get_account(string account_name) const {
            return my->get_account(account_name);
        }

        asset_object wallet_api::get_asset(string asset_symbol) const {
            auto a = my->_remote_db->get_assets({asset_name_type(asset_symbol)}).front();

            FC_ASSERT(a);
            return *a;
        }

        asset_bitasset_data_object wallet_api::get_bitasset_data(string asset_symbol) const {
            return my->get_bitasset_data(asset_symbol);
        }

        proposal_object wallet_api::get_proposal(string account_name, integral_id_type id) const {
            return my->get_proposal(account_name, id);
        }

        bool wallet_api::import_key(string wif_key) {
            FC_ASSERT(!is_locked());
            // backup wallet
            fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
            if (!optional_private_key)
                FC_THROW("Invalid private key");
            //   string shorthash = detail::pubkey_to_shorthash( optional_private_key->get_public_key() );
            //   copy_wallet_file( "before-import-key-" + shorthash );

            if (my->import_key(wif_key)) {
                save_wallet_file();
                //     copy_wallet_file( "after-import-key-" + shorthash );
                return true;
            }
            return false;
        }

        string wallet_api::normalize_brain_key(string s) const {
            return detail::normalize_brain_key(s);
        }

        variant wallet_api::info() {
            return my->info();
        }

        variant_object wallet_api::about() const {
            return my->about();
        }

        /*
           fc::ecc::private_key wallet_api::derive_private_key(const std::string& prefix_string, int sequence_number) const
           {
           return detail::derive_private_key( prefix_string, sequence_number );
           }
           */

        set<account_name_type> wallet_api::list_witnesses(const string &lowerbound, uint32_t limit) {
            return my->_remote_db->lookup_witness_accounts(lowerbound, limit);
        }

        optional<witness_api_obj> wallet_api::get_witness(string owner_account) {
            return my->get_witness(owner_account);
        }

        annotated_signed_transaction wallet_api::set_voting_proxy(string account_to_modify, string voting_account,
                                                                  bool broadcast /* = false */) {
            return my->set_voting_proxy(account_to_modify, voting_account, broadcast);
        }

        void wallet_api::set_wallet_filename(string wallet_filename) {
            my->_wallet_filename = wallet_filename;
        }

        annotated_signed_transaction wallet_api::sign_transaction(signed_transaction tx, bool broadcast /* = false */) {
            try {
                return my->sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((tx))
        }

        operation wallet_api::get_prototype_operation(string operation_name) {
            return my->get_prototype_operation(operation_name);
        }

        void wallet_api::network_add_nodes(const vector<string> &nodes) {
            my->network_add_nodes(nodes);
        }

        vector<variant> wallet_api::network_get_connected_peers() {
            return my->network_get_connected_peers();
        }

        string wallet_api::help() const {
            std::vector<std::string> method_names = my->method_documentation.get_method_names();
            std::stringstream ss;
            for (const std::string method_name : method_names) {
                try {
                    ss << my->method_documentation.get_brief_description(method_name);
                } catch (const fc::key_not_found_exception &) {
                    ss << method_name << " (no help available)\n";
                }
            }
            return ss.str();
        }

        string wallet_api::get_help(const string &method) const {
            fc::api<wallet_api> tmp;
            std::stringstream ss;
            ss << "\n";

            std::string doxygenHelpString = my->method_documentation.get_detailed_description(method);
            if (!doxygenHelpString.empty()) {
                ss << doxygenHelpString;
            } else {
                ss << "No help defined for method " << method << "\n";
            }

            return ss.str();
        }

        bool wallet_api::load_wallet_file(string wallet_filename) {
            return my->load_wallet_file(wallet_filename);
        }

        void wallet_api::save_wallet_file(string wallet_filename) {
            my->save_wallet_file(wallet_filename);
        }

        std::map<string,
                std::function<string(fc::variant, const fc::variants &)>> wallet_api::get_result_formatters() const {
            return my->get_result_formatters();
        }

        bool wallet_api::is_locked() const {
            return my->is_locked();
        }

        bool wallet_api::is_new() const {
            return my->_wallet.cipher_keys.size() == 0;
        }

        void wallet_api::encrypt_keys() {
            my->encrypt_keys();
        }

        void wallet_api::lock() {
            try {
                FC_ASSERT(!is_locked());
                encrypt_keys();
                for (auto key : my->_keys) {
                    key.second = key_to_wif(fc::ecc::private_key());
                }
                my->_keys.clear();
                my->_checksum = fc::sha512();
                my->self.lock_changed(true);
            } FC_CAPTURE_AND_RETHROW()
        }

        void wallet_api::unlock(string password) {
            try {
                FC_ASSERT(password.size() > 0);
                auto pw = fc::sha512::hash(password.c_str(), password.size());
                vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
                auto pk = fc::raw::unpack<plain_keys>(decrypted);
                FC_ASSERT(pk.checksum == pw);
                my->_keys = std::move(pk.keys);
                my->_checksum = pk.checksum;
                my->self.lock_changed(false);
            } FC_CAPTURE_AND_RETHROW()
        }

        void wallet_api::set_password(string password) {
            if (!is_new())
                FC_ASSERT(!is_locked(), "The wallet must be unlocked before the password can be set");
            my->_checksum = fc::sha512::hash(password.c_str(), password.size());
            lock();
        }

        map<public_key_type, string> wallet_api::list_keys() {
            FC_ASSERT(!is_locked());
            return my->_keys;
        }

        string wallet_api::get_private_key(public_key_type pubkey) const {
            return key_to_wif(my->get_private_key(pubkey));
        }

        pair<public_key_type, string> wallet_api::get_private_key_from_password(string account, string role,
                                                                                string password) const {
            auto seed = account + role + password;
            FC_ASSERT(seed.size());
            auto secret = fc::sha256::hash(seed.c_str(), seed.size());
            auto priv = fc::ecc::private_key::regenerate(secret);
            return std::make_pair(public_key_type(priv.get_public_key()), key_to_wif(priv));
        }

        signed_block_with_info::signed_block_with_info(const signed_block &block) : signed_block(block) {
            block_id = id();
            signing_key = signee();
            transaction_ids.reserve(transactions.size());
            for (const signed_transaction &tx : transactions) {
                transaction_ids.push_back(tx.id());
            }
        }

        feed_history_api_obj wallet_api::get_feed_history() const {
            return my->_remote_db->get_feed_history();
        }

        /**
         * This method is used by faucets to create new accounts for other users which must
         * provide their desired keys. The resulting account may not be controllable by this
         * wallet.
         */
        annotated_signed_transaction wallet_api::create_account_with_keys(string creator, string new_account_name,
                                                                          string json_meta, public_key_type owner,
                                                                          public_key_type active,
                                                                          public_key_type posting, public_key_type memo,
                                                                          bool broadcast) const {
            try {
                FC_ASSERT(!is_locked());
                account_create_operation<0, 17, 0> op;
                op.creator = creator;
                op.new_account_name = new_account_name;
                op.owner = authority(1, owner, 1);
                op.active = authority(1, active, 1);
                op.posting = authority(1, posting, 1);
                op.memo_key = memo;
                op.json_metadata = json_meta;
                op.fee = asset<0, 17, 0>(my->_remote_db->get_chain_properties().account_creation_fee.amount *
                                         STEEMIT_CREATE_ACCOUNT_WITH_STEEM_MODIFIER, STEEM_SYMBOL_NAME);

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();

                return my->sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta)(owner)(active)(memo)(broadcast))
        }

        /**
         * This method is used by faucets to create new accounts for other users which must
         * provide their desired keys. The resulting account may not be controllable by this
         * wallet.
         */
        annotated_signed_transaction wallet_api::create_account_with_keys_delegated(string creator,

                                                                                    asset<0, 17, 0> steem_fee,
                                                                                    asset<0, 17, 0> delegated_vests,
                                                                                    string new_account_name,
                                                                                    string json_meta,
                                                                                    public_key_type owner,
                                                                                    public_key_type active,
                                                                                    public_key_type posting,
                                                                                    public_key_type memo,
                                                                                    bool broadcast) const {
            try {
                FC_ASSERT(!is_locked());
                account_create_with_delegation_operation<0, 17, 0> op;
                op.creator = creator;
                op.new_account_name = new_account_name;
                op.owner = authority(1, owner, 1);
                op.active = authority(1, active, 1);
                op.posting = authority(1, posting, 1);
                op.memo_key = memo;
                op.json_metadata = std::move(json_meta);
                op.fee = steem_fee;
                op.delegation = delegated_vests;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();

                return my->sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta)(owner)(active)(memo)(broadcast))
        }

        annotated_signed_transaction wallet_api::request_account_recovery(string recovery_account,
                                                                          string account_to_recover,
                                                                          authority new_authority, bool broadcast) {
            FC_ASSERT(!is_locked());
            request_account_recovery_operation<0, 17, 0> op;
            op.recovery_account = recovery_account;
            op.account_to_recover = account_to_recover;
            op.new_owner_authority = new_authority;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::recover_account(string account_to_recover, authority recent_authority,
                                                                 authority new_authority, bool broadcast) {
            FC_ASSERT(!is_locked());

            recover_account_operation<0, 17, 0> op;
            op.account_to_recover = account_to_recover;
            op.new_owner_authority = new_authority;
            op.recent_owner_authority = recent_authority;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::change_recovery_account(string owner, string new_recovery_account,
                                                                         bool broadcast) {
            FC_ASSERT(!is_locked());

            change_recovery_account_operation<0, 17, 0> op;
            op.account_to_recover = owner;
            op.new_recovery_account = new_recovery_account;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        vector<owner_authority_history_api_obj> wallet_api::get_owner_history(string account) const {
            return my->_remote_db->get_owner_history(account);
        }

        annotated_signed_transaction wallet_api::update_account(string account_name, string json_meta,
                                                                public_key_type owner, public_key_type active,
                                                                public_key_type posting, public_key_type memo,
                                                                bool broadcast) const {
            try {
                FC_ASSERT(!is_locked());

                account_update_operation<0, 17, 0> op;
                op.account = account_name;
                op.owner = authority(1, owner, 1);
                op.active = authority(1, active, 1);
                op.posting = authority(1, posting, 1);
                op.memo_key = memo;
                op.json_metadata = json_meta;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();

                return my->sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((account_name)(json_meta)(owner)(active)(memo)(broadcast))
        }

        annotated_signed_transaction wallet_api::update_account_auth_key(string account_name, authority_type type,
                                                                         public_key_type key, weight_type weight,
                                                                         bool broadcast) {
            FC_ASSERT(!is_locked());

            auto accounts = my->_remote_db->get_accounts({account_name});
            FC_ASSERT(accounts.size() == 1, "Account does not exist");
            FC_ASSERT(account_name == accounts[0].name, "Account name doesn't match?");

            account_update_operation<0, 17, 0> op;
            op.account = account_name;
            op.memo_key = accounts[0].memo_key;
            op.json_metadata = accounts[0].json_metadata;

            authority new_auth;

            switch (type) {
                case (owner):
                    new_auth = accounts[0].owner;
                    break;
                case (active):
                    new_auth = accounts[0].active;
                    break;
                case (posting):
                    new_auth = accounts[0].posting;
                    break;
            }

            if (weight == 0) // Remove the key
            {
                new_auth.key_auths.erase(key);
            } else {
                new_auth.add_authority(key, weight);
            }

            if (new_auth.is_impossible()) {
                if (type == owner) {
                    FC_ASSERT(false, "Owner authority change would render account irrecoverable.");
                }

                wlog("Authority is now impossible.");
            }

            switch (type) {
                case (owner):
                    op.owner = new_auth;
                    break;
                case (active):
                    op.active = new_auth;
                    break;
                case (posting):
                    op.posting = new_auth;
                    break;
            }

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::update_account_auth_account(string account_name, authority_type type,
                                                                             string auth_account, weight_type weight,
                                                                             bool broadcast) {
            FC_ASSERT(!is_locked());

            auto accounts = my->_remote_db->get_accounts({account_name});
            FC_ASSERT(accounts.size() == 1, "Account does not exist");
            FC_ASSERT(account_name == accounts[0].name, "Account name doesn't match?");

            account_update_operation<0, 17, 0> op;
            op.account = account_name;
            op.memo_key = accounts[0].memo_key;
            op.json_metadata = accounts[0].json_metadata;

            authority new_auth;

            switch (type) {
                case (owner):
                    new_auth = accounts[0].owner;
                    break;
                case (active):
                    new_auth = accounts[0].active;
                    break;
                case (posting):
                    new_auth = accounts[0].posting;
                    break;
            }

            if (weight == 0) // Remove the key
            {
                new_auth.account_auths.erase(auth_account);
            } else {
                new_auth.add_authority(auth_account, weight);
            }

            if (new_auth.is_impossible()) {
                if (type == owner) {
                    FC_ASSERT(false, "Owner authority change would render account irrecoverable.");
                }

                wlog("Authority is now impossible.");
            }

            switch (type) {
                case (owner):
                    op.owner = new_auth;
                    break;
                case (active):
                    op.active = new_auth;
                    break;
                case (posting):
                    op.posting = new_auth;
                    break;
            }

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::update_account_auth_threshold(string account_name, authority_type type,
                                                                               uint32_t threshold, bool broadcast) {
            FC_ASSERT(!is_locked());

            auto accounts = my->_remote_db->get_accounts({account_name});
            FC_ASSERT(accounts.size() == 1, "Account does not exist");
            FC_ASSERT(account_name == accounts[0].name, "Account name doesn't match?");
            FC_ASSERT(threshold != 0, "Authority is implicitly satisfied");

            account_update_operation<0, 17, 0> op;
            op.account = account_name;
            op.memo_key = accounts[0].memo_key;
            op.json_metadata = accounts[0].json_metadata;

            authority new_auth;

            switch (type) {
                case (owner):
                    new_auth = accounts[0].owner;
                    break;
                case (active):
                    new_auth = accounts[0].active;
                    break;
                case (posting):
                    new_auth = accounts[0].posting;
                    break;
            }

            new_auth.weight_threshold = threshold;

            if (new_auth.is_impossible()) {
                if (type == owner) {
                    FC_ASSERT(false, "Owner authority change would render account irrecoverable.");
                }

                wlog("Authority is now impossible.");
            }

            switch (type) {
                case (owner):
                    op.owner = new_auth;
                    break;
                case (active):
                    op.active = new_auth;
                    break;
                case (posting):
                    op.posting = new_auth;
                    break;
            }

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::update_account_meta(string account_name, string json_meta,
                                                                     bool broadcast) {
            FC_ASSERT(!is_locked());

            auto accounts = my->_remote_db->get_accounts({account_name});
            FC_ASSERT(accounts.size() == 1, "Account does not exist");
            FC_ASSERT(account_name == accounts[0].name, "Account name doesn't match?");

            account_update_operation<0, 17, 0> op;
            op.account = account_name;
            op.memo_key = accounts[0].memo_key;
            op.json_metadata = json_meta;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::update_account_memo_key(string account_name, public_key_type key,
                                                                         bool broadcast) {
            FC_ASSERT(!is_locked());

            auto accounts = my->_remote_db->get_accounts({account_name});
            FC_ASSERT(accounts.size() == 1, "Account does not exist");
            FC_ASSERT(account_name == accounts[0].name, "Account name doesn't match?");

            account_update_operation<0, 17, 0> op;
            op.account = account_name;
            op.memo_key = key;
            op.json_metadata = accounts[0].json_metadata;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::delegate_vesting_shares(string delegator, string delegatee,
                                                                         asset<0, 17, 0> vesting_shares,
                                                                         bool broadcast) {
            FC_ASSERT(!is_locked());

            auto accounts = my->_remote_db->get_accounts({delegator, delegatee});
            FC_ASSERT(accounts.size() == 2, "One or more of the accounts specified do not exist.");
            FC_ASSERT(delegator == accounts[0].name, "Delegator account is not right?");
            FC_ASSERT(delegatee == accounts[1].name, "Delegator account is not right?");

            delegate_vesting_shares_operation<0, 17, 0> op;
            op.delegator = delegator;
            op.delegatee = delegatee;
            op.vesting_shares = vesting_shares;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        /**
         *  This method will genrate new owner, active, and memo keys for the new account which
         *  will be controlable by this wallet.
         */
        annotated_signed_transaction wallet_api::create_account(string creator, string new_account_name,
                                                                string json_meta, bool broadcast) {
            try {
                FC_ASSERT(!is_locked());
                auto owner = suggest_brain_key();
                auto active = suggest_brain_key();
                auto posting = suggest_brain_key();
                auto memo = suggest_brain_key();
                import_key(owner.wif_priv_key);
                import_key(active.wif_priv_key);
                import_key(posting.wif_priv_key);
                import_key(memo.wif_priv_key);
                return create_account_with_keys(creator, new_account_name, json_meta, owner.pub_key, active.pub_key,
                                                posting.pub_key, memo.pub_key, broadcast);
            } FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta))
        }

        /**
         *  This method will genrate new owner, active, and memo keys for the new account which
         *  will be controllable by this wallet.
         */
        annotated_signed_transaction wallet_api::create_account_delegated(string creator, asset<0, 17, 0> steem_fee,
                                                                          asset<0, 17, 0> delegated_vests,
                                                                          string new_account_name, string json_meta,
                                                                          bool broadcast) {
            try {
                FC_ASSERT(!is_locked());
                auto owner = suggest_brain_key();
                auto active = suggest_brain_key();
                auto posting = suggest_brain_key();
                auto memo = suggest_brain_key();
                import_key(owner.wif_priv_key);
                import_key(active.wif_priv_key);
                import_key(posting.wif_priv_key);
                import_key(memo.wif_priv_key);
                return create_account_with_keys_delegated(creator, std::move(steem_fee), std::move(delegated_vests),
                                                          new_account_name, json_meta, owner.pub_key, active.pub_key,
                                                          posting.pub_key, memo.pub_key, broadcast);
            } FC_CAPTURE_AND_RETHROW((creator)(new_account_name)(json_meta))
        }


        annotated_signed_transaction wallet_api::update_witness(string witness_account_name, string url,
                                                                public_key_type block_signing_key,
                                                                const chain_properties<0, 17, 0> &props,
                                                                bool broadcast) {
            FC_ASSERT(!is_locked());

            witness_update_operation<0, 17, 0> op;

            fc::optional<witness_api_obj> wit = my->_remote_db->get_witness_by_account(witness_account_name);
            if (!wit.valid()) {
                op.url = url;
            } else {
                FC_ASSERT(wit->owner == witness_account_name);
                if (url != "") {
                    op.url = url;
                } else {
                    op.url = wit->url;
                }
            }
            op.owner = witness_account_name;
            op.block_signing_key = block_signing_key;
            op.props = props;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::vote_for_witness(string voting_account, string witness_to_vote_for,
                                                                  bool approve, bool broadcast) {
            try {
                FC_ASSERT(!is_locked());
                account_witness_vote_operation<0, 17, 0> op;
                op.account = voting_account;
                op.witness = witness_to_vote_for;
                op.approve = approve;

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();

                return my->sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((voting_account)(witness_to_vote_for)(approve)(broadcast))
        }

        string wallet_api::get_encrypted_memo(string from, string to, string memo) {

            if (memo.size() > 0 && memo[0] == '#') {
                memo_data m;

                auto from_account = get_account(from);
                auto to_account = get_account(to);

                m.from = from_account.memo_key;
                m.to = to_account.memo_key;
                m.nonce = fc::time_point::now().time_since_epoch().count();

                auto from_priv = my->get_private_key(m.from);
                auto shared_secret = from_priv.get_shared_secret(m.to);

                fc::sha512::encoder enc;
                fc::raw::pack(enc, m.nonce);
                fc::raw::pack(enc, shared_secret);
                auto encrypt_key = enc.result();

                m.encrypted = fc::aes_encrypt(encrypt_key, fc::raw::pack(memo.substr(1)));
                m.check = fc::sha256::hash(encrypt_key)._hash[0];
                return m;
            } else {
                return memo;
            }
        }

        annotated_signed_transaction wallet_api::transfer(string from, string to, asset<0, 17, 0> amount, string memo,
                                                          bool broadcast) {
            try {
                FC_ASSERT(!is_locked());
                fc::optional<asset_object> asset_obj = get_asset(amount.symbol);
                FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", amount.symbol));

                transfer_operation<0, 17, 0> op;
                op.from = from;
                op.to = to;
                op.amount = amount;

                op.memo = get_encrypted_memo(from, to, memo);

                signed_transaction tx;
                tx.operations.push_back(op);
                tx.validate();

                return my->sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((from)(to)(amount)(memo)(broadcast))
        }

        annotated_signed_transaction wallet_api::escrow_transfer(string from, string to, string agent,
                                                                 uint32_t escrow_id, asset<0, 17, 0> sbd_amount,
                                                                 asset<0, 17, 0> steem_amount, asset<0, 17, 0> fee,
                                                                 time_point_sec ratification_deadline,
                                                                 time_point_sec escrow_expiration, string json_meta,
                                                                 bool broadcast) {
            FC_ASSERT(!is_locked());
            escrow_transfer_operation<0, 17, 0> op;
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
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::escrow_approve(string from, string to, string agent, string who,
                                                                uint32_t escrow_id, bool approve, bool broadcast) {
            FC_ASSERT(!is_locked());
            escrow_approve_operation<0, 17, 0> op;
            op.from = from;
            op.to = to;
            op.agent = agent;
            op.who = who;
            op.escrow_id = escrow_id;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::escrow_dispute(string from, string to, string agent, string who,
                                                                uint32_t escrow_id, bool broadcast) {
            FC_ASSERT(!is_locked());
            escrow_dispute_operation<0, 17, 0> op;
            op.from = from;
            op.to = to;
            op.agent = agent;
            op.who = who;
            op.escrow_id = escrow_id;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::escrow_release(string from, string to, string agent, string who,
                                                                string receiver, uint32_t escrow_id,
                                                                asset<0, 17, 0> sbd_amount,
                                                                asset<0, 17, 0> steem_amount, bool broadcast) {
            FC_ASSERT(!is_locked());
            escrow_release_operation<0, 17, 0> op;
            op.from = from;
            op.to = to;
            op.agent = agent;
            op.who = who;
            op.receiver = receiver;
            op.escrow_id = escrow_id;
            op.sbd_amount = sbd_amount;
            op.steem_amount = steem_amount;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();
            return my->sign_transaction(tx, broadcast);
        }

        /**
         *  Transfers into savings happen immediately, transfers from savings take 72 hours
         */
        annotated_signed_transaction wallet_api::transfer_to_savings(string from, string to, asset<0, 17, 0> amount,
                                                                     string memo, bool broadcast) {
            FC_ASSERT(!is_locked());
            transfer_to_savings_operation<0, 17, 0> op;
            op.from = from;
            op.to = to;
            op.memo = get_encrypted_memo(from, to, memo);
            op.amount = amount;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        /**
         * @param request_id - an unique ID assigned by from account, the id is used to cancel the operation and can be reused after the transfer completes
         */
        annotated_signed_transaction wallet_api::transfer_from_savings(string from, uint32_t request_id, string to,
                                                                       asset<0, 17, 0> amount, string memo,
                                                                       bool broadcast) {
            FC_ASSERT(!is_locked());
            transfer_from_savings_operation<0, 17, 0> op;
            op.from = from;
            op.request_id = request_id;
            op.to = to;
            op.amount = amount;
            op.memo = get_encrypted_memo(from, to, memo);

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        /**
         *  @param request_id the id used in transfer_from_savings
         *  @param from the account that initiated the transfer
         */
        annotated_signed_transaction wallet_api::cancel_transfer_from_savings(string from, uint32_t request_id,
                                                                              bool broadcast) {
            FC_ASSERT(!is_locked());
            cancel_transfer_from_savings_operation<0, 17, 0> op;
            op.from = from;
            op.request_id = request_id;
            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::transfer_to_vesting(string from, string to, asset<0, 17, 0> amount,
                                                                     bool broadcast) {
            FC_ASSERT(!is_locked());
            transfer_to_vesting_operation<0, 17, 0> op;
            op.from = from;
            op.to = (to == from ? "" : to);
            op.amount = amount;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::withdraw_vesting(string from, asset<0, 17, 0> vesting_shares,
                                                                  bool broadcast) {
            FC_ASSERT(!is_locked());
            withdraw_vesting_operation<0, 17, 0> op;
            op.account = from;
            op.vesting_shares = vesting_shares;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::set_withdraw_vesting_route(string from, string to, uint16_t percent,
                                                                            bool auto_vest, bool broadcast) {
            FC_ASSERT(!is_locked());
            set_withdraw_vesting_route_operation<0, 17, 0> op;
            op.from_account = from;
            op.to_account = to;
            op.percent = percent;
            op.auto_vest = auto_vest;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::convert_sbd(string from, asset<0, 17, 0> amount, bool broadcast) {
            FC_ASSERT(!is_locked());
            convert_operation<0, 17, 0> op;
            op.owner = from;
            op.request_id = fc::time_point::now().sec_since_epoch();
            op.amount = amount;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::publish_feed(string witness, price<0, 17, 0> exchange_rate,
                                                              bool broadcast) {
            FC_ASSERT(!is_locked());
            feed_publish_operation<0, 17, 0> op;
            op.publisher = witness;
            op.exchange_rate = exchange_rate;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        vector<convert_request_object> wallet_api::get_conversion_requests(string owner_account) {
            return my->_remote_db->get_conversion_requests(owner_account);
        }

        string wallet_api::decrypt_memo(string encrypted_memo) {
            if (is_locked()) {
                return encrypted_memo;
            }

            if (encrypted_memo.size() && encrypted_memo[0] == '#') {
                auto m = memo_data::from_string(encrypted_memo);
                if (m) {
                    fc::sha512 shared_secret;
                    auto from_key = my->try_get_private_key(m->from);
                    if (!from_key) {
                        auto to_key = my->try_get_private_key(m->to);
                        if (!to_key) {
                            return encrypted_memo;
                        }
                        shared_secret = to_key->get_shared_secret(m->from);
                    } else {
                        shared_secret = from_key->get_shared_secret(m->to);
                    }
                    fc::sha512::encoder enc;
                    fc::raw::pack(enc, m->nonce);
                    fc::raw::pack(enc, shared_secret);
                    auto encryption_key = enc.result();

                    uint32_t check = fc::sha256::hash(encryption_key)._hash[0];
                    if (check != m->check) {
                        return encrypted_memo;
                    }

                    try {
                        vector<char> decrypted = fc::aes_decrypt(encryption_key, m->encrypted);
                        return fc::raw::unpack<std::string>(decrypted);
                    } catch (...) {
                    }
                }
            }
            return encrypted_memo;
        }

        annotated_signed_transaction wallet_api::decline_voting_rights(string account, bool decline, bool broadcast) {
            FC_ASSERT(!is_locked());
            decline_voting_rights_operation<0, 17, 0> op;
            op.account = account;
            op.decline = decline;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        map<uint32_t, applied_operation> wallet_api::get_account_history(string account, uint32_t from,
                                                                         uint32_t limit) {
            auto result = my->_remote_db->get_account_history(account, from, limit);
            if (!is_locked()) {
                for (auto &item : result) {
                    if (item.second.op.which() == operation::tag<transfer_operation<0, 17, 0>>::value) {
                        auto &top = item.second.op.get<transfer_operation<0, 17, 0>>();
                        top.memo = decrypt_memo(top.memo);
                    } else if (item.second.op.which() ==
                               operation::tag<transfer_from_savings_operation<0, 17, 0>>::value) {
                        auto &top = item.second.op.get<transfer_from_savings_operation<0, 17, 0>>();
                        top.memo = decrypt_memo(top.memo);
                    } else if (item.second.op.which() ==
                               operation::tag<transfer_to_savings_operation<0, 17, 0>>::value) {
                        auto &top = item.second.op.get<transfer_to_savings_operation<0, 17, 0>>();
                        top.memo = decrypt_memo(top.memo);
                    }
                }
            }
            return result;
        }

        vector<withdraw_route> wallet_api::get_withdraw_routes(string account, withdraw_route_type type) const {
            return my->_remote_db->get_withdraw_routes(account, type);
        }

        market_history::order_book wallet_api::get_order_book(const string &base, const string &quote, unsigned limit) {
            market_history::order_book result;

            try {
                my->use_remote_market_history_api();
            } catch (fc::exception &e) {
                elog("Connected node needs to enable market_history");
                return result;
            }

            return (*my->_remote_market_history_api)->get_order_book(base, quote, limit);
        }

        vector<extended_limit_order> wallet_api::get_limit_orders_by_owner(string account_name) {
            try {
                my->use_remote_market_history_api();
            } catch (fc::exception &e) {
                elog("Connected node needs to enable market_history");
                return {};
            }

            return (*my->_remote_market_history_api)->get_limit_orders_by_owner(account_name);
        }

        vector<call_order_object> wallet_api::get_call_orders_by_owner(string account_name) {
            try {
                my->use_remote_market_history_api();
            } catch (fc::exception &e) {
                elog("Connected node needs to enable market_history");
                return {};
            }

            return (*my->_remote_market_history_api)->get_call_orders_by_owner(account_name);
        }

        vector<force_settlement_object> wallet_api::get_settle_orders_by_owner(string account_name) {
            try {
                my->use_remote_market_history_api();
            } catch (fc::exception &e) {
                elog("Connected node needs to enable market_history");
                return {};
            }

            return (*my->_remote_market_history_api)->get_settle_orders_by_owner(account_name);
        }

        annotated_signed_transaction wallet_api::create_order(string owner, protocol::integral_id_type order_id,
                                                              asset<0, 17, 0> amount_to_sell,
                                                              asset<0, 17, 0> min_to_receive, bool fill_or_kill,
                                                              uint32_t expiration_sec, bool broadcast) {
            FC_ASSERT(!is_locked());
            limit_order_create_operation<0, 17, 0> op;
            op.owner = owner;
            op.order_id = order_id;
            op.amount_to_sell = amount_to_sell;
            op.min_to_receive = min_to_receive;
            op.fill_or_kill = fill_or_kill;
            op.expiration = expiration_sec ? (fc::time_point::now() + fc::seconds(expiration_sec))
                                           : fc::time_point::maximum();

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::cancel_order(string owner, protocol::integral_id_type order_id,
                                                              bool broadcast) {
            FC_ASSERT(!is_locked());
            limit_order_cancel_operation<0, 17, 0> op;
            op.owner = owner;
            op.order_id = order_id;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        signed_transaction wallet_api::sell_asset(string seller_account, asset<0, 17, 0> amount_to_sell,
                                                  asset<0, 17, 0> amount_to_receive, uint32_t expiration, protocol::integral_id_type order_id, bool fill_or_kill, bool broadcast) {
            FC_ASSERT(!is_locked());

            auto account = get_account(seller_account);

            limit_order_create_operation<0, 17, 0> op;
            op.owner = account.name;
            op.order_id = order_id;
            op.amount_to_sell = get_asset(amount_to_sell.symbol_name()).amount(amount_to_sell.amount);
            op.min_to_receive = get_asset(amount_to_receive.symbol_name()).amount(amount_to_receive.amount);

            if (expiration) {
                op.expiration = fc::time_point::now() + fc::seconds(expiration);
            }

            op.fill_or_kill = fill_or_kill;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return sign_transaction(tx, broadcast);
        }

        signed_transaction wallet_api::sell(string seller_account, string base, string quote, double rate,
                                            double amount, protocol::integral_id_type order_id, bool broadcast) {
            return sell_asset(seller_account, asset<0, 17, 0>(amount, asset<0, 17, 0>::from_string(base).symbol),
                              asset<0, 17, 0>(rate * amount, asset<0, 17, 0>::from_string(quote).symbol), 0, order_id, false,
                              broadcast);
        }

        signed_transaction wallet_api::buy(string buyer_account, string base, string quote, double rate, double amount, protocol::integral_id_type order_id, bool broadcast) {
            return sell_asset(buyer_account, asset<0, 17, 0>(rate * amount, asset<0, 17, 0>::from_string(quote).symbol),
                              asset<0, 17, 0>(amount, asset<0, 17, 0>::from_string(base).symbol), 0, order_id, false, broadcast);
        }

        signed_transaction wallet_api::borrow_asset(string seller_name, asset<0, 17, 0> amount_to_borrow,
                                                    asset<0, 17, 0> amount_of_collateral, bool broadcast) {
            FC_ASSERT(!is_locked());
            auto seller = get_account(seller_name);
            asset_object mia = get_asset(amount_to_borrow.symbol);
            FC_ASSERT(mia.is_market_issued());

            asset_object collateral = get_asset(
                    asset<0, 17, 0>(0, get_bitasset_data(mia.asset_name).options.short_backing_asset).symbol);

            call_order_update_operation<0, 17, 0> op;
            op.funding_account = seller.name;
            op.delta_debt = mia.amount(amount_to_borrow.amount);
            op.delta_collateral = collateral.amount(amount_of_collateral.amount);

            signed_transaction trx;
            trx.operations = {op};
            trx.validate();
            idump((broadcast));

            return sign_transaction(trx, broadcast);
        }

        signed_transaction wallet_api::issue_asset(string to_account, asset<0, 17, 0> amount, string memo,
                                                   bool broadcast) {
            auto asset_obj = get_asset(amount.symbol);

            account_api_obj to = get_account(to_account);
            account_api_obj issuer = get_account(asset_obj.issuer);

            asset_issue_operation<0, 17, 0> issue_op;
            issue_op.issuer = asset_obj.issuer;
            issue_op.asset_to_issue = asset_obj.amount(amount.amount);
            issue_op.issue_to_account = to.name;
            issue_op.memo = memo;

            signed_transaction tx;
            tx.operations.push_back(issue_op);
            tx.validate();

            return sign_transaction(tx, broadcast);
        }

        signed_transaction wallet_api::create_asset(string issuer, string symbol, uint8_t precision,
                                                    asset_options<0, 17, 0> common, fc::optional<bitasset_options> bitasset_opts,
                                                    bool broadcast) {
            try {
                account_api_obj issuer_account = get_account(issuer);
                FC_ASSERT(!my->find_asset(symbol).valid(), "Asset with that symbol already exists!");

                asset_create_operation<0, 17, 0> create_op;
                create_op.issuer = issuer_account.name;
                create_op.asset_name = symbol;
                create_op.precision = precision;
                create_op.common_options = common;
                create_op.bitasset_opts = bitasset_opts;

                signed_transaction tx;
                tx.operations.push_back(create_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((issuer)(symbol)(precision)(common)(bitasset_opts)(broadcast))
        }

        signed_transaction wallet_api::update_asset(string symbol, optional<string> new_issuer,
                                                    asset_options<0, 17, 0> new_options, bool broadcast /* = false */) {
            try {
                optional<asset_object> asset_to_update = my->find_asset(symbol);
                if (!asset_to_update)
                    FC_THROW("No asset with that symbol exists!");
                optional<account_name_type> new_issuer_account_id;
                if (new_issuer) {
                    account_api_obj new_issuer_account = get_account(*new_issuer);
                    new_issuer_account_id = new_issuer_account.name;
                }

                asset_update_operation<0, 17, 0> update_op;
                update_op.issuer = asset_to_update->issuer;
                update_op.asset_to_update = asset_to_update->asset_name;
                update_op.new_issuer = new_issuer_account_id;
                update_op.new_options = new_options;

                signed_transaction tx;
                tx.operations.push_back(update_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((symbol)(new_issuer)(new_options)(broadcast))
        }

        signed_transaction wallet_api::update_bitasset(string symbol, bitasset_options new_options,
                                                       bool broadcast /* = false */) {
            try {
                optional<asset_object> asset_to_update = my->find_asset(symbol);
                if (!asset_to_update)
                    FC_THROW("No asset with that symbol exists!");

                asset_update_bitasset_operation<0, 17, 0> update_op;
                update_op.issuer = asset_to_update->issuer;
                update_op.asset_to_update = asset_to_update->asset_name;
                update_op.new_options = new_options;

                signed_transaction tx;
                tx.operations.push_back(update_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((symbol)(new_options)(broadcast))
        }

        signed_transaction wallet_api::update_asset_feed_producers(string symbol, flat_set<string> new_feed_producers,
                                                                   bool broadcast /* = false */) {
            try {
                optional<asset_object> asset_to_update = my->find_asset(symbol);
                if (!asset_to_update)
                    FC_THROW("No asset with that symbol exists!");

                asset_update_feed_producers_operation<0, 17, 0> update_op;
                update_op.issuer = asset_to_update->issuer;
                update_op.asset_to_update = asset_to_update->asset_name;
                update_op.new_feed_producers.reserve(new_feed_producers.size());
                std::transform(new_feed_producers.begin(), new_feed_producers.end(),
                               std::inserter(update_op.new_feed_producers, update_op.new_feed_producers.end()),
                               [this](const std::string &account_name) {
                                   return get_account(account_name).name;
                               });

                signed_transaction tx;
                tx.operations.push_back(update_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((symbol)(new_feed_producers)(broadcast))
        }

        signed_transaction wallet_api::publish_asset_feed(string publishing_account, string symbol,
                                                          price_feed<0, 17, 0> feed, bool broadcast /* = false */) {
            try {
                optional<asset_object> asset_to_update = my->find_asset(symbol);
                if (!asset_to_update)
                    FC_THROW("No asset with that symbol exists!");

                asset_publish_feed_operation<0, 17, 0> publish_op;
                publish_op.publisher = get_account(publishing_account).name;
                publish_op.asset_name = asset_to_update->asset_name;
                publish_op.feed = feed;

                signed_transaction tx;
                tx.operations.push_back(publish_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((publishing_account)(symbol)(feed)(broadcast))
        }

        signed_transaction wallet_api::fund_asset_fee_pool(string from, string symbol, string amount,
                                                           bool broadcast /* = false */) {
            try {
                account_api_obj from_account = get_account(from);
                optional<asset_object> asset_to_fund = my->find_asset(symbol);
                if (!asset_to_fund)
                    FC_THROW("No asset with that symbol exists!");
                asset_object core_asset = get_asset(STEEM_SYMBOL_NAME);

                asset_fund_fee_pool_operation<0, 17, 0> fund_op;
                fund_op.from_account = from_account.name;
                fund_op.asset_name = asset_to_fund->asset_name;
                fund_op.amount = core_asset.amount_from_string(amount).amount;

                signed_transaction tx;
                tx.operations.push_back(fund_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((from)(symbol)(amount)(broadcast))
        }

        signed_transaction wallet_api::reserve_asset(string from, string amount, string symbol,
                                                     bool broadcast /* = false */) {
            try {
                account_api_obj from_account = get_account(from);
                optional<asset_object> asset_to_reserve = my->find_asset(symbol);
                if (!asset_to_reserve)
                    FC_THROW("No asset with that symbol exists!");

                asset_reserve_operation<0, 17, 0> reserve_op;
                reserve_op.payer = from_account.name;
                reserve_op.amount_to_reserve = asset_to_reserve->amount_from_string(amount);

                signed_transaction tx;
                tx.operations.push_back(reserve_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((from)(amount)(symbol)(broadcast))
        }

        signed_transaction wallet_api::global_settle_asset(string symbol, price<0, 17, 0> settle_price,
                                                           bool broadcast /* = false */) {
            try {
                optional<asset_object> asset_to_settle = my->find_asset(symbol);
                if (!asset_to_settle)
                    FC_THROW("No asset with that symbol exists!");

                asset_global_settle_operation<0, 17, 0> settle_op;
                settle_op.issuer = asset_to_settle->issuer;
                settle_op.asset_to_settle = asset_to_settle->asset_name;
                settle_op.settle_price = settle_price;

                signed_transaction tx;
                tx.operations.push_back(settle_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((symbol)(settle_price)(broadcast))
        }

        signed_transaction wallet_api::settle_asset(string account_to_settle, string amount_to_settle, string symbol,
                                                    bool broadcast /* = false */) {
            try {
                optional<asset_object> asset_to_settle = my->find_asset(symbol);
                if (!asset_to_settle)
                    FC_THROW("No asset with that symbol exists!");

                asset_settle_operation<0, 17, 0> settle_op;
                settle_op.account = get_account(account_to_settle).name;
                settle_op.amount = asset_to_settle->amount_from_string(amount_to_settle);

                signed_transaction tx;
                tx.operations.push_back(settle_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((account_to_settle)(amount_to_settle)(symbol)(broadcast))
        }

        signed_transaction wallet_api::whitelist_account(string authorizing_account, string account_to_list,
                                                         account_whitelist_operation<0, 17,
                                                                 0>::account_listing new_listing_status,
                                                         bool broadcast /* = false */) {
            try {
                account_whitelist_operation<0, 17, 0> whitelist_op;
                whitelist_op.authorizing_account = get_account(authorizing_account).name;
                whitelist_op.account_to_list = get_account(account_to_list).name;
                whitelist_op.new_listing = new_listing_status;

                signed_transaction tx;
                tx.operations.push_back(whitelist_op);
                tx.validate();

                return sign_transaction(tx, broadcast);
            } FC_CAPTURE_AND_RETHROW((authorizing_account)(account_to_list)(new_listing_status)(broadcast))
        }

        annotated_signed_transaction wallet_api::post_comment(string author, string permlink, string parent_author,
                                                              string parent_permlink, string title, string body,
                                                              string json, bool broadcast) {
            FC_ASSERT(!is_locked());
            comment_operation<0, 17, 0> op;
            op.parent_author = parent_author;
            op.parent_permlink = parent_permlink;
            op.author = author;
            op.permlink = permlink;
            op.title = title;
            op.body = body;
            op.json_metadata = json;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::extend_payout_by_cost(string payer, string author, string permlink,
                                                                       asset<0, 17, 0> extension_cost, bool broadcast) {
            FC_ASSERT(!is_locked());

            comment_payout_extension_operation<0, 17, 0> op;
            op.payer = payer;
            op.author = author;
            op.permlink = permlink;
            op.amount = extension_cost;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::extend_payout_by_time(string payer, string author, string permlink,
                                                                       fc::time_point_sec extension_time,
                                                                       bool broadcast) {
            FC_ASSERT(!is_locked());

            comment_payout_extension_operation<0, 17, 0> op;
            op.payer = payer;
            op.author = author;
            op.permlink = permlink;
            op.extension_time = extension_time;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::vote(string voter, string author, string permlink, int16_t weight,
                                                      bool broadcast) {
            FC_ASSERT(!is_locked());
            FC_ASSERT(abs(weight) <= 100, "Weight must be between -100 and 100 and not 0");

            vote_operation<0, 17, 0> op;
            op.voter = voter;
            op.author = author;
            op.permlink = permlink;
            op.weight = weight * STEEMIT_1_PERCENT;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        void wallet_api::set_transaction_expiration(uint32_t seconds) {
            my->set_transaction_expiration(seconds);
        }

        annotated_signed_transaction wallet_api::challenge(string challenger, string challenged, bool broadcast) {
            FC_ASSERT(!is_locked());

            challenge_authority_operation<0, 17, 0> op;
            op.challenger = challenger;
            op.challenged = challenged;
            op.require_owner = false;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        annotated_signed_transaction wallet_api::prove(string challenged, bool broadcast) {
            FC_ASSERT(!is_locked());

            prove_authority_operation<0, 17, 0> op;
            op.challenged = challenged;
            op.require_owner = false;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }


        annotated_signed_transaction wallet_api::get_transaction(transaction_id_type id) const {
            return my->_remote_db->get_transaction(id);
        }

        annotated_signed_transaction wallet_api::follow(string follower, string following, set<string> what,
                                                        bool broadcast) {
            auto follwer_account = get_account(follower);
            FC_ASSERT(following.size());
            if (following[0] == '@') {
                get_account(following.substr(1));
            }
            FC_ASSERT(following.size() > 1);

            follow::follow_operation fop;
            fop.follower = follower;
            fop.following = following;
            fop.what = what;
            follow::follow_plugin_operation op = fop;

            custom_json_operation jop;
            jop.id = "follow";
            jop.json = fc::json::to_string(op);
            jop.required_posting_auths.insert(follower);

            signed_transaction trx;
            trx.operations.push_back(jop);
            trx.validate();

            return my->sign_transaction(trx, broadcast);
        }

        annotated_signed_transaction wallet_api::reblog(string account, string author, string permlink, bool broadcast )
        {
           follow::reblog_operation rop;
           rop.account = account;
           rop.author = author;
           rop.permlink = permlink;
           follow::follow_plugin_operation op = rop;

           custom_json_operation jop;
           jop.id = "follow";
           jop.json = fc::json::to_string(op);
           jop.required_posting_auths.insert(account);

           signed_transaction trx;
           trx.operations.push_back( jop );
           trx.validate();

           return my->sign_transaction( trx, broadcast );
        }
        
        annotated_signed_transaction wallet_api::send_private_message(string from, string to, string subject,
                                                                      string body, bool broadcast) {
            FC_ASSERT(!is_locked(), "wallet must be unlocked to send a private message");
            auto from_account = get_account(from);
            auto to_account = get_account(to);

            custom_operation op;
            op.required_auths.insert(from);
            op.id = STEEMIT_PRIVATE_MESSAGE_COP_ID;


            private_message_operation pmo;
            pmo.from = from;
            pmo.to = to;
            pmo.sent_time = fc::time_point::now().time_since_epoch().count();
            pmo.from_memo_key = from_account.memo_key;
            pmo.to_memo_key = to_account.memo_key;

            message_body message;
            message.subject = subject;
            message.body = body;

            auto priv_key = wif_to_key(get_private_key(pmo.from_memo_key));
            FC_ASSERT(priv_key, "unable to find private key for memo");
            auto shared_secret = priv_key->get_shared_secret(pmo.to_memo_key);
            fc::sha512::encoder enc;
            fc::raw::pack(enc, pmo.sent_time);
            fc::raw::pack(enc, shared_secret);
            auto encrypt_key = enc.result();
            auto hash_encrypt_key = fc::sha256::hash(encrypt_key);
            pmo.checksum = hash_encrypt_key._hash[0];

            vector<char> plain_text = fc::raw::pack(message);
            pmo.encrypted_message = fc::aes_encrypt(encrypt_key, plain_text);

            message_api_obj obj;
            obj.to_memo_key = pmo.to_memo_key;
            obj.from_memo_key = pmo.from_memo_key;
            obj.checksum = pmo.checksum;
            obj.sent_time = pmo.sent_time;
            obj.encrypted_message = pmo.encrypted_message;
            auto decrypted = try_decrypt_message(obj);

            op.data = fc::raw::pack(pmo);

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return my->sign_transaction(tx, broadcast);
        }

        message_body wallet_api::try_decrypt_message(const message_api_obj &mo) {
            message_body result;

            fc::sha512 shared_secret;

            auto it = my->_keys.find(mo.from_memo_key);
            if (it == my->_keys.end()) {
                it = my->_keys.find(mo.to_memo_key);
                if (it == my->_keys.end()) {
                    wlog("unable to find keys");
                    return result;
                }
                auto priv_key = wif_to_key(it->second);
                if (!priv_key) {
                    return result;
                }
                shared_secret = priv_key->get_shared_secret(mo.from_memo_key);
            } else {
                auto priv_key = wif_to_key(it->second);
                if (!priv_key) {
                    return result;
                }
                shared_secret = priv_key->get_shared_secret(mo.to_memo_key);
            }


            fc::sha512::encoder enc;
            fc::raw::pack(enc, mo.sent_time);
            fc::raw::pack(enc, shared_secret);
            auto encrypt_key = enc.result();

            uint32_t check = fc::sha256::hash(encrypt_key)._hash[0];

            if (mo.checksum != check) {
                return result;
            }

            auto decrypt_data = fc::aes_decrypt(encrypt_key, mo.encrypted_message);
            try {
                return fc::raw::unpack<message_body>(decrypt_data);
            } catch (...) {
                return result;
            }
        }

        vector<extended_message_object> wallet_api::get_inbox(string account, fc::time_point newest, uint32_t limit) {
            FC_ASSERT(!is_locked());
            vector<extended_message_object> result;
            auto remote_result = (*my->_remote_message_api)->get_inbox(account, newest, limit);
            for (const auto &item : remote_result) {
                result.emplace_back(item);
                result.back().message = try_decrypt_message(item);
            }
            return result;
        }

        vector<extended_message_object> wallet_api::get_outbox(string account, fc::time_point newest, uint32_t limit) {
            FC_ASSERT(!is_locked());
            vector<extended_message_object> result;
            auto remote_result = (*my->_remote_message_api)->get_outbox(account, newest, limit);
            for (const auto &item : remote_result) {
                result.emplace_back(item);
                result.back().message = try_decrypt_message(item);
            }
            return result;
        }

        signed_transaction wallet_api::approve_proposal(const string &owner, integral_id_type proposal_id,
                                                        const approval_delta &delta, bool broadcast /* = false */
        ) {
            proposal_update_operation<0, 17, 0> update_op;

            update_op.owner = get_account(owner).name;
            update_op.proposal_id = proposal_id;
            // make sure the proposal exists

            get_proposal(update_op.owner, proposal_id);

            for (const std::string &name : delta.active_approvals_to_add) {
                update_op.active_approvals_to_add.insert(get_account(name).name);
            }
            for (const std::string &name : delta.active_approvals_to_remove) {
                update_op.active_approvals_to_remove.insert(get_account(name).name);
            }
            for (const std::string &name : delta.owner_approvals_to_add) {
                update_op.owner_approvals_to_add.insert(get_account(name).name);
            }
            for (const std::string &name : delta.owner_approvals_to_remove) {
                update_op.owner_approvals_to_remove.insert(get_account(name).name);
            }
            for (const std::string &name : delta.posting_approvals_to_add) {
                update_op.posting_approvals_to_add.insert(get_account(name).name);
            }
            for (const std::string &name : delta.posting_approvals_to_remove) {
                update_op.posting_approvals_to_remove.insert(get_account(name).name);
            }
            for (const std::string &k : delta.key_approvals_to_add) {
                update_op.key_approvals_to_add.insert(public_key_type(k));
            }
            for (const std::string &k : delta.key_approvals_to_remove) {
                update_op.key_approvals_to_remove.insert(public_key_type(k));
            }

            signed_transaction tx;
            tx.operations.push_back(update_op);
            tx.validate();
            return sign_transaction(tx, broadcast);
        }
    }
} // golos::wallet
