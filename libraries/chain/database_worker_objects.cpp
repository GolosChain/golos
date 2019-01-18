#include <golos/chain/database.hpp>
#include <golos/chain/worker_objects.hpp>
#include <golos/chain/account_object.hpp>
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

    void database::process_worker_cashout() {
        if (!has_hardfork(STEEMIT_HARDFORK_0_21__1013)) {
            return;
        }

        const auto now = head_block_time();

        const auto& wto_idx = get_index<worker_techspec_index, by_next_cashout_time>();

        for (auto wto_itr = wto_idx.begin(); wto_itr != wto_idx.end() && wto_itr->next_cashout_time <= now; ++wto_itr) {
            const auto& wpo = get_worker_proposal(wto_itr->worker_proposal_author, wto_itr->worker_proposal_permlink);

            const auto& reward = wto_itr->development_cost / wto_itr->payments_count;

            const auto& gpo = get_dynamic_global_properties();
            modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.total_worker_fund_steem -= reward;
            });

            adjust_balance(get_account(wto_itr->worker), reward);

            if (wto_itr->finished_payments_count+1 == wto_itr->payments_count) {
                modify(*wto_itr, [&](worker_techspec_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = time_point_sec::maximum();
                });

                modify(wpo, [&](worker_proposal_object& wpo) {
                    wpo.state = worker_proposal_state::closed;
                });
            } else {
                modify(*wto_itr, [&](worker_techspec_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = now + wto.payments_interval;
                });
            }

            push_virtual_operation(worker_reward_operation(wto_itr->worker, wto_itr->author, to_string(wto_itr->permlink), reward));
        }
    }

} } // golos::chain
