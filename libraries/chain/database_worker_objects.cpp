#include <golos/chain/database.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

    const worker_proposal_object& database::get_worker_proposal(const comment_id_type& post) const { try {
        return get<worker_proposal_object, by_post>(post);
    } catch (const std::out_of_range &e) {
        const auto& comment = get_comment(post);
        GOLOS_THROW_MISSING_OBJECT("worker_proposal_object", fc::mutable_variant_object()("account",comment.author)("permlink",comment.permlink));
    } FC_CAPTURE_AND_RETHROW((post)) }

    const worker_proposal_object* database::find_worker_proposal(const comment_id_type& post) const {
        return find<worker_proposal_object, by_post>(post);
    }

    const worker_techspec_object& database::get_worker_techspec(const comment_id_type& post) const { try {
        return get<worker_techspec_object, by_post>(post);
    } catch (const std::out_of_range &e) {
        const auto& comment = get_comment(post);
        GOLOS_THROW_MISSING_OBJECT("worker_techspec_object", fc::mutable_variant_object()("account",comment.author)("permlink",comment.permlink));
    } FC_CAPTURE_AND_RETHROW((post)) }

    const worker_techspec_object* database::find_worker_techspec(const comment_id_type& post) const {
        return find<worker_techspec_object, by_post>(post);
    }

    const worker_techspec_object& database::get_worker_result(const comment_id_type& post) const { try {
        return get<worker_techspec_object, by_worker_result>(post);
    } catch (const std::out_of_range &e) {
        const auto& comment = get_comment(post);
        GOLOS_THROW_MISSING_OBJECT("worker_techspec_object", fc::mutable_variant_object()("account",comment.author)("worker_result_permlink",comment.permlink));
    } FC_CAPTURE_AND_RETHROW((post)) }

    const worker_techspec_object* database::find_worker_result(const comment_id_type& post) const {
        return find<worker_techspec_object, by_worker_result>(post);
    }

    template<typename ApproveMultiIndex, typename ApproveIndex>
    flat_map<worker_techspec_approve_state, int32_t> count_worker_approves(const database& _db, const comment_id_type& post) {
        flat_map<worker_techspec_approve_state, int32_t> result;

        const auto& approve_idx = _db.get_index<ApproveMultiIndex, ApproveIndex>();
        auto approve_itr = approve_idx.lower_bound(post);
        for (; approve_itr != approve_idx.end() && approve_itr->post == post; ++approve_itr) {
            auto witness = _db.find_witness(approve_itr->approver);
            if (witness && witness->schedule == witness_object::top19) {
                result[approve_itr->state]++;
            }
        }

        return result;
    }

    flat_map<worker_techspec_approve_state, int32_t> database::count_worker_techspec_approves(const comment_id_type& post) {
        return count_worker_approves<worker_techspec_approve_index, by_techspec_approver>(*this, post);
    }

    flat_map<worker_techspec_approve_state, int32_t> database::count_worker_payment_approves(const comment_id_type& post) {
        return count_worker_approves<worker_payment_approve_index, by_techspec_approver>(*this, post);
    }

    asset database::calculate_worker_techspec_consumption_per_day(const worker_techspec_object& wto) {
        auto payments_period = int64_t(wto.payments_interval) * wto.payments_count;
        uint128_t cost(wto.development_cost.amount.value);
        cost += wto.specification_cost.amount.value;
        cost *= fc::days(1).to_seconds();
        cost /= payments_period;
        return asset(cost.to_uint64(), STEEM_SYMBOL);
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
                modify(gpo, [&](dynamic_global_property_object& gpo) {
                    gpo.worker_consumption_per_day -= calculate_worker_techspec_consumption_per_day(*wto_itr);
                });

                clear_worker_payment_approves(*wto_itr);

                modify(*wto_itr, [&](worker_techspec_object& wto) {
                    wto.finished_payments_count++;
                    wto.next_cashout_time = time_point_sec::maximum();
                    wto.state = worker_techspec_state::payment_complete;
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

            const auto& wto_post = get_comment(wto_itr->post);

            adjust_balance(get_account(wto_post.author), author_reward);
            adjust_balance(get_account(wto_itr->worker), worker_reward);

            push_virtual_operation(techspec_reward_operation(wto_post.author, to_string(wto_post.permlink), author_reward));
            push_virtual_operation(worker_reward_operation(wto_itr->worker, wto_post.author, to_string(wto_post.permlink), worker_reward));
        }
    }

    void database::set_clear_old_worker_approves(bool clear_old_worker_approves) {
        _clear_old_worker_approves = clear_old_worker_approves;
    }

    template<typename ApproveMultiIndex, typename ApproveIndex>
    void clear_worker_approves(database& _db, const comment_id_type& post) {
        const auto& approve_idx = _db.get_index<ApproveMultiIndex, ApproveIndex>();
        auto approve_itr = approve_idx.lower_bound(post);
        while (approve_itr != approve_idx.end() && approve_itr->post == post) {
            const auto& approve = *approve_itr;
            ++approve_itr;
            _db.remove(approve);
        }
    }

    void database::clear_worker_techspec_approves(const worker_techspec_object& wto) {
        if (!_clear_old_worker_approves) {
            return;
        }

        clear_worker_approves<worker_techspec_approve_index, by_techspec_approver>(*this, wto.post);
    }

    void database::clear_worker_payment_approves(const worker_techspec_object& wto) {
        if (!_clear_old_worker_approves) {
            return;
        }

        clear_worker_approves<worker_payment_approve_index, by_techspec_approver>(*this, wto.post);
    }

    void database::close_worker_techspec(const worker_techspec_object& wto, worker_techspec_state closed_state) {
        bool has_approves = false;

        if (wto.state >= worker_techspec_state::approved) {
            const auto& wpo = get_worker_proposal(wto.worker_proposal_post);
            if (wpo.type == worker_proposal_type::task) {
                modify(wpo, [&](worker_proposal_object& wpo) {
                    wpo.state = worker_proposal_state::created;
                    wpo.approved_techspec_post = comment_id_type(-1);
                });
            }

            const auto& gpo = get_dynamic_global_properties();
            modify(gpo, [&](dynamic_global_property_object& gpo) {
                gpo.worker_consumption_per_day -= calculate_worker_techspec_consumption_per_day(wto);
            });

            has_approves = true;
        } else {
            const auto& wtao_idx = get_index<worker_techspec_approve_index, by_techspec_approver>();
            const auto& wpao_idx = get_index<worker_payment_approve_index, by_techspec_approver>();
            auto wtao_itr = wtao_idx.find(wto.post);
            auto wpao_itr = wpao_idx.find(wto.post);
            has_approves = (wtao_itr != wtao_idx.end() || wpao_itr != wpao_idx.end());
        }

        clear_worker_techspec_approves(wto);
        clear_worker_payment_approves(wto);

        if (closed_state == worker_techspec_state::closed_by_author && !has_approves) {
            remove(wto);
        } else {
            modify(wto, [&](worker_techspec_object& wto) {
                wto.next_cashout_time = time_point_sec::maximum();
                wto.state = closed_state;
            });
        }
    }

    void database::clear_expired_worker_objects() {
        if (!has_hardfork(STEEMIT_HARDFORK_0_21__1013)) {
            return;
        }

        const auto& mprops = get_witness_schedule_object().median_props;

        const auto now = head_block_time();

        const auto& idx = get_index<worker_techspec_index, by_post>();
        for (auto itr = idx.begin(); itr != idx.end(); itr++) {
            if (itr->state != worker_techspec_state::created) {
                continue;
            }

            const auto& wto_post = get_comment(itr->post);
            if (wto_post.created + mprops.worker_techspec_approve_term_sec > now) {
                break;
            }

            close_worker_techspec(*itr, worker_techspec_state::closed_by_expiration);

            push_virtual_operation(techspec_expired_operation(wto_post.author, to_string(wto_post.permlink), false));
        }

        const auto& result_idx = get_index<worker_techspec_index, by_worker_result>();
        for (auto result_itr = result_idx.begin(); result_itr != result_idx.end(); result_itr++) {
            if (result_itr->state != worker_techspec_state::complete) {
                continue;
            }

            const auto& result_post = get_comment(result_itr->worker_result_post);
            if (result_post.created + mprops.worker_result_approve_term_sec > now) {
                break;
            }

            close_worker_techspec(*result_itr, worker_techspec_state::closed_by_expiration);

            const auto& wto_post = get_comment(result_itr->post);
            push_virtual_operation(techspec_expired_operation(wto_post.author, to_string(wto_post.permlink), true));
        }
    }

} } // golos::chain
