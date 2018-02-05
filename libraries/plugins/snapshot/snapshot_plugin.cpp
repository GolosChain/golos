#include <golos/snapshot/snapshot_plugin.hpp>
#include <golos/snapshot/snapshot_state.hpp>

#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/operation_notification.hpp>

#include <golos/account_by_key/account_by_key_plugin.hpp>

#include <boost/iostreams/device/mapped_file.hpp>

namespace golos {
    namespace plugin {
        namespace snapshot {
            namespace detail {
                class snapshot_plugin_impl {
                public:
                    snapshot_plugin_impl(snapshot_plugin &plugin)
                            : self(plugin) {

                    }

                    golos::chain::database &database() {
                        return self.database();
                    }

                    void pre_operation(const chain::operation_notification &op_obj);

                    void post_operation(const chain::operation_notification &op_obj);

                    void update_key_lookup(const chain::account_authority_object &a);

                    snapshot_plugin &self;
                };

                struct pre_operation_visitor {
                    snapshot_plugin &_plugin;

                    pre_operation_visitor(snapshot_plugin &plugin)
                            : _plugin(plugin) {
                    }

                    typedef void result_type;

                    template<typename T>
                    void operator()(const T &) const {
                    }
                };

                struct post_operation_visitor {
                    snapshot_plugin &_plugin;

                    post_operation_visitor(snapshot_plugin &plugin)
                            : _plugin(plugin) {
                    }

                    typedef void result_type;

                    template<typename T>
                    void operator()(const T &) const {
                    }

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                    void operator()(const chain::hardfork_operation<Major, Hardfork, Release> &op) const {
                        if (op.hardfork_id == STEEMIT_HARDFORK_0_17) {
                            auto &db = _plugin.database();

                            if (_plugin.get_loaded_snapshots().right.find("1f0617dfc2e7aa49b0d6c394b36087ead02bc7f781e7550dae13e8cb12f13436") !=
                                _plugin.get_loaded_snapshots().right.end()) {
                                snapshot_state snapshot = fc::json::from_file(fc::path(_plugin.get_loaded_snapshots().right.at("1f0617dfc2e7aa49b0d6c394b36087ead02bc7f781e7550dae13e8cb12f13436"))).as<snapshot_state>();
                                for (account_summary &account : snapshot.accounts) {
                                    db.modify(*db.find_account(account.name), [&](chain::account_object &a) {
                                        std::size_t position = a.json_metadata.find("created_at: 'GENESIS'");
                                        if (position != std::string::npos) {
                                            a.json_metadata.erase(a.json_metadata.find("created_at: 'GENESIS'"),
                                                    a.json_metadata.find("created_at: 'GENESIS'") +
                                                    std::string("created_at: 'GENESIS'").length());
                                            a.json_metadata.insert(
                                                    a.json_metadata.find_first_of('{') +
                                                    1, "\"created_at\": \"GENESIS\",");
                                        }
                                    });
                                }
                            }
                        }
                    }
                };

                void snapshot_plugin_impl::pre_operation(const chain::operation_notification &note) {
                    note.op.visit(pre_operation_visitor(self));
                }

                void snapshot_plugin_impl::post_operation(const chain::operation_notification &note) {
                    note.op.visit(post_operation_visitor(self));
                }

                void snapshot_plugin_impl::update_key_lookup(const chain::account_authority_object &a) {
                    try {
                        self.application->get_plugin<account_by_key::account_by_key_plugin>(ACCOUNT_BY_KEY_PLUGIN_NAME)->update_key_lookup(a);
                    } catch (const fc::assert_exception &e) {
                        ilog("Account by key plugin not loaded");
                    }
                }
            }

            snapshot_plugin::snapshot_plugin(golos::application::application *app)
                    : plugin(app),
                      application(app),
                      impl(new detail::snapshot_plugin_impl(*this)) {
            }

            snapshot_plugin::~snapshot_plugin() {
            }

            std::string snapshot_plugin::plugin_name() const {
                return SNAPSHOT_PLUGIN_NAME;
            }

            void snapshot_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
                ilog("Initializing snapshot plugin");

                this->options = options;
            }

            void snapshot_plugin::plugin_set_program_options(boost::program_options::options_description &command_line_options, boost::program_options::options_description &config_file_options) {
                command_line_options.add_options()
                        ("snapshot-file", boost::program_options::value<vector<string>>()->composing()->multitoken(), "Snapshot files to load");
                config_file_options.
                        add(command_line_options);
            }

            void snapshot_plugin::plugin_startup() {
                if (options.count("snapshot-file") != 0u) {
                    load_snapshots(options["snapshot-file"].as<vector<string>>());
                } else {
                #ifndef STEEMIT_BUILD_TESTNET
                    load_snapshots({"snapshot5392323.json"});
                #endif
                }
            }

            const boost::bimap<string, string> &snapshot_plugin::get_loaded_snapshots() const {
                return loaded_snapshots;
            }

            void snapshot_plugin::load_snapshots(const std::vector<std::string> &snapshots) {
                chain::database &db = database();

                for (const vector<string>::value_type &iterator : snapshots) {
                    FC_ASSERT(fc::exists(iterator), "Snapshot file '${file}' was not found.", ("file", iterator));

                    ilog("Loading snapshot from ${s}", ("s", iterator));

                    std::string snapshot_hash(fc::sha256::hash(boost::iostreams::mapped_file_source(fc::path(iterator).string()).data()));

                    snapshot_state snapshot = fc::json::from_file(fc::path(iterator)).as<snapshot_state>();
                    for (account_summary &account : snapshot.accounts) {
                        if (db.find_account(account.name) == nullptr) {
                            const chain::account_object &new_account = db.create<chain::account_object>([&](chain::account_object &a) {
                                a.name = account.name;
                                a.memo_key = account.keys.memo_key;

                                if (snapshot_hash ==
                                    "1f0617dfc2e7aa49b0d6c394b36087ead02bc7f781e7550dae13e8cb12f13436") {
                                    a.json_metadata = "{created_at: 'GENESIS'}";
                                    a.recovery_account = STEEMIT_INIT_MINER_NAME;
                                } else {
                                    a.json_metadata = account.json_metadata.data();
                                    a.recovery_account = account.recovery_account;
                                }
                            });

                            auto &index = db.get_index<chain::account_balance_index>().indices().get<chain::by_account_asset>();
                            auto itr = index.find(boost::make_tuple(new_account.name, STEEM_SYMBOL_NAME));
                            if (itr == index.end()) {
                                db.create<chain::account_balance_object>([new_account](chain::account_balance_object &b) {
                                    b.owner = new_account.name;
                                    b.asset_name = STEEM_SYMBOL_NAME;
                                    b.balance = 0;
                                });
                            }

                            itr = index.find(boost::make_tuple(new_account.name, SBD_SYMBOL_NAME));
                            if (itr == index.end()) {
                                db.create<chain::account_balance_object>([new_account](chain::account_balance_object &b) {
                                    b.owner = new_account.name;
                                    b.asset_name = SBD_SYMBOL_NAME;
                                    b.balance = 0;
                                });
                            }

                            auto &stats_index = db.get_index<chain::account_statistics_index>().indices().get<chain::by_name>();
                            auto stats_itr = stats_index.find(new_account.name);
                            if (stats_itr == stats_index.end()) {
                                db.create<chain::account_statistics_object>([&](chain::account_statistics_object& s){
                                    s.owner = new_account.name;
                                });
                            }

                            impl->update_key_lookup(db.create<chain::account_authority_object>([&](chain::account_authority_object &auth) {
                                auth.account = account.name;
                                auth.owner.weight_threshold = 1;
                                auth.owner = account.keys.owner_key;
                                auth.active = account.keys.active_key;
                                auth.posting = account.keys.posting_key;
                            }));
                        }
                    }

                    loaded_snapshots.insert({iterator, snapshot_hash});
                }
            }
        }
    }
}

/**
 * The STEEMIT_DEFINE_PLUGIN() macro will define a golos::plugin::create_hello_api_plugin()
 * factory method which is expected by the manifest.
 */

STEEMIT_DEFINE_PLUGIN(snapshot, golos::plugin::snapshot::snapshot_plugin)