#pragma once

#include <golos/application/applied_operation.hpp>
#include <golos/application/state.hpp>

#include <golos/chain/database.hpp>
#include <golos/chain/objects/proposal_object.hpp>
#include <golos/chain/objects/steem_objects.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <golos/chain/objects/history_object.hpp>

#include <golos/follow/follow_plugin.hpp>

#include <fc/api.hpp>
#include <fc/optional.hpp>
#include <fc/variant_object.hpp>
#include <fc/network/ip.hpp>

#include <boost/container/flat_set.hpp>

#include <golos/application/discussion_query.hpp>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace golos {
    namespace application {

        using namespace golos::chain;
        using namespace golos::protocol;

        struct api_context;

        struct scheduled_hardfork {
            hardfork_version hf_version;
            fc::time_point_sec live_time;
        };

        struct withdraw_route {
            std::string from_account;
            std::string to_account;
            uint16_t percent;
            bool auto_vest;
        };

        enum withdraw_route_type {
            incoming,
            outgoing,
            all
        };

        class database_api_impl;


/**
 * @brief The database_api class implements the RPC API for the chain database.
 *
 * This API exposes accessors on the database which query state tracked by a blockchain validating node. This API is
 * read-only; all modifications to the database must be performed via transactions. Transactions are broadcast via
 * the @ref network_broadcast_api.
 */
        class database_api {
        public:
            database_api(const golos::application::api_context &ctx);

            ~database_api();

            ///////////////////
            // Subscriptions //
            ///////////////////

            void set_subscribe_callback(std::function<void(const variant &)> cb, bool clear_filter);

            void set_pending_transaction_callback(std::function<void(const variant &)> cb);

            void set_block_applied_callback(std::function<void(const variant &block_header)> cb);

            /**
             * @brief Stop receiving any notifications
             *
             * This unsubscribes from all subscribed markets and objects.
             */
            void cancel_all_subscriptions();

            std::vector<tag_api_obj> get_trending_tags(std::string after_tag, uint32_t limit) const;

            std::vector<category_api_obj> get_trending_categories(std::string after, uint32_t limit) const;

            std::vector<category_api_obj> get_best_categories(std::string after, uint32_t limit) const;

            std::vector<category_api_obj> get_active_categories(std::string after, uint32_t limit) const;

            std::vector<category_api_obj> get_recent_categories(std::string after, uint32_t limit) const;

            std::vector<account_name_type> get_active_witnesses() const;

            std::vector<account_name_type> get_miner_queue() const;

            /**
            *  This API is a short-cut for returning all of the state required for a particular URL
            *  with a single query.
            */
            state get_state(std::string path) const;

            /////////////////////////////
            // Blocks and transactions //
            /////////////////////////////

            /**
             * @brief Retrieve a block header
             * @param block_num Height of the block whose header should be returned
             * @return header of the referenced block, or null if no matching block was found
             */
            optional<block_header> get_block_header(uint32_t block_num) const;

            /**
             * @brief Retrieve a full, signed block
             * @param block_num Height of the block to be returned
             * @return the referenced block, or null if no matching block was found
             */
            optional<signed_block> get_block(uint32_t block_num) const;

            /**
             *  @brief Get sequence of operations included/generated within a particular block
             *  @param block_num Height of the block whose generated virtual operations should be returned
             *  @param only_virtual Whether to only include virtual operations in returned results (default: true)
             *  @return sequence of operations included/generated within the block
             */
            std::vector<applied_operation> get_ops_in_block(uint32_t block_num, bool only_virtual = true) const;

            /////////////
            // Globals //
            /////////////

            /**
             * @brief Retrieve compile-time constants
             */
            fc::variant_object get_config() const;

            /**
             * @brief Retrieve database unused memory amount
             */
            size_t get_free_memory() const;

            /**
             * @brief Retrieve the current @ref dynamic_global_property_object
             */
            dynamic_global_property_object get_dynamic_global_properties() const;

            chain_properties<0, 17, 0> get_chain_properties() const;

            price<0, 17, 0> get_current_median_history_price() const;

            feed_history_api_obj get_feed_history() const;

            witness_schedule_object get_witness_schedule() const;

            hardfork_version get_hardfork_version() const;

            scheduled_hardfork get_next_scheduled_hardfork() const;

            reward_fund_object get_reward_fund(string name) const;

            asset<0, 17, 0> get_name_cost(std::string name) const;

            //////////////
            // Accounts //
            //////////////

            std::vector<extended_account> get_accounts(std::vector<std::string> names) const;

            /**
             * @brief Get a list of accounts by name
             * @param account_names Names of the accounts to retrieve
             * @return The accounts holding the provided names
             *
             * This function has semantics identical to @ref get_objects
             */
            std::vector<optional<account_api_obj>> lookup_account_names(const std::vector<std::string> &account_names) const;

            /**
             * @brief Get names and IDs for registered accounts
             * @param lower_bound_name Lower bound of the first name to return
             * @param limit Maximum number of results to return -- must not exceed 1000
             * @return Map of account names to corresponding IDs
             */
            std::set<std::string> lookup_accounts(const std::string &lower_bound_name, uint32_t limit) const;

            //////////////
            // Balances //
            //////////////

            /**
             * @brief Get an account's balances in various assets
             * @param name of the account to get balances for
             * @param assets names of the assets to get balances of; if empty, get all assets account has a balance in
             * @return Balances of the account
             */
            vector<asset<0, 17, 0>> get_account_balances(account_name_type name, const flat_set<asset_name_type> &assets) const;

            /**
             * @brief Get the total number of accounts registered with the blockchain
             */
            uint64_t get_account_count() const;

            std::vector<owner_authority_history_api_obj> get_owner_history(std::string account) const;

            optional<account_recovery_request_api_obj> get_recovery_request(std::string account) const;

            optional<escrow_object> get_escrow(std::string from, uint32_t escrow_id) const;

            std::vector<withdraw_route> get_withdraw_routes(std::string account, withdraw_route_type type = outgoing) const;

            optional<account_bandwidth_object> get_account_bandwidth(std::string account, bandwidth_type type) const;

            std::vector<savings_withdraw_api_obj> get_savings_withdraw_from(std::string account) const;

            std::vector<savings_withdraw_api_obj> get_savings_withdraw_to(std::string account) const;

            vector<vesting_delegation_object> get_vesting_delegations(string account, string from, uint32_t limit = 100) const;

            vector<vesting_delegation_expiration_object> get_expiring_vesting_delegations(string account, time_point_sec from, uint32_t limit = 100) const;

            ///////////////
            // Witnesses //
            ///////////////

            /**
             * @brief Get a list of witnesses by ID
             * @param witness_ids IDs of the witnesses to retrieve
             * @return The witnesses corresponding to the provided IDs
             *
             * This function has semantics identical to @ref get_objects
             */
            std::vector<optional<witness_api_obj>> get_witnesses(const std::vector<witness_object::id_type> &witness_ids) const;

            std::vector<convert_request_object> get_conversion_requests(const std::string &account_name) const;

            /**
             * @brief Get the witness owned by a given account
             * @param account The name of the account whose witness should be retrieved
             * @return The witness object, or null if the account does not have a witness
             */
            fc::optional<witness_api_obj> get_witness_by_account(std::string account_name) const;

            /**
             *  This method is used to fetch witnesses with pagination.
             *
             *  @return an array of `count` witnesses sorted by total votes after witness `from` with at most `limit' results.
             */
            std::vector<witness_api_obj> get_witnesses_by_vote(std::string from, uint32_t limit) const;

            /**
             * @brief Get names and IDs for registered witnesses
             * @param lower_bound_name Lower bound of the first name to return
             * @param limit Maximum number of results to return -- must not exceed 1000
             * @return Map of witness names to corresponding IDs
             */
            std::set<account_name_type> lookup_witness_accounts(const std::string &lower_bound_name, uint32_t limit) const;

            /**
             * @brief Get the total number of witnesses registered with the blockchain
             */
            uint64_t get_witness_count() const;

            ////////////
            // Assets //
            ////////////

            /**
             * @brief Get a list of assets by ID
             * @param asset_symbols IDs of the assets to retrieve
             * @return The assets corresponding to the provided IDs
             *
             * This function has semantics identical to @ref get_objects
             */
            vector<optional<asset_object>> get_assets(const vector<asset_name_type> &asset_symbols) const;

            vector<optional<asset_object>> get_assets_by_issuer(string issuer) const;

            vector<optional<asset_dynamic_data_object>> get_assets_dynamic_data(const vector<asset_name_type> &asset_symbols) const;

            vector<optional<asset_bitasset_data_object>> get_bitassets_data(const vector<asset_name_type> &asset_symbols) const;

            /**
             * @brief Get assets alphabetically by symbol name
             * @param lower_bound_symbol Lower bound of symbol names to retrieve
             * @param limit Maximum number of assets to fetch (must not exceed 100)
             * @return The assets found
             */
            vector<asset_object> list_assets(const asset_name_type &lower_bound_symbol, uint32_t limit) const;

            ////////////////////////////
            // Authority / Validation //
            ////////////////////////////

            /// @brief Get a hexdump of the serialized binary form of a transaction
            std::string get_transaction_hex(const signed_transaction &trx) const;

            annotated_signed_transaction get_transaction(transaction_id_type trx_id) const;

            /**
             *  This API will take a partially signed transaction and a set of public keys that the owner has the ability to sign for
             *  and return the minimal subset of public keys that should add signatures to the transaction.
             */
            std::set<public_key_type> get_required_signatures(const signed_transaction &trx, const flat_set<public_key_type> &available_keys) const;

            /**
             *  This method will return the set of all public keys that could possibly sign for a given transaction.  This call can
             *  be used by wallets to filter their set of public keys to just the relevant subset prior to calling @ref get_required_signatures
             *  to get the minimum subset.
             */
            std::set<public_key_type> get_potential_signatures(const signed_transaction &trx) const;

            /**
             * @return true of the @ref trx has all of the required signatures, otherwise throws an exception
             */
            bool verify_authority(const signed_transaction &trx) const;

            /*
             * @return true if the signers have enough authority to authorize an account
             */
            bool verify_account_authority(const std::string &name, const flat_set<public_key_type> &signers) const;

            /**
             *  if permlink is "" then it will return all votes for author
             */
            std::vector<vote_state> get_active_votes(std::string author, std::string permlink) const;

            std::vector<account_vote> get_account_votes(std::string voter) const;


            discussion get_content(std::string author, std::string permlink) const;

            std::vector<discussion> get_content_replies(std::string parent, std::string parent_permlink) const;

            /**
             * Used to retrieve top 1000 tags list used by an author sorted by most frequently used
             * @param author select tags of this author
             * @return vector of top 1000 tags used by an author sorted by most frequently used
             **/
            std::vector<pair<std::string, uint32_t>> get_tags_used_by_author(const std::string &author) const;

            /**
             * Used to retrieve the list of first payout discussions sorted by rshares^2 amount
             * @param query @ref discussion_query
             * @return vector of first payout mode discussions sorted by rshares^2 amount
             **/
            std::vector<discussion> get_discussions_by_trending(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by created time
             * @param query @ref discussion_query
             * @return vector of discussions sorted by created time
             **/
            std::vector<discussion> get_discussions_by_created(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by last activity time
             * @param query @ref discussion_query
             * @return vector of discussions sorted by last activity time
             **/
            std::vector<discussion> get_discussions_by_active(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by cashout time
             * @param query @ref discussion_query
             * @return vector of discussions sorted by last cashout time
             **/
            std::vector<discussion> get_discussions_by_cashout(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by net rshares amount
             * @param query @ref discussion_query
             * @return vector of discussions sorted by net rshares amount
             **/
            std::vector<discussion> get_discussions_by_payout(const discussion_query &query) const;

            std::vector<discussion> get_post_discussions_by_payout(const discussion_query &query) const;

            std::vector<discussion> get_comment_discussions_by_payout(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by direct votes amount
             * @param query @ref discussion_query
             * @return vector of discussions sorted by direct votes amount
             **/
            std::vector<discussion> get_discussions_by_votes(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by children posts amount
             * @param query @ref discussion_query
             * @return vector of discussions sorted by children posts amount
             **/
            std::vector<discussion> get_discussions_by_children(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by hot amount
             * @param query @ref discussion_query
             * @return vector of discussions sorted by hot amount
             **/
            std::vector<discussion> get_discussions_by_hot(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions from the feed of a specific author
             * @param query @ref discussion_query
             * @attention @ref discussion_query#select_authors must be set and must contain the @ref discussion_query#start_author param if the last one is not null
             * @return vector of discussions from the feed of authors in @ref discussion_query#select_authors
             **/
            std::vector<discussion> get_discussions_by_feed(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions from the blog of a specific author
             * @param query @ref discussion_query
             * @attention @ref discussion_query#select_authors must be set and must contain the @ref discussion_query#start_author param if the last one is not null
             * @return vector of discussions from the blog of authors in @ref discussion_query#select_authors
             **/
            std::vector<discussion> get_discussions_by_blog(const discussion_query &query) const;

            std::vector<discussion> get_discussions_by_comments(const discussion_query &query) const;

            /**
             * Used to retrieve the list of discussions sorted by promoted balance amount
             * @param query @ref discussion_query
             * @return vector of discussions sorted by promoted balance amount
             **/
            std::vector<discussion> get_discussions_by_promoted(const discussion_query &query) const;


            /**
             *  Return the active discussions with the highest cumulative pending payouts without respect to category, total
             *  pending payout means the pending payout of all children as well.
             */
            std::vector<discussion> get_replies_by_last_update(account_name_type start_author, std::string start_permlink, uint32_t limit) const;


            /**
             *  This method is used to fetch all posts/comments by start_author that occur after before_date and start_permlink with up to limit being returned.
             *
             *  If start_permlink is empty then only before_date will be considered. If both are specified the eariler to the two metrics will be used. This
             *  should allow easy pagination.
             */
            std::vector<discussion> get_discussions_by_author_before_date(std::string author, std::string start_permlink, time_point_sec before_date, uint32_t limit) const;

            /**
             *  Account operations have sequence numbers from 0 to N where N is the most recent operation. This method
             *  returns operations in the range [from-limit, from]
             *
             *  @param from - the absolute sequence number, -1 means most recent, limit is the number of operations before from.
             *  @param limit - the maximum number of items that can be queried (0 to 1000], must be less than from
             */
            std::map<uint32_t, applied_operation> get_account_history(std::string account, uint64_t from, uint32_t limit) const;

            /**
             * Used to retrieve comment payout window extension cost by time
             * @param author comment author
             * @param permlink comment permlink
             * @param time deadline time the payout window pretends to be extended for
             * @return SBD amount required to set payout window duration up to time passed
             */

            asset<0, 17, 0> get_payout_extension_cost(const string &author, const string &permlink, fc::time_point_sec time) const;

            /**
             * Used o retrieve comment payout window extension time by cost
             * @param author comment author
             * @param permlink comment permlink
             * @param cost SBD amount pretended to be spent on extension
             * @return deadline time the payout window pretends to be extended for
             */

            fc::time_point_sec get_payout_extension_time(const string &author, const string &permlink, asset<0, 17, 0> cost) const;

            ///////////////////////////
            // Proposed transactions //
            ///////////////////////////

            /**
             *  @return the set of proposed transactions relevant to the specified account id.
             */
            vector<proposal_object> get_proposed_transactions(account_name_type name) const;

            ////////////////////////////
            // Handlers - not exposed //
            ////////////////////////////
            void on_api_startup();

            discussion get_discussion(comment_object::id_type, uint32_t truncate_body = 0) const;

        private:
            void set_pending_payout(discussion &d) const;

            void set_url(discussion &d) const;

            template<typename Object,
                    typename DatabaseIndex,
                    typename DiscussionIndex,
                    typename CommentIndex,
                    typename Index,
                    typename StartItr
            > std::multimap<Object, discussion, DiscussionIndex> get_discussions(
                    const discussion_query &query,
                    const std::string &tag,
                    comment_object::id_type parent,
                    const Index &tidx,
                    StartItr tidx_itr,
                    const std::function<bool(const comment_api_object &)> &filter,
                    const std::function<bool(const comment_api_object &)> &exit,
                    const std::function<bool(const Object &)> &tag_exit,
                    bool ignore_parent = false) const;


            template<typename Object,
                    typename DatabaseIndex,
                    typename DiscussionIndex,
                    typename CommentIndex,
                    typename ...Args
            > std::multimap<Object, discussion, DiscussionIndex> select(
                    const std::set<std::string> &select_set,
                    const discussion_query &query,
                    comment_object::id_type parent,
                    const std::function<bool(const comment_api_object &)> &filter,
                    const std::function<bool(const comment_api_object &)> &exit,
                    const std::function<bool(const Object &)> &exit2,
                    Args... args) const;

            template<typename DatabaseIndex,
                    typename DiscussionIndex
            > std::vector<discussion> feed(const std::set<string> &select_set,
                    const discussion_query &query,
                    const std::string &start_author,
                    const std::string &start_permlink) const;

            template<typename DatabaseIndex,
                    typename DiscussionIndex
            > std::vector<discussion> blog(const std::set<string> &select_set,
                    const discussion_query &query,
                    const std::string &start_author,
                    const std::string &start_permlink) const;


            comment_object::id_type get_parent(const discussion_query &q) const;

            void recursively_fetch_content(state &_state, discussion &root, std::set<std::string> &referenced_accounts) const;

            std::shared_ptr<database_api_impl> my;
        };
    }
}

FC_REFLECT((golos::application::scheduled_hardfork), (hf_version)(live_time));
FC_REFLECT((golos::application::withdraw_route), (from_account)(to_account)(percent)(auto_vest));

FC_REFLECT_ENUM(golos::application::withdraw_route_type, (incoming)(outgoing)(all));

FC_API(golos::application::database_api,
// Subscriptions
        (set_subscribe_callback)
                (set_pending_transaction_callback)
                (set_block_applied_callback)
                (cancel_all_subscriptions)

                // Tags
                (get_trending_tags)
                (get_tags_used_by_author)
                (get_discussions_by_payout)
                (get_post_discussions_by_payout)
                (get_comment_discussions_by_payout)
                (get_discussions_by_trending)
                (get_discussions_by_created)
                (get_discussions_by_active)
                (get_discussions_by_cashout)
                (get_discussions_by_votes)
                (get_discussions_by_children)
                (get_discussions_by_hot)
                (get_discussions_by_feed)
                (get_discussions_by_blog)
                (get_discussions_by_comments)
                (get_discussions_by_promoted)

                // Blocks and transactions
                (get_block_header)
                (get_block)
                (get_ops_in_block)
                (get_trending_categories)
                (get_best_categories)
                (get_active_categories)
                (get_recent_categories)

                // Globals
                (get_config)
                (get_free_memory)
                (get_dynamic_global_properties)
                (get_chain_properties)
                (get_feed_history)
                (get_current_median_history_price)
                (get_witness_schedule)
                (get_hardfork_version)
                (get_next_scheduled_hardfork)
                (get_reward_fund)
                (get_state)
                (get_name_cost)

                // Accounts
                (get_accounts)
                (lookup_account_names)
                (lookup_accounts)
                (get_account_count)
                (get_conversion_requests)
                (get_account_history)
                (get_owner_history)
                (get_recovery_request)
                (get_escrow)
                (get_withdraw_routes)
                (get_account_bandwidth)
                (get_savings_withdraw_from)
                (get_savings_withdraw_to)
                (get_vesting_delegations)
                (get_expiring_vesting_delegations)

                // Balances
                (get_account_balances)

                // Assets
                (get_assets)
                (get_assets_by_issuer)
                (get_assets_dynamic_data)
                (get_bitassets_data)
                (list_assets)

                // Authority / validation
                (get_transaction_hex)
                (get_transaction)
                (get_required_signatures)
                (get_potential_signatures)
                (verify_authority)
                (verify_account_authority)

                // Votes
                (get_active_votes)
                (get_account_votes)

                // Content
                (get_content)
                (get_content_replies)
                (get_discussions_by_author_before_date)
                (get_replies_by_last_update)
                (get_payout_extension_cost)
                (get_payout_extension_time)

                // Witnesses
                (get_witnesses)
                (get_witness_by_account)
                (get_witnesses_by_vote)
                (lookup_witness_accounts)
                (get_witness_count)
                (get_active_witnesses)
                (get_miner_queue)

                // Proposed transactions
                (get_proposed_transactions)
)
