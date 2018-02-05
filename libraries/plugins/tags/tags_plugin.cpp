#include <golos/application/impacted.hpp>

#include <golos/application/discussion_query.hpp>
#include <golos/application/api_objects/comment_api_object.hpp>

#include <golos/protocol/config.hpp>

#include <golos/chain/database.hpp>
#include <golos/version/hardfork.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/chain/objects/account_object.hpp>
#include <golos/chain/objects/comment_object.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>
#include <fc/io/json.hpp>
#include <fc/string.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>

#include <golos/tags/tags_plugin.hpp>

namespace golos {
    namespace tags {

        namespace detail {

            using namespace golos::protocol;

            class tags_plugin_impl {
            public:
                tags_plugin_impl(tags_plugin &_plugin) : _self(_plugin) {
                }

                virtual ~tags_plugin_impl();

                golos::chain::database &database() {
                    return _self.database();
                }

                void on_operation(const operation_notification &note);

                tags_plugin &_self;
            };

            tags_plugin_impl::~tags_plugin_impl() {

            }

            struct operation_visitor {
                operation_visitor(database &db) : _db(db) {
                };
                typedef void result_type;

                database &_db;

                void remove_stats(const tag_object &tag, const tag_stats_object &stats) const {
                    _db.modify(stats, [&](tag_stats_object &s) {
                        if (tag.parent == comment_object::id_type()) {
                            s.total_children_rshares2 -= tag.children_rshares2;
                            s.top_posts--;
                        } else {
                            s.comments--;
                        }
                        s.net_votes -= tag.net_votes;
                    });
                }

                void add_stats(const tag_object &tag, const tag_stats_object &stats) const {
                    _db.modify(stats, [&](tag_stats_object &s) {
                        if (tag.parent == comment_object::id_type()) {
                            s.total_children_rshares2 += tag.children_rshares2;
                            s.top_posts++;
                        } else {
                            s.comments++;
                        }
                        s.net_votes += tag.net_votes;
                    });
                }

                void remove_tag(const tag_object &tag) const {
                    /// TODO: update tag stats object
                    _db.remove(tag);

                    const auto &idx = _db.get_index<author_tag_stats_index>().indices().get<by_author_tag_posts>();
                    auto itr = idx.lower_bound(boost::make_tuple(tag.author, tag.name));
                    if (itr != idx.end() && itr->author == tag.author && itr->tag == tag.name) {
                        _db.modify(*itr, [&](author_tag_stats_object &stats) {
                            stats.total_posts--;
                        });
                    }
                }

                const tag_stats_object &get_stats(const string &tag) const {
                    const auto &stats_idx = _db.get_index<tag_stats_index>().indices().get<by_tag>();
                    auto itr = stats_idx.find(tag);
                    if (itr != stats_idx.end()) {
                        return *itr;
                    }

                    return _db.create<tag_stats_object>([&](tag_stats_object &stats) {
                        stats.tag = tag;
                    });
                }

                comment_metadata filter_tags(const comment_object &c) const {
                    comment_metadata meta;

                    if (!c.json_metadata.empty()) {
                        try {
                            meta = fc::json::from_string(to_string(c.json_metadata)).as<comment_metadata>();
                        } catch (const fc::exception &e) {
                            // Do nothing on malformed json_metadata
                        }
                    }

                    set<string> lower_tags;
                    if (!c.category.empty()) {
                        meta.tags.insert(fc::to_lower(to_string(c.category)));
                    }

                    uint8_t tag_limit = 5;
                    uint8_t count = 0;
                    for (const auto &tag : meta.tags) {
                        ++count;
                        if (count > tag_limit || lower_tags.size() > tag_limit) {
                            break;
                        }
                        if (tag.empty()) {
                            continue;
                        }
                        lower_tags.insert(fc::to_lower(tag));
                    }

                    /// the universal tag applies to everything safe for work or nsfw with a non-negative payout
                    if (c.net_rshares >= 0) {
                        lower_tags.insert(string()); /// add it to the universal tag
                    }

                    meta.tags = std::move(lower_tags);

                    return meta;
                }


                void update_tag(const tag_object &current, const comment_object &comment, double hot,
                                double trending) const {
                    const auto &stats = get_stats(current.name);
                    remove_stats(current, stats);

                    if (comment.cashout_time != fc::time_point_sec::maximum()) {
                        _db.modify(current, [&](tag_object &obj) {
                            obj.active = comment.active;
                            obj.cashout = _db.calculate_discussion_payout_time(comment);
                            obj.children = comment.children;
                            obj.net_rshares = comment.net_rshares.value;
                            obj.net_votes = comment.net_votes;
                            obj.children_rshares2 = comment.children_rshares2;
                            obj.hot = hot;
                            obj.trending = trending;
                            if (obj.cashout == fc::time_point_sec()) {
                                obj.promoted_balance = 0;
                            }
                        });
                        add_stats(current, stats);
                    } else {
                        _db.remove(current);
                    }
                }

                void create_tag(const string &tag, const comment_object &comment, double hot, double trending) const {


                    comment_object::id_type parent;
                    account_object::id_type author = _db.get_account(comment.author).id;

                    if (comment.parent_author.size()) {
                        parent = _db.get_comment(comment.parent_author, comment.parent_permlink).id;
                    }

                    const auto &tag_obj = _db.create<tag_object>([&](tag_object &obj) {
                        obj.name = tag;
                        obj.comment = comment.id;
                        obj.parent = parent;
                        obj.created = comment.created;
                        obj.active = comment.active;
                        obj.cashout = comment.cashout_time;
                        obj.net_votes = comment.net_votes;
                        obj.children = comment.children;
                        obj.net_rshares = comment.net_rshares.value;
                        obj.children_rshares2 = comment.children_rshares2;
                        obj.author = author;
                        obj.hot = hot;
                        obj.trending = trending;
                    });
                    add_stats(tag_obj, get_stats(tag));


                    const auto &idx = _db.get_index<author_tag_stats_index>().indices().get<by_author_tag_posts>();
                    auto itr = idx.lower_bound(boost::make_tuple(author, tag));
                    if (itr != idx.end() && itr->author == author && itr->tag == tag) {
                        _db.modify(*itr, [&](author_tag_stats_object &stats) {
                            stats.total_posts++;
                        });
                    } else {
                        _db.create<author_tag_stats_object>([&](author_tag_stats_object &stats) {
                            stats.author = author;
                            stats.tag = tag;
                            stats.total_posts = 1;
                        });
                    }
                }

                /**
                 * https://medium.com/hacking-and-gonzo/how-reddit-ranking-algorithms-work-ef111e33d0d9#.lcbj6auuw
                 */
                template<int64_t S, int32_t T>
                double calculate_score(const share_type &score, const time_point_sec &created) const {
                    /// new algorithm
                    auto mod_score = score.value / S;

                    /// reddit algorithm
                    double order = log10(std::max<int64_t>(std::abs(mod_score), 1));
                    int sign = 0;

                    if (mod_score > 0) {
                        sign = 1;
                    } else if (mod_score < 0) {
                        sign = -1;
                    }

                    return sign * order + double(created.sec_since_epoch()) / double(T);
                }

                inline double calculate_hot(const share_type &score, const time_point_sec &created) const {
                    return calculate_score<10000000, 10000>(score, created);
                }

                inline double calculate_trending(const share_type &score, const time_point_sec &created) const {
                    return calculate_score<10000000, 480000>(score, created);
                }


                /** finds tags that have been added or removed or updated */
                void update_tags(const comment_object &c, bool parse_tags = false) const {
                    try {

                        auto hot = calculate_hot(c.net_rshares, c.created);
                        auto trending = calculate_trending(c.net_rshares, c.created);
                        const auto &comment_idx = _db.get_index<tag_index>().indices().get<by_comment>();

                        if (parse_tags) {
                            auto meta = filter_tags(c);
                            auto citr = comment_idx.lower_bound(c.id);

                            map<string, const tag_object *> existing_tags;
                            vector<const tag_object *> remove_queue;
                            while (citr != comment_idx.end() && citr->comment == c.id) {
                                const tag_object *tag = &*citr;
                                ++citr;
                                if (meta.tags.find(tag->name) == meta.tags.end()) {
                                    remove_queue.push_back(tag);
                                } else {
                                    existing_tags[tag->name] = tag;
                                }
                            }

                            for (const auto &tag : meta.tags) {
                                auto existing = existing_tags.find(tag);
                                if (existing == existing_tags.end()) {
                                    create_tag(tag, c, hot, trending);
                                } else {
                                    update_tag(*existing->second, c, hot, trending);
                                }
                            }

                            for (const auto &item : remove_queue) {
                                remove_tag(*item);
                            }
                        } else {
                            auto citr = comment_idx.lower_bound(c.id);
                            while (citr != comment_idx.end() && citr->comment == c.id) {
                                update_tag(*citr, c, hot, trending);
                                ++citr;
                            }
                        }

                        if (c.parent_author.size()) {
                            update_tags(_db.get_comment(c.parent_author, c.parent_permlink));
                        }
                    } FC_CAPTURE_LOG_AND_RETHROW((c))
                }

                const peer_stats_object &get_or_create_peer_stats(account_object::id_type voter,
                                                                  account_object::id_type peer) const {
                    const auto &peeridx = _db.get_index<peer_stats_index>().indices().get<by_voter_peer>();
                    auto itr = peeridx.find(boost::make_tuple(voter, peer));
                    if (itr == peeridx.end()) {
                        return _db.create<peer_stats_object>([&](peer_stats_object &obj) {
                            obj.voter = voter;
                            obj.peer = peer;
                        });
                    }
                    return *itr;
                }

                void update_indirect_vote(account_object::id_type a, account_object::id_type b, int positive) const {
                    if (a == b) {
                        return;
                    }
                    const auto &ab = get_or_create_peer_stats(a, b);
                    const auto &ba = get_or_create_peer_stats(b, a);
                    _db.modify(ab, [&](peer_stats_object &o) {
                        o.indirect_positive_votes += positive;
                        o.indirect_votes++;
                        o.update_rank();
                    });
                    _db.modify(ba, [&](peer_stats_object &o) {
                        o.indirect_positive_votes += positive;
                        o.indirect_votes++;
                        o.update_rank();
                    });
                }

                void update_peer_stats(const account_object &voter, const account_object &author,
                                       const comment_object &c, int vote) const {
                    if (voter.id == author.id) {
                        return;
                    } /// ignore votes for yourself
                    if (c.parent_author.size()) {
                        return;
                    } /// only count top level posts

                    const auto &stat = get_or_create_peer_stats(voter.id, author.id);
                    _db.modify(stat, [&](peer_stats_object &obj) {
                        obj.direct_votes++;
                        obj.direct_positive_votes += vote > 0;
                        obj.update_rank();
                    });

                    const auto &voteidx = _db.get_index<comment_vote_index>().indices().get<by_comment_voter>();
                    auto itr = voteidx.lower_bound(
                            boost::make_tuple(comment_object::id_type(c.id), account_object::id_type()));
                    while (itr != voteidx.end() && itr->comment == c.id) {
                        update_indirect_vote(voter.id, itr->voter, (itr->vote_percent > 0) == (vote > 0));
                        ++itr;
                    }
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                void operator()(const comment_operation<Major, Hardfork, Release> &op) const {
                    update_tags(_db.get_comment(op.author, op.permlink), true);
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                void operator()(const transfer_operation<Major, Hardfork, Release> &op) const {
                    if (op.to == STEEMIT_NULL_ACCOUNT && op.amount.symbol_name() == SBD_SYMBOL_NAME) {
                        vector<string> part;
                        part.reserve(4);
                        auto path = op.memo;
                        boost::split(part, path, boost::is_any_of("/"));
                        if (!part[0].empty() && part[0][0] == '@') {
                            auto acnt = part[0].substr(1);
                            auto perm = part[1];

                            auto c = _db.find_comment(acnt, perm);
                            if (c && c->parent_author.size() == 0) {
                                const auto &comment_idx = _db.get_index<tag_index>().indices().get<by_comment>();
                                auto citr = comment_idx.lower_bound(c->id);
                                while (citr != comment_idx.end() && citr->comment == c->id) {
                                    _db.modify(*citr, [&](tag_object &t) {
                                        if (t.cashout != fc::time_point_sec::maximum()) {
                                            t.promoted_balance += op.amount.amount;
                                        }
                                    });
                                    ++citr;
                                }
                            } else {
                                ilog("unable to find body");
                            }
                        }
                    }
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                void operator()(const vote_operation<Major, Hardfork, Release> &op) const {
                    update_tags(_db.get_comment(op.author, op.permlink));
                    /*
                    update_peer_stats( db.get_account(op.voter),
                                       db.get_account(op.author),
                                       db.get_comment(op.author, op.permlink),
                                       op.weight );
                                       */
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                void operator()(const delete_comment_operation<Major, Hardfork, Release> &op) const {
                    const auto &idx = _db.get_index<tag_index>().indices().get<by_author_comment>();

                    const auto &auth = _db.get_account(op.author);
                    auto itr = idx.lower_bound(boost::make_tuple(auth.id));
                    while (itr != idx.end() && itr->author == auth.id) {
                        const auto &tobj = *itr;
                        const auto *obj = _db.find<comment_object>(itr->comment);
                        ++itr;
                        if (!obj) {
                            _db.remove(tobj);
                        }
                    }
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                void operator()(const comment_reward_operation<Major, Hardfork, Release> &op) const {
                    const auto &c = _db.get_comment(op.author, op.permlink);
                    update_tags(c);

                    auto meta = filter_tags(c);

                    for (const auto &tag : meta.tags) {
                        _db.modify(get_stats(tag), [&](tag_stats_object &ts) {
                            ts.total_payout += asset<0, 17, 0>(op.payout.amount, op.payout.symbol);
                        });
                    }
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
                void operator()(const comment_payout_update_operation<Major, Hardfork, Release> &op) const {
                    const auto &c = _db.get_comment(op.author, op.permlink);
                    update_tags(c);
                }

                template<typename Op>
                void operator()(Op &&) const {} /// ignore all other ops
            };


            void tags_plugin_impl::on_operation(const operation_notification &note) {
                try {
                    /// plugins shouldn't ever throw
                    note.op.visit(operation_visitor(database()));
                } catch (const fc::exception &e) {
                    edump((e.to_detail_string()));
                } catch (...) {
                    elog("unhandled exception");
                }
            }

        } /// end detail namespace

        tags_plugin::tags_plugin(application *app) : plugin(app), my(new detail::tags_plugin_impl(*this)) {
            chain::database &db = database();
            db.add_plugin_index<tag_index>();
            db.add_plugin_index<tag_stats_index>();
            db.add_plugin_index<peer_stats_index>();
            db.add_plugin_index<author_tag_stats_index>();
        }

        tags_plugin::~tags_plugin() {

        }

        bool tags_plugin::filter(const golos::application::discussion_query &query,
                                 const golos::application::comment_api_object &c,
                                 const std::function<bool(const golos::application::comment_api_object &)> &condition) {
            if (!query.select_authors.empty()) {
                if (query.select_authors.find(c.author) == query.select_authors.end()) {
                    return true;
                }
            }

            tags::comment_metadata meta;

            if (!c.json_metadata.empty()) {
                try {
                    meta = fc::json::from_string(c.json_metadata).as<tags::comment_metadata>();
                } catch (const fc::exception &e) {
                    // Do nothing on malformed json_metadata
                }
            }

            for (const std::set<std::string>::value_type &iterator : query.filter_tags) {
                if (meta.tags.find(iterator) != meta.tags.end()) {
                    return true;
                }
            }

            return condition(c) || query.filter_tags.find(c.category) != query.filter_tags.end();
        }

        void tags_plugin::plugin_set_program_options(boost::program_options::options_description &cli,
                                                     boost::program_options::options_description &cfg) {

        }

        void tags_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
            ilog("Initializing tags plugin");
            database().post_apply_operation.connect([&](const operation_notification &note) {
                my->on_operation(note);
            });

            app().register_api_factory<tag_api>("tag_api");
        }

        void tags_plugin::plugin_startup() {

        }
    }
} /// golos::tags

STEEMIT_DEFINE_PLUGIN(tags, golos::tags::tags_plugin)
