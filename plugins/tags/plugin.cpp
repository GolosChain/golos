#include <boost/program_options/options_description.hpp>
#include <golos/plugins/tags/plugin.hpp>
#include <golos/plugins/tags/tags_object.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/chain/index.hpp>
#include <golos/api/discussion.hpp>
#include <golos/plugins/tags/discussion_query.hpp>
#include <golos/api/vote_state.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/api/discussion_helper.hpp>
#include <golos/plugins/tags/tag_visitor.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/plugins/social_network/social_network.hpp>


namespace golos { namespace plugins { namespace tags {

    using golos::plugins::social_network::comment_last_update_index;

    using golos::chain::feed_history_object;
    using golos::api::discussion_helper;

    struct tags_plugin::impl final {
        impl(): database_(appbase::app().get_plugin<chain::plugin>().db()) {
            helper = std::make_unique<discussion_helper>(
                database_,
                follow::fill_account_reputation,
                fill_promoted,
                social_network::fill_comment_info);
        }

        ~impl() {}

        void on_operation(const operation_notification& note) {
            try {
                /// plugins shouldn't ever throw
                note.op.visit(tags::operation_visitor(database_, tags_number, tag_max_length));
            } catch (const fc::exception& e) {
                edump((e.to_detail_string()));
            } catch (...) {
                elog("unhandled exception");
            }
        }

        golos::chain::database& database() {
            return database_;
        }

        golos::chain::database& database() const {
            return database_;
        }

        std::vector<vote_state> select_active_votes(const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset) const;

        bool filter_tags(const tags::tag_type type, std::set<std::string>& select_tags) const;

        bool filter_authors(discussion_query& query) const;

        bool filter_start_comment(discussion_query& query) const;

        bool filter_parent_comment(discussion_query& query) const;

        bool filter_query(discussion_query& query) const;

        template<typename DatabaseIndex, typename DiscussionIndex, typename Fill>
        std::vector<discussion> select_unordered_discussions(discussion_query&, Fill&&) const;

        template<typename Iterator, typename Order, typename Select, typename Exit>
        void select_discussions(
            std::set<comment_object::id_type>& id_set,
            std::vector<discussion>& result,
            const discussion_query& query,
            Iterator itr, Iterator etr,
            Select&& select,
            Exit&& exit,
            Order&& order
        ) const;

        template<typename DiscussionOrder, typename Selector>
        std::vector<discussion> select_ordered_discussions(discussion_query&, Selector&&) const;

        std::vector<tag_api_object> get_trending_tags(const std::string& after, uint32_t limit) const;

        std::vector<std::pair<std::string, uint32_t>> get_tags_used_by_author(const std::string& author) const;

        void set_pending_payout(discussion& d) const;

        void set_url(discussion& d) const;

        std::vector<discussion> get_replies_by_last_update(
            account_name_type start_parent_author, std::string start_permlink,
            uint32_t limit, uint32_t vote_limit
        ) const;

        discussion get_discussion(const comment_object& c, uint32_t vote_limit, uint32_t votes_offset) const ;

        discussion create_discussion(const comment_object& o) const;
        discussion create_discussion(const comment_object& o, const discussion_query& query) const;
        void fill_discussion(discussion& d, const discussion_query& query) const;
        void fill_comment_api_object(const comment_object& o, discussion& d) const;

        comment_api_object create_comment_api_object(const comment_object & o) const;

        get_languages_result get_languages();

        std::size_t tags_number;
        std::size_t tag_max_length;
    private:
        golos::chain::database& database_;
        std::unique_ptr<discussion_helper> helper;
    };

    std::vector<vote_state> tags_plugin::impl::select_active_votes(
        const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
    ) const {
        return helper->select_active_votes(author, permlink, limit, offset);
    }

    discussion tags_plugin::impl::get_discussion(const comment_object& c, uint32_t vote_limit, uint32_t votes_offset) const {
        return helper->get_discussion(c, vote_limit, votes_offset);
    }

    get_languages_result tags_plugin::impl::get_languages() {
        auto& idx = database().get_index<tags::language_index>().indices().get<tags::by_tag>();
        get_languages_result result;
        for (auto itr = idx.begin(); idx.end() != itr; ++itr) {
            result.languages.insert(itr->name);
        }
        return result;
    }

    discussion tags_plugin::impl::create_discussion(const comment_object& o) const {
        return helper->create_discussion(o);
    }

    void tags_plugin::impl::fill_comment_api_object(const comment_object& o, discussion& d) const {
        helper->fill_comment_api_object(o, d);
    }

    void tags_plugin::impl::fill_discussion(discussion& d, const discussion_query& query) const {
        set_url(d);
        set_pending_payout(d);
        d.active_votes = select_active_votes(d.author, d.permlink, query.vote_limit, query.vote_offset);
        d.body_length = static_cast<uint32_t>(d.body.size());
        if (query.truncate_body) {
            if (d.body.size() > query.truncate_body) {
                d.body.erase(query.truncate_body);
            }

            if (!fc::is_utf8(d.title)) {
                d.title = fc::prune_invalid_utf8(d.title);
            }

            if (!fc::is_utf8(d.body)) {
                d.body = fc::prune_invalid_utf8(d.body);
            }

            if (!fc::is_utf8(d.json_metadata)) {
                d.json_metadata = fc::prune_invalid_utf8(d.json_metadata);
            }
        }
    }

    discussion tags_plugin::impl::create_discussion(const comment_object& o, const discussion_query& query) const {

        discussion d = create_discussion(o);
        fill_discussion(d, query);

        return d;
    }

    comment_api_object tags_plugin::impl::create_comment_api_object(const comment_object & o) const {
        return helper->create_comment_api_object( o );
    }

    DEFINE_API(tags_plugin, get_languages) {
        PLUGIN_API_VALIDATE_ARGS();
        return pimpl->database().with_weak_read_lock([&]() {
            return pimpl->get_languages();
        });
    }

    void tags_plugin::plugin_startup() {
        wlog("tags plugin: plugin_startup()");
    }

    void tags_plugin::plugin_shutdown() {
        wlog("tags plugin: plugin_shutdown(()");
    }

    const std::string& tags_plugin::name() {
        static std::string name = "tags";
        return name;
    }

    tags_plugin::tags_plugin() {
    }

    void tags_plugin::set_program_options(
        boost::program_options::options_description&,
        boost::program_options::options_description& config_file_options
    ) {
        config_file_options.add_options()
            (
                "tags-number", boost::program_options::value<uint16_t>()->default_value(5),
                "Maximum number of tags"
            ) (
                "tag-max-length", boost::program_options::value<uint16_t>()->default_value(512),
                "Maximum length of tag"
            );
    }

    void tags_plugin::plugin_initialize(const boost::program_options::variables_map& options) {
        pimpl = std::make_unique<impl>();
        auto& db = pimpl->database();
        db.post_apply_operation.connect([&](const operation_notification& note) {
            pimpl->on_operation(note);
        });
        add_plugin_index<tags::tag_index>(db);
        add_plugin_index<tags::tag_stats_index>(db);
        add_plugin_index<tags::author_tag_stats_index>(db);
        add_plugin_index<tags::language_index>(db);

        pimpl->tags_number = options.at("tags-number").as<uint16_t>();
        pimpl->tag_max_length = options.at("tag-max-length").as<uint16_t>();

        JSON_RPC_REGISTER_API (name());

    }

    tags_plugin::~tags_plugin() = default;

    void tags_plugin::impl::set_url(discussion& d) const {
        helper->set_url( d );
    }

    boost::multiprecision::uint256_t to256(const fc::uint128_t& t) {
        boost::multiprecision::uint256_t result(t.high_bits());
        result <<= 65;
        result += t.low_bits();
        return result;
    }

    void tags_plugin::impl::set_pending_payout(discussion& d) const {
        helper->set_pending_payout(d);
    }

    bool tags_plugin::impl::filter_tags(const tags::tag_type type, std::set<std::string>& select_tags) const {
        if (select_tags.empty()) {
            return true;
        }

        auto& db = database();
        auto& idx = db.get_index<tags::tag_index>().indices().get<tags::by_tag>();
        auto src_tags = std::move(select_tags);
        for (const auto& name: src_tags) {
            if (idx.find(std::make_tuple(name, type)) != idx.end()) {
                select_tags.insert(name);
            }
        }
        return !select_tags.empty();
    }

    bool tags_plugin::impl::filter_authors(discussion_query& query) const {
        if (query.select_authors.empty()) {
            return true;
        }

        auto& db = database();
        auto select_authors = std::move(query.select_authors);
        for (auto& name: select_authors) {
            auto* author = db.find_account(name);
            if (author) {
                query.select_author_ids.insert(author->id);
                query.select_authors.insert(name);
            }
        }
        return query.has_author_selector();
    }

    bool tags_plugin::impl::filter_start_comment(discussion_query& query) const {
        if (query.has_start_comment()) {
            if (!query.start_permlink.valid()) {
                return false;
            }
            auto* comment = database().find_comment(*query.start_author, *query.start_permlink);
            if (!comment) {
                return false;
            }

            query.start_comment = create_discussion(*comment, query);
            auto& d = query.start_comment;
            operation_visitor v(database_, tags_number, tag_max_length);

            d.hot = v.calculate_hot(d.net_rshares, d.created);
            d.trending = v.calculate_trending(d.net_rshares, d.created);
        }
        return true;
    }

    bool tags_plugin::impl::filter_parent_comment(discussion_query& query) const {
        if (query.has_parent_comment()) {
            if (!query.parent_permlink) {
                return false;
            }
            auto* comment = database().find_comment(*query.parent_author, *query.parent_permlink);
            if (comment) {
                return false;
            }
            query.parent_comment = create_discussion(*comment, query);
        }
        return true;
    }

    bool tags_plugin::impl::filter_query(discussion_query& query) const {
        if (!filter_tags(tags::tag_type::language, query.select_languages) ||
            !filter_tags(tags::tag_type::tag, query.select_tags) ||
            !filter_authors(query)
        ) {
            return false;
        }

        return true;
    }

    template<
        typename DatabaseIndex,
        typename DiscussionIndex,
        typename Fill>
    std::vector<discussion> tags_plugin::impl::select_unordered_discussions(
        discussion_query& query,
        Fill&& fill
    ) const {
        std::vector<discussion> result;

        if (!filter_start_comment(query) || !filter_query(query)) {
            return result;
        }

        auto& db = database();
        const auto& idx = db.get_index<DatabaseIndex>().indices().template get<DiscussionIndex>();
        auto etr = idx.end();
        bool can_add = true;

        result.reserve(query.limit);

        std::set<comment_object::id_type> id_set;
        auto aitr = query.select_authors.begin();
        if (query.has_start_comment()) {
            can_add = false;
        }

        for (; query.select_authors.end() != aitr && result.size() < query.limit; ++aitr) {
            auto itr = idx.lower_bound(*aitr);
            for (; itr != etr && itr->account == *aitr && result.size() < query.limit; ++itr) {
                if (id_set.count(itr->comment)) {
                    continue;
                }
                id_set.insert(itr->comment);

                if (query.has_start_comment() && !can_add) {
                    can_add = (query.is_good_start(itr->comment));
                    if (!can_add) {
                        continue;
                    }
                }

                const auto* comment = db.find(itr->comment);
                if (!comment) {
                    continue;
                }

                if ((query.parent_author && *query.parent_author != comment->parent_author) ||
                    (query.parent_permlink && *query.parent_permlink != to_string(comment->parent_permlink))
                ) {
                    continue;
                }

                discussion d = create_discussion(*comment);
                if (!query.is_good_tags(d, tags_number, tag_max_length)) {
                    continue;
                }

                fill_discussion(d, query);
                fill(d, *itr);
                result.push_back(std::move(d));
            }
        }
        return result;
    }

    template<
        typename Iterator,
        typename Order,
        typename Select,
        typename Exit>
    void tags_plugin::impl::select_discussions(
        std::set<comment_object::id_type>& id_set,
        std::vector<discussion>& result,
        const discussion_query& query,
        Iterator itr, Iterator etr,
        Select&& select,
        Exit&& exit,
        Order&& order
    ) const {
        auto& db = database();
        for (; itr != etr && !exit(*itr); ++itr) {
            if (id_set.count(itr->comment)) {
                continue;
            }
            id_set.insert(itr->comment);

            if (!query.is_good_parent(itr->parent) || !query.is_good_author(itr->author)) {
                continue;
            }

            const auto* comment = db.find(itr->comment);
            if (!comment) {
                continue;
            }

            discussion d = create_discussion(*comment);
            d.promoted = asset(itr->promoted_balance, SBD_SYMBOL);

            if (!select(d) || !query.is_good_tags(d, tags_number, tag_max_length)) {
                continue;
            }

            fill_discussion(d, query);
            d.hot = itr->hot;
            d.trending = itr->trending;

            if (query.has_start_comment() && !query.is_good_start(d.id) && !order(query.start_comment, d)) {
                continue;
            }

            result.push_back(std::move(d));
        }
    }

    template<
        typename DiscussionOrder,
        typename Selector>
    std::vector<discussion> tags_plugin::impl::select_ordered_discussions(
        discussion_query& query,
        Selector&& selector
    ) const {
        std::vector<discussion> unordered;
        auto& db = database();

        db.with_weak_read_lock([&]() {
            if (!filter_query(query) || !filter_start_comment(query) || !filter_parent_comment(query) ||
                (query.has_start_comment() && !query.is_good_author(*query.start_author))
            ) {
                return false;
            }

            std::set<comment_object::id_type> id_set;
            if (query.has_tags_selector()) { // seems to have a least complexity
                const auto& idx = db.get_index<tags::tag_index>().indices().get<tags::by_tag>();
                auto etr = idx.end();
                unordered.reserve(query.select_tags.size() * query.limit);

                for (auto& name: query.select_tags) {
                    select_discussions(
                        id_set, unordered, query, idx.lower_bound(std::make_tuple(name, tags::tag_type::tag)), etr,
                        selector,
                        [&](const tags::tag_object& tag){
                            return tag.name != name || tag.type != tags::tag_type::tag;
                        },
                        DiscussionOrder());
                }
            } else if (query.has_author_selector()) { // a more complexity
                const auto& idx = db.get_index<tags::tag_index>().indices().get<tags::by_author_comment>();
                auto etr = idx.end();
                unordered.reserve(query.select_author_ids.size() * query.limit);

                for (auto& id: query.select_author_ids) {
                    select_discussions(
                        id_set, unordered, query, idx.lower_bound(id), etr,
                        selector,
                        [&](const tags::tag_object& tag){
                            return tag.author != id;
                        },
                        DiscussionOrder());
                }
            } else if (query.has_language_selector()) { // the most complexity
                const auto& idx = db.get_index<tags::tag_index>().indices().get<tags::by_tag>();
                auto etr = idx.end();
                unordered.reserve(query.select_languages.size() * query.limit);

                for (auto& name: query.select_languages) {
                    select_discussions(
                        id_set, unordered, query, idx.lower_bound(std::make_tuple(name, tags::tag_type::language)), etr,
                        selector,
                        [&](const tags::tag_object& tag){
                            return tag.name != name || tag.type != tags::tag_type::language;
                        },
                        DiscussionOrder());
                }
            } else {
                const auto& indices = db.get_index<tags::tag_index>().indices();
                const auto& idx = indices.get<DiscussionOrder>();
                auto itr = idx.begin();

                if (query.has_start_comment()) {
                    const auto& cidx = indices.get<tags::by_comment>();
                    const auto citr = cidx.find(query.start_comment.id);
                    if (citr == cidx.end()) {
                        return false;
                    }
                    query.reset_start_comment();
                    itr = idx.iterator_to(*citr);
                }

                unordered.reserve(query.limit);

                select_discussions(
                    id_set, unordered, query, itr, idx.end(), selector,
                    [&](const tags::tag_object& tag){
                        return unordered.size() >= query.limit;
                    },
                    [&](const auto&, const auto&) {
                        return true;
                    });
            }
            return true;
        });

        std::vector<discussion> result;
        if (unordered.empty()) {
            return result;
        }

        auto it = unordered.begin();
        const auto et = unordered.end();
        std::sort(it, et, DiscussionOrder());

        if (query.has_start_comment()) {
            for (; et != it && it->id != query.start_comment.id; ++it);
            if (et == it) {
                return result;
            }
        }

        for (uint32_t idx = 0; idx < query.limit && et != it; ++it, ++idx) {
            result.push_back(std::move(*it));
        }

        return result;
    }

    DEFINE_API(tags_plugin, get_discussions_by_blog) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );

        query.prepare();
        query.validate();
        GOLOS_CHECK_PARAM(query.select_authors,
            GOLOS_CHECK_VALUE(query.select_authors.size(), "Must get blogs for specific authors"));

        auto& db = pimpl->database();
        GOLOS_ASSERT(db.has_index<follow::feed_index>(), golos::unsupported_api_method, 
                "Node is not running the follow plugin");

        return db.with_weak_read_lock([&]() {
            return pimpl->select_unordered_discussions<follow::blog_index, follow::by_blog>(
                query,
                [&](discussion& d, const follow::blog_object& b) {
                    d.first_reblogged_on = b.reblogged_on;
                    d.reblog_author = b.account;
                    d.reblog_title = to_string(b.reblog_title);
                    d.reblog_body = to_string(b.reblog_body);
                    d.reblog_json_metadata = to_string(b.reblog_json_metadata);
                });
        });
    }

    DEFINE_API(tags_plugin, get_discussions_by_feed) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        GOLOS_CHECK_PARAM(query.select_authors,
            GOLOS_CHECK_VALUE(query.select_authors.size(), "Must get feeds for specific authors"));

        auto& db = pimpl->database();
        GOLOS_ASSERT(db.has_index<follow::feed_index>(), golos::unsupported_api_method,
                "Node is not running the follow plugin");

        return db.with_weak_read_lock([&]() {
            return pimpl->select_unordered_discussions<follow::feed_index, follow::by_feed>(
                query,
                [&](discussion& d, const follow::feed_object& f) {
                    d.reblogged_by.assign(f.reblogged_by.begin(), f.reblogged_by.end());
                    d.first_reblogged_by = f.first_reblogged_by;
                    d.first_reblogged_on = f.first_reblogged_on;
                    for (const auto& a : f.reblogged_by) {
                        const auto& blog_idx = db.get_index<follow::blog_index>().indices().get<follow::by_comment>();
                        auto blog_itr = blog_idx.find(std::make_tuple(f.comment, a));
                        d.reblog_entries.emplace_back(
                            a,
                            to_string(blog_itr->reblog_title),
                            to_string(blog_itr->reblog_body),
                            to_string(blog_itr->reblog_json_metadata)
                        );
                    }
                });
        });
    }

    DEFINE_API(tags_plugin, get_discussions_by_comments) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        std::vector<discussion> result;
        query.prepare();
        query.validate();
        GOLOS_CHECK_PARAM(query.start_author,
            GOLOS_CHECK_VALUE(!!query.start_author, "Must get comments for specific authors"));

        auto& db = pimpl->database();

        if (!db.has_index<comment_last_update_index>()) {
            return result;
        }

        return db.with_weak_read_lock([&]() {
            const auto& clu_cmt_idx = db.get_index<comment_last_update_index>().indices().get<golos::plugins::social_network::by_comment>();
            const auto& clu_idx = db.get_index<comment_last_update_index>().indices().get<golos::plugins::social_network::by_author_last_update>();

            auto itr = clu_idx.lower_bound(*query.start_author);
            if (itr == clu_idx.end()) {
                return result;
            }

            if (!!query.start_permlink) {
                const auto &lidx = db.get_index<comment_index>().indices().get<by_permlink>();
                auto litr = lidx.find(std::make_tuple(*query.start_author, *query.start_permlink));
                if (litr == lidx.end()) {
                    return result;
                }
                auto clu_itr = clu_cmt_idx.find(litr->id);
                if (clu_itr == clu_cmt_idx.end()) {
                    return result;
                } else {
                    itr = clu_idx.iterator_to(*clu_itr);
                }
            }

            if (!pimpl->filter_query(query)) {
                return result;
            }

            result.reserve(query.limit);

            for (; itr != clu_idx.end() && itr->author == *query.start_author && result.size() < query.limit; ++itr) {
                if (itr->parent_author.size() > 0) {
                    discussion p;
                    auto& comment = db.get_comment(itr->comment);
                    pimpl->fill_comment_api_object(db.get_comment(comment.root_comment), p);
                    if (!query.is_good_tags(p, pimpl->tags_number, pimpl->tag_max_length) ||
                        !query.is_good_author(p.author)
                    ) {
                        continue;
                    }
                    discussion d;
                    pimpl->fill_comment_api_object(comment, d);
                    result.push_back(std::move(d));
                    pimpl->fill_discussion(result.back(), query);
                }
            }
            return result;
        });
    }

    DEFINE_API(tags_plugin, get_discussions_by_trending) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_trending>(
            query,
            [&](const discussion& d) -> bool {
                return d.net_rshares > 0;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_promoted) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_promoted>(
            query,
            [&](const discussion& d) -> bool {
                return !!d.promoted && d.promoted->amount > 0;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_created) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_created>(
            query,
            [&](const discussion& d) -> bool {
                return true;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_active) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        auto& db = pimpl->database();
        if (!db.has_index<comment_last_update_index>()) {
            return std::vector<discussion>();
        }
        return pimpl->select_ordered_discussions<sort::by_active>(
            query,
            [&](const discussion& d) -> bool {
                return true;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_cashout) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_cashout>(
            query,
            [&](const discussion& d) -> bool {
                return d.net_rshares > 0;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_payout) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_net_rshares>(
            query,
            [&](const discussion& d) -> bool {
                return d.net_rshares > 0;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_votes) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_net_votes>(
            query,
            [&](const discussion& d) -> bool {
                return true;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_children) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_children>(
            query,
            [&](const discussion& d) -> bool {
                return true;
            }
        );
    }

    DEFINE_API(tags_plugin, get_discussions_by_hot) {
        PLUGIN_API_VALIDATE_ARGS(
            (discussion_query, query)
        );
        query.prepare();
        query.validate();
        return pimpl->select_ordered_discussions<sort::by_hot>(
            query,
            [&](const discussion& d) -> bool {
                return d.net_rshares > 0;
            }
        );
    }

    std::vector<tag_api_object>
    tags_plugin::impl::get_trending_tags(const std::string& after, uint32_t limit) const {
        limit = std::min(limit, uint32_t(1000));
        std::vector<tag_api_object> result;
        result.reserve(limit);

        const auto& nidx = database().get_index<tags::tag_stats_index>().indices().get<tags::by_tag>();
        const auto& ridx = database().get_index<tags::tag_stats_index>().indices().get<tags::by_trending>();
        auto itr = ridx.begin();
        if (!after.empty() && nidx.size()) {
            auto nitr = nidx.lower_bound(std::make_tuple(tags::tag_type::tag, after));
            if (nitr == nidx.end()) {
                itr = ridx.end();
            } else {
                itr = ridx.iterator_to(*nitr);
            }
        }

        for (; itr->type == tags::tag_type::tag && itr != ridx.end() && result.size() < limit; ++itr) {
            tag_api_object push_object = tag_api_object(*itr);

            if (push_object.name.empty()) {
                continue;
            }

            if (!fc::is_utf8(push_object.name)) {
                push_object.name = fc::prune_invalid_utf8(push_object.name);
            }

            result.emplace_back(push_object);
        }
        return result;
    }

    DEFINE_API(tags_plugin, get_trending_tags) {
        PLUGIN_API_VALIDATE_ARGS(
            (string,   after)
            (uint32_t, limit)
        );

        return pimpl->database().with_weak_read_lock([&]() {
            return pimpl->get_trending_tags(after, limit);
        });
    }

    std::vector<std::pair<std::string, uint32_t>> tags_plugin::impl::get_tags_used_by_author(
        const std::string& author
    ) const {
        std::vector<std::pair<std::string, uint32_t>> result;
        auto& db = database();
        const auto* acnt = db.find_account(author);
        if (acnt == nullptr) {
            return result;
        }
        const auto& tidx = db.get_index<tags::author_tag_stats_index>().indices().get<tags::by_author_posts_tag>();
        auto itr = tidx.lower_bound(std::make_tuple(acnt->id, tags::tag_type::tag));
        for (;itr != tidx.end() && itr->author == acnt->id && result.size() < 1000; ++itr) {
            if (itr->type == tags::tag_type::tag && itr->name.size()) {
                if (!fc::is_utf8(itr->name)) {
                    result.emplace_back(std::make_pair(fc::prune_invalid_utf8(itr->name), itr->total_posts));
                } else {
                    result.emplace_back(std::make_pair(itr->name, itr->total_posts));
                }
            }
        }
        return result;
    }

    DEFINE_API(tags_plugin, get_tags_used_by_author) {
        PLUGIN_API_VALIDATE_ARGS(
            (string, author)
        );
        return pimpl->database().with_weak_read_lock([&]() {
            return pimpl->get_tags_used_by_author(author);
        });
    }

    DEFINE_API(tags_plugin, get_discussions_by_author_before_date) {
        std::vector<discussion> result;

        PLUGIN_API_VALIDATE_ARGS(
            (string,         author)
            (string,         start_permlink)
            (time_point_sec, before_date)
            (uint32_t,       limit)
            (uint32_t,       vote_limit, DEFAULT_VOTE_LIMIT)
            (uint32_t,       vote_offset, 0)
        );
        GOLOS_CHECK_LIMIT_PARAM(limit, 100);

        result.reserve(limit);

        if (before_date == time_point_sec()) {
            before_date = time_point_sec::maximum();
        }

        auto& db = pimpl->database();

        if (!db.has_index<comment_last_update_index>()) {
            return result;
        }

        return db.with_weak_read_lock([&]() {
            try {
                uint32_t count = 0;
                const auto& clu_cmt_idx = db.get_index<comment_last_update_index>().indices().get<golos::plugins::social_network::by_comment>();
                const auto& clu_idx = db.get_index<comment_last_update_index>().indices().get<golos::plugins::social_network::by_author_last_update>();

                auto itr = clu_idx.lower_bound(std::make_tuple(author, before_date));
                if (start_permlink.size()) {
                    const auto comment = db.find_comment(author, start_permlink);
                    if (comment == nullptr) {
                        return result;
                    }
                    auto clu_itr = clu_cmt_idx.find(comment->id);
                    if (clu_itr != clu_cmt_idx.end() && clu_itr->last_update < before_date) {
                        itr = clu_idx.iterator_to(*clu_itr);
                    }
                }

                for (; itr != clu_idx.end() && itr->author == author && count < limit; ++itr) {
                    if (itr->parent_author.size() == 0) {
                        result.push_back(pimpl->get_discussion(db.get_comment(itr->comment), vote_limit, vote_offset));
                        ++count;
                    }
                }

                return result;
            } FC_CAPTURE_AND_RETHROW((author)(start_permlink)(before_date)(limit))
        });
    }

    // Needed for correct work of golos::api::discussion_helper::set_pending_payout and etc api methods
    void fill_promoted(const golos::chain::database& db, discussion & d) {
        if (!db.has_index<tags::tag_index>()) {
            return;
        }

        const auto& cidx = db.get_index<tags::tag_index>().indices().get<tags::by_comment>();
        auto itr = cidx.lower_bound(d.id);
        if (itr != cidx.end() && itr->comment == d.id) {
            d.promoted = asset(itr->promoted_balance, SBD_SYMBOL);
        } else {
            d.promoted = asset(0, SBD_SYMBOL);
        }
    }

} } } // golos::plugins::tags
