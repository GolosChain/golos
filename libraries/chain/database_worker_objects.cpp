#include <golos/chain/database.hpp>
#include <golos/chain/worker_objects.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

    const worker_proposal_object& database::get_worker_proposal(const comment_id_type& post) const { try {
        return get<worker_proposal_object, by_post>(post);
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_proposal_object", fc::mutable_variant_object()("post",post));
    } FC_CAPTURE_AND_RETHROW((post)) }

    const worker_proposal_object* database::find_worker_proposal(const comment_id_type& post) const {
        return find<worker_proposal_object, by_post>(post);
    }

    const worker_techspec_object& database::get_worker_techspec(const comment_id_type& post) const { try {
        return get<worker_techspec_object, by_post>(post);
    } catch (const std::out_of_range &e) {
        GOLOS_THROW_MISSING_OBJECT("worker_techspec_object", fc::mutable_variant_object()("post",post));
    } FC_CAPTURE_AND_RETHROW((post)) }

    const worker_techspec_object* database::find_worker_techspec(const comment_id_type& post) const {
        return find<worker_techspec_object, by_post>(post);
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
            auto remaining_payments_count = wto_itr->payments_count - wto_itr->finished_payments_count;

            auto author_reward = wto_itr->specification_cost / wto_itr->payments_count;
            auto worker_reward = wto_itr->development_cost / wto_itr->payments_count;

            if (remaining_payments_count == 1) {
                author_reward = wto_itr->specification_cost - (author_reward * wto_itr->finished_payments_count);
                worker_reward = wto_itr->development_cost - (worker_reward * wto_itr->finished_payments_count);
            }

            const auto& gpo = get_dynamic_global_properties();

            if (gpo.total_worker_fund_steem < (author_reward + worker_reward)) {
                return;
            }

            if (remaining_payments_count == 1) {
                const auto& wpo_post = get_comment(wto_itr->worker_proposal_author, wto_itr->worker_proposal_permlink);
                const auto& wpo = get_worker_proposal(wpo_post.id);
                modify(wpo, [&](worker_proposal_object& wpo) {
                    wpo.state = worker_proposal_state::closed;
                });

                modify(gpo, [&](dynamic_global_property_object& gpo) {
                    gpo.worker_consumption_per_month -= wto_itr->month_consumption;
                });

                modify(*wto_itr, [&](worker_techspec_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = time_point_sec::maximum();
                    wto.month_consumption = asset(0, STEEM_SYMBOL);
                });
            } else {
                modify(*wto_itr, [&](worker_techspec_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = now + wto.payments_interval;
                });
            }

            modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.total_worker_fund_steem -= (author_reward + worker_reward);
            });

            adjust_balance(get_account(wto_itr->author), author_reward);
            adjust_balance(get_account(wto_itr->worker), worker_reward);

            const auto& wto_post = get_comment(wto_itr->post);
            push_virtual_operation(techspec_reward_operation(wto_itr->author, to_string(wto_post.permlink), author_reward));
            push_virtual_operation(worker_reward_operation(wto_itr->worker, wto_itr->author, to_string(wto_post.permlink), worker_reward));
        }
    }

    void database::update_worker_proposal_rshares(const comment_object& post, share_type net_rshares_new) {
        auto* wpo = find_worker_proposal(post.id);
        if (wpo) {
            modify(*wpo, [&](worker_proposal_object& wpo) {
                wpo.net_rshares = net_rshares_new;
            });
        }
    }

    void database::update_worker_techspec_rshares(const comment_object& post, share_type net_rshares_new) {
        auto* wto = find_worker_techspec(post.id);
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
