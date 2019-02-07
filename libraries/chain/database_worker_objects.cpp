#include <golos/chain/database.hpp>
#include <golos/chain/worker_objects.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

    const worker_proposal_object& database::get_worker_proposal(
        const account_name_type& author,
        const std::string& permlink
    ) const { try {
        return get<worker_proposal_object, by_permlink>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_proposal_object", fc::mutable_variant_object()("author",author)("permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_proposal_object& database::get_worker_proposal(
        const account_name_type& author,
        const shared_string& permlink
    ) const { try {
        return get<worker_proposal_object, by_permlink>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_proposal_object", fc::mutable_variant_object()("author",author)("permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_proposal_object* database::find_worker_proposal(
        const account_name_type& author,
        const std::string& permlink
    ) const {
        return find<worker_proposal_object, by_permlink>(std::make_tuple(author, permlink));
    }

    const worker_proposal_object* database::find_worker_proposal(
        const account_name_type& author,
        const shared_string& permlink
    ) const {
        return find<worker_proposal_object, by_permlink>(std::make_tuple(author, permlink));
    }

    const worker_techspec_object& database::get_worker_techspec(
        const account_name_type& author,
        const std::string& permlink
    ) const { try {
        return get<worker_techspec_object, by_permlink>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_techspec_object", fc::mutable_variant_object()("author",author)("permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_techspec_object& database::get_worker_techspec(
        const account_name_type& author,
        const shared_string& permlink
    ) const { try {
        return get<worker_techspec_object, by_permlink>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_techspec_object", fc::mutable_variant_object()("author",author)("permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_techspec_object* database::find_worker_techspec(
        const account_name_type& author,
        const std::string& permlink
    ) const {
        return find<worker_techspec_object, by_permlink>(std::make_tuple(author, permlink));
    }

    const worker_techspec_object* database::find_worker_techspec(
        const account_name_type& author,
        const shared_string& permlink
    ) const {
        return find<worker_techspec_object, by_permlink>(std::make_tuple(author, permlink));
    }

    const worker_techspec_object& database::get_worker_result(
        const account_name_type& author,
        const std::string& permlink
    ) const { try {
        return get<worker_techspec_object, by_worker_result>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_techspec_object", fc::mutable_variant_object()("author",author)("worker_result_permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_techspec_object& database::get_worker_result(
        const account_name_type& author,
        const shared_string& permlink
    ) const { try {
        return get<worker_techspec_object, by_worker_result>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_techspec_object", fc::mutable_variant_object()("author",author)("worker_result_permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_techspec_object* database::find_worker_result(
        const account_name_type& author,
        const std::string& permlink
    ) const {
        return find<worker_techspec_object, by_worker_result>(std::make_tuple(author, permlink));
    }

    const worker_techspec_object* database::find_worker_result(
        const account_name_type& author,
        const shared_string& permlink
    ) const {
        return find<worker_techspec_object, by_worker_result>(std::make_tuple(author, permlink));
    }

// Yet another temporary PoC
// TODO: This can be generalized to generate all 3: get_, find_ and throw_if_exists_
// methods with variable number of params (like DEFINE_/DECLARE_API + PLUGIN_API_VALIDATE_ARGS)
#define DB_DEFINE_THROW_IF_EXIST(O, T1, N1, T2, N2) \
    void database::throw_if_exists_##O(T1 N1, T2 N2) const { \
        if (nullptr != find_##O(N1, N2)) { \
            GOLOS_THROW_OBJECT_ALREADY_EXIST(#O, fc::mutable_variant_object()(#N1,N1)(#N2,N2)); \
        } \
    }

    DB_DEFINE_THROW_IF_EXIST(worker_intermediate, const account_name_type&, author, const std::string&, permlink);

    const worker_intermediate_object& database::get_worker_intermediate(
        const account_name_type& author,
        const std::string& permlink
    ) const { try {
        return get<worker_intermediate_object, by_permlink>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_intermediate", fc::mutable_variant_object()("author",author)("permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_intermediate_object& database::get_worker_intermediate(
        const account_name_type& author,
        const shared_string& permlink
    ) const { try {
        return get<worker_intermediate_object, by_permlink>(std::make_tuple(author, permlink));
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_intermediate", fc::mutable_variant_object()("author",author)("permlink",permlink));
    } FC_CAPTURE_AND_RETHROW((author)(permlink)) }

    const worker_intermediate_object* database::find_worker_intermediate(
        const account_name_type& author,
        const std::string& permlink
    ) const {
        return find<worker_intermediate_object, by_permlink>(std::make_tuple(author, permlink));
    }

    const worker_intermediate_object* database::find_worker_intermediate(
        const account_name_type& author,
        const shared_string& permlink
    ) const {
        return find<worker_intermediate_object, by_permlink>(std::make_tuple(author, permlink));
    }

    void database::process_worker_cashout() {
        if (!has_hardfork(STEEMIT_HARDFORK_0_21__1013)) {
            return;
        }

        const auto now = head_block_time();

        const auto& wto_idx = get_index<worker_techspec_index, by_next_cashout_time>();

        // Updating consumption

        asset consumption;

        auto month_sec = fc::days(30).to_seconds();
        for (auto wto_itr = wto_idx.begin(); wto_itr != wto_idx.end() && wto_itr->next_cashout_time < time_point_sec::maximum(); ++wto_itr) {
            auto payment = (wto_itr->specification_cost + wto_itr->development_cost) / wto_itr->payments_count;

            consumption += (payment * (month_sec / wto_itr->payments_interval));
        }

        {
            const auto& gpo = get_dynamic_global_properties();
            modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.worker_consumption_per_month = consumption;
            });
        }

        // Cashout
        for (auto wto_itr = wto_idx.begin(); wto_itr != wto_idx.end() && wto_itr->next_cashout_time <= now; ++wto_itr) {
            auto author_reward = wto_itr->specification_cost / wto_itr->payments_count;
            auto worker_reward = wto_itr->development_cost / wto_itr->payments_count;

            const auto& gpo = get_dynamic_global_properties();

            if (gpo.total_worker_fund_steem < (author_reward + worker_reward)) {
                return;
            }

            modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.total_worker_fund_steem -= (author_reward + worker_reward);
            });

            adjust_balance(get_account(wto_itr->author), author_reward);
            adjust_balance(get_account(wto_itr->worker), worker_reward);

            push_virtual_operation(techspec_reward_operation(wto_itr->author, to_string(wto_itr->permlink), author_reward));
            push_virtual_operation(worker_reward_operation(wto_itr->worker, wto_itr->author, to_string(wto_itr->permlink), worker_reward));

            if (wto_itr->finished_payments_count+1 == wto_itr->payments_count) {
                modify(*wto_itr, [&](worker_techspec_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = time_point_sec::maximum();
                });

                const auto& wpo = get_worker_proposal(wto_itr->worker_proposal_author, wto_itr->worker_proposal_permlink);
                modify(wpo, [&](worker_proposal_object& wpo) {
                    wpo.state = worker_proposal_state::closed;
                });
            } else {
                modify(*wto_itr, [&](worker_techspec_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = head_block_time() + wto.payments_interval;
                });
            }
        }
    }

    void database::update_worker_proposal_rshares(const comment_object& comment, share_type net_rshares_new) {
        auto* wpo = find_worker_proposal(comment.author, comment.permlink);
        if (wpo) {
            modify(*wpo, [&](worker_proposal_object& wpo) {
                wpo.net_rshares = net_rshares_new;
            });
        }
    }

    void database::update_worker_techspec_rshares(const comment_object& comment, share_type net_rshares_new) {
        auto* wto = find_worker_techspec(comment.author, comment.permlink);
        if (wto) {
            modify(*wto, [&](worker_techspec_object& wto) {
                wto.net_rshares = net_rshares_new;
            });
        }
    }

    void database::update_worker_techspec_approves(const worker_techspec_object& wto,
            const worker_techspec_approve_state& old_state,
            const worker_techspec_approve_state& new_state) {
        if (old_state == new_state) {
            return;
        }
        modify(wto, [&](worker_techspec_object& wto) {
            if (old_state == worker_techspec_approve_state::approve) {
                wto.approves--;
            }
            if (old_state == worker_techspec_approve_state::disapprove) {
                wto.disapproves--;
            }
            if (new_state == worker_techspec_approve_state::approve) {
                wto.approves++;
            }
            if (new_state == worker_techspec_approve_state::disapprove) {
                wto.disapproves++;
            }
        });
    }

} } // golos::chain
