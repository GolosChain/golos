#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/worker_objects.hpp>

namespace golos { namespace chain {

    void worker_proposal_evaluator::do_apply(const worker_proposal_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_proposal_operation");

        const auto& comment = _db.get_comment(o.author, o.permlink);

        GOLOS_CHECK_LOGIC(comment.parent_author == STEEMIT_ROOT_POST_PARENT,
            logic_exception::worker_proposal_can_be_created_only_on_post,
            "Worker proposal can be created only on post");

        const auto now = _db.head_block_time();

        const auto* wpo = _db.find_worker_proposal(o.author, o.permlink);

        if (wpo) {
            _db.modify(*wpo, [&](worker_proposal_object& wpo) {
                wpo.type = o.type;
                wpo.modified = now;
            });
            return;
        }

        _db.create<worker_proposal_object>([&](worker_proposal_object& wpo) {
            wpo.author = o.author;
            wpo.permlink = comment.permlink;
            wpo.type = o.type;
            wpo.state = worker_proposal_state::created;
            wpo.created = now;
            wpo.net_rshares = comment.net_rshares;
        });
    }

    void worker_proposal_delete_evaluator::do_apply(const worker_proposal_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_proposal_delete_operation");

        const auto& wpo = _db.get_worker_proposal(o.author, o.permlink);

        GOLOS_CHECK_LOGIC(wpo.state == worker_proposal_state::created,
            logic_exception::cannot_delete_worker_proposal_with_approved_techspec,
            "Cannot delete worker proposal with approved techspec");

        const auto& wto_idx = _db.get_index<worker_techspec_index, by_worker_proposal>();
        auto wto_itr = wto_idx.find(std::make_tuple(o.author, o.permlink));
        GOLOS_CHECK_LOGIC(wto_itr == wto_idx.end(),
            logic_exception::cannot_delete_worker_proposal_with_techspecs,
            "Cannot delete worker proposal with techspecs");

        _db.remove(wpo);
    }

    void worker_techspec_evaluator::do_apply(const worker_techspec_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_techspec_operation");

        const auto now = _db.head_block_time();

        const auto& comment = _db.get_comment(o.author, o.permlink);

        GOLOS_CHECK_LOGIC(comment.parent_author == STEEMIT_ROOT_POST_PARENT,
            logic_exception::worker_techspec_can_be_created_only_on_post,
            "Worker techspec can be created only on post");

        const auto* wpo = _db.find_worker_proposal(o.worker_proposal_author, o.worker_proposal_permlink);

        GOLOS_CHECK_LOGIC(wpo,
            logic_exception::worker_techspec_can_be_created_only_for_existing_proposal,
            "Worker techspec can be created only for existing proposal");

        GOLOS_CHECK_LOGIC(wpo->state == worker_proposal_state::created,
            logic_exception::this_worker_proposal_already_has_approved_techspec,
            "This worker proposal already has approved techspec");

        const auto& wto_idx = _db.get_index<worker_techspec_index, by_worker_proposal>();
        auto wto_itr = wto_idx.find(std::make_tuple(o.worker_proposal_author, o.worker_proposal_permlink, o.author));
        if (wto_itr != wto_idx.end()) {
            GOLOS_CHECK_LOGIC(o.permlink == to_string(wto_itr->permlink),
                logic_exception::there_already_is_your_techspec_with_another_permlink,
                "There already is your techspec with another permlink");

            GOLOS_CHECK_LOGIC(o.specification_cost.symbol == wto_itr->specification_cost.symbol,
                logic_exception::cannot_change_cost_symbol,
                "Cannot change cost symbol");
            GOLOS_CHECK_LOGIC(o.development_cost.symbol == wto_itr->development_cost.symbol,
                logic_exception::cannot_change_cost_symbol,
                "Cannot change cost symbol");

            _db.modify(*wto_itr, [&](worker_techspec_object& wto) {
                wto.modified = now;
                wto.specification_cost = o.specification_cost;
                wto.specification_eta = o.specification_eta;
                wto.development_cost = o.development_cost;
                wto.development_eta = o.development_eta;
                wto.payments_count = o.payments_count;
                wto.payments_interval = o.payments_interval;
            });

            return;
        }

        _db.create<worker_techspec_object>([&](worker_techspec_object& wto) {
            wto.author = o.author;
            wto.permlink = comment.permlink;
            wto.worker_proposal_author = o.worker_proposal_author;
            from_string(wto.worker_proposal_permlink, o.worker_proposal_permlink);
            wto.created = now;
            wto.net_rshares = comment.net_rshares;
            wto.specification_cost = o.specification_cost;
            wto.specification_eta = o.specification_eta;
            wto.development_cost = o.development_cost;
            wto.development_eta = o.development_eta;
            wto.payments_count = o.payments_count;
            wto.payments_interval = o.payments_interval;
        });
    }

    void worker_techspec_delete_evaluator::do_apply(const worker_techspec_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_techspec_delete_operation");

        const auto& wto = _db.get_worker_techspec(o.author, o.permlink);

        const auto& wpo = _db.get_worker_proposal(wto.worker_proposal_author, wto.worker_proposal_permlink);

        GOLOS_CHECK_LOGIC(wpo.state < worker_proposal_state::payment,
            logic_exception::cannot_delete_worker_techspec_for_paying_proposal,
            "Cannot delete worker techspec for paying proposal");

        if (wpo.approved_techspec_author == wto.author && wpo.approved_techspec_permlink == wto.permlink) {
            _db.modify(wpo, [&](worker_proposal_object& wpo) {
                wpo.state = worker_proposal_state::created;
            });
        }

        _db.remove(wto);
    }

    void worker_techspec_approve_evaluator::do_apply(const worker_techspec_approve_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_techspec_approve_operation");

        auto approver_witness = _db.get_witness(o.approver);
        GOLOS_CHECK_LOGIC(approver_witness.schedule == witness_object::top19,
            logic_exception::approver_of_techspec_should_be_in_top19_of_witnesses,
            "Approver of techspec should be in Top 19 of witnesses");

        const auto& wto = _db.get_worker_techspec(o.author, o.permlink);

        const auto& wpo = _db.get_worker_proposal(wto.worker_proposal_author, wto.worker_proposal_permlink);

        GOLOS_CHECK_LOGIC(wpo.approved_techspec_permlink.empty(),
            logic_exception::techspec_is_already_approved,
            "Techspec is already approved");

        const auto& wtao_idx = _db.get_index<worker_techspec_approve_index, by_techspec_approver>();
        auto wtao_itr = wtao_idx.find(std::make_tuple(o.author, o.permlink, o.approver));

        if (o.state == worker_techspec_approve_state::abstain) {
            if (wtao_itr != wtao_idx.end()) {
                _db.update_worker_techspec_approves(wto, wtao_itr->state, worker_techspec_approve_state::abstain);

                _db.remove(*wtao_itr);
            }

            return;
        }

        if (wtao_itr != wtao_idx.end()) {
            _db.update_worker_techspec_approves(wto, wtao_itr->state, o.state);

            _db.modify(*wtao_itr, [&](worker_techspec_approve_object& wtao) {
                wtao.state = o.state;
            });
        } else {
            _db.update_worker_techspec_approves(wto, worker_techspec_approve_state::abstain, o.state);

            _db.create<worker_techspec_approve_object>([&](worker_techspec_approve_object& wtao) {
                wtao.approver = o.approver;
                wtao.author = o.author;
                from_string(wtao.permlink, o.permlink);
                wtao.state = o.state;
            });
        }

        if (o.state == worker_techspec_approve_state::approve) {
            auto approvers = 0;

            wtao_itr = wtao_idx.lower_bound(std::make_tuple(o.author, o.permlink));
            for (; wtao_itr != wtao_idx.end()
                    && wtao_itr->author == o.author && to_string(wtao_itr->permlink) == o.permlink; ++wtao_itr) {
                auto witness = _db.find_witness(wtao_itr->approver);
                if (witness && witness->schedule == witness_object::top19 && wtao_itr->state == worker_techspec_approve_state::approve) {
                    approvers++;
                }
            }

            if (approvers < STEEMIT_MAJOR_VOTED_WITNESSES) {
                return;
            }

            _db.modify(wpo, [&](worker_proposal_object& wpo) {
                wpo.approved_techspec_author = o.author;
                from_string(wpo.approved_techspec_permlink, o.permlink);
                wpo.state = worker_proposal_state::techspec;
            });
        }
    }

    void worker_intermediate_evaluator::do_apply(const worker_intermediate_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_intermediate_operation");

        GOLOS_CHECK_OBJECT_MISSING(_db, worker_intermediate, o.author, o.permlink);

        const auto& wto = _db.get_worker_techspec(o.author, o.worker_techspec_permlink);

        GOLOS_CHECK_LOGIC(wto.worker_result_permlink.empty(),
            logic_exception::worker_techspec_already_has_final_result,
            "Worker techspec already has final result");

        _db.create<worker_intermediate_object>([&](worker_intermediate_object& wio) {
            wio.author = o.author;
            from_string(wio.permlink, o.permlink);
            from_string(wio.worker_techspec_permlink, o.worker_techspec_permlink);
            wio.created = _db.head_block_time();
        });
    }

    void worker_intermediate_delete_evaluator::do_apply(const worker_intermediate_delete_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_intermediate_delete_operation");

        const auto& wio = _db.get_worker_intermediate(o.author, o.permlink);

        const auto& wto = _db.get_worker_techspec(o.author, wio.worker_techspec_permlink);

        GOLOS_CHECK_LOGIC(wto.worker_result_permlink.empty(),
            logic_exception::cannot_delete_intermediate_for_techspec_with_final_result,
            "Cannot delete intermediate for techspec with final result");

        _db.remove(wio);
    }

    void worker_result_fill_evaluator::do_apply(const worker_result_fill_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_result_fill_operation");

        const auto now = _db.head_block_time();

        GOLOS_CHECK_LOGIC(o.completion_date <= now,
            logic_exception::work_completion_date_cannot_be_in_future,
            "Work completion date cannot be in future");

        const auto& comment = _db.get_comment(o.author, o.permlink);

        GOLOS_CHECK_LOGIC(comment.parent_author == STEEMIT_ROOT_POST_PARENT,
            logic_exception::worker_result_can_be_created_only_on_post,
            "Worker result can be created only on post");

        const auto& wto = _db.get_worker_techspec(o.author, o.worker_techspec_permlink);

        const auto* wto_result = _db.find_worker_result(o.author, o.permlink);
        GOLOS_CHECK_LOGIC(!wto_result,
            logic_exception::this_post_already_used_as_worker_result,
            "This post already used as worker result");

        const auto& wpo = _db.get_worker_proposal(wto.worker_proposal_author, wto.worker_proposal_permlink);

        GOLOS_CHECK_LOGIC(wpo.approved_techspec_author == o.author && wpo.approved_techspec_permlink == wto.permlink,
            logic_exception::worker_result_can_be_created_only_for_techspec_in_work,
            "Worker result can be created only for techspec in work");
        if (wpo.type == worker_proposal_type::premade_work) {
            GOLOS_CHECK_LOGIC(wpo.state == worker_proposal_state::techspec,
                logic_exception::worker_result_can_be_created_only_for_techspec_in_work,
                "Worker result can be created only for techspec in work");
        } else {
            GOLOS_CHECK_LOGIC(wpo.state == worker_proposal_state::work,
                logic_exception::worker_result_can_be_created_only_for_techspec_in_work,
                "Worker result can be created only for techspec in work");
        }

        _db.modify(wto, [&](worker_techspec_object& wto) {
            from_string(wto.worker_result_permlink, o.permlink);

            if (o.completion_date != time_point_sec::min()) {
                wto.completion_date = o.completion_date;
            } else {
                wto.completion_date = now;
            }
        });

        _db.modify(wpo, [&](worker_proposal_object& wpo) {
            wpo.state = worker_proposal_state::witnesses_review;
        });
    }

    void worker_result_clear_evaluator::do_apply(const worker_result_clear_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_result_clear_operation");

        const auto& wto = _db.get_worker_result(o.author, o.permlink);

        const auto& wpo = _db.get_worker_proposal(wto.worker_proposal_author, wto.worker_proposal_permlink);

        GOLOS_CHECK_LOGIC(wpo.state < worker_proposal_state::payment,
            logic_exception::cannot_delete_worker_result_for_paying_proposal,
            "Cannot delete worker result for paying proposal");

        _db.modify(wpo, [&](worker_proposal_object& wpo) {
            wpo.state = worker_proposal_state::work;
        });

        _db.modify(wto, [&](worker_techspec_object& wto) {
            wto.worker_result_permlink.clear();
            wto.completion_date = time_point::min();
        });
    }

    void worker_result_approve_evaluator::do_apply(const worker_result_approve_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_result_approve_operation");

        auto approver_witness = _db.get_witness(o.approver);
        GOLOS_CHECK_LOGIC(approver_witness.schedule == witness_object::top19,
            logic_exception::approver_of_result_should_be_in_top19_of_witnesses,
            "Approver of result should be in Top 19 of witnesses");

        const auto& wto = _db.get_worker_result(o.author, o.permlink);

        const auto& wpo = _db.get_worker_proposal(wto.worker_proposal_author, wto.worker_proposal_permlink);

        if (o.state == worker_techspec_approve_state::disapprove) {
            GOLOS_CHECK_LOGIC(wpo.state == worker_proposal_state::work || wpo.state == worker_proposal_state::witnesses_review,
                logic_exception::worker_proposal_should_be_in_work_or_review_state_to_disapprove,
                "Worker proposal should be in work or review state to disapprove");
        } else if (o.state == worker_techspec_approve_state::approve) {
            GOLOS_CHECK_LOGIC(wpo.state == worker_proposal_state::witnesses_review,
                logic_exception::worker_proposal_should_be_in_review_state_to_approve,
                "Worker proposal should be in review state to approve");
        }

        const auto& wrao_idx = _db.get_index<worker_result_approve_index, by_result_approver>();
        auto wrao_itr = wrao_idx.find(std::make_tuple(o.author, o.permlink, o.approver));

        if (o.state == worker_techspec_approve_state::abstain) {
            if (wrao_itr != wrao_idx.end()) {
                _db.remove(*wrao_itr);
            }

            return;
        }

        if (wrao_itr != wrao_idx.end()) {
            _db.modify(*wrao_itr, [&](worker_result_approve_object& wrao) {
                wrao.state = o.state;
            });
        } else {
            _db.create<worker_result_approve_object>([&](worker_result_approve_object& wrao) {
                wrao.approver = o.approver;
                wrao.author = o.author;
                from_string(wrao.permlink, o.permlink);
                wrao.state = o.state;
            });
        }

        auto count_approvers = [&](auto state) {
            auto approvers = 0;
            wrao_itr = wrao_idx.lower_bound(std::make_tuple(o.author, o.permlink));
            for (; wrao_itr != wrao_idx.end()
                    && wrao_itr->author == o.author && to_string(wrao_itr->permlink) == o.permlink; ++wrao_itr) {
                auto witness = _db.find_witness(wrao_itr->approver);
                if (witness && witness->schedule == witness_object::top19 && wrao_itr->state == state) {
                    approvers++;
                }
            }
            return approvers;
        };

        const auto& gpo = _db.get_dynamic_global_properties();

        if (o.state == worker_techspec_approve_state::disapprove) {
            auto disapprovers = count_approvers(worker_techspec_approve_state::disapprove);

            if (disapprovers >= STEEMIT_SUPER_MAJOR_VOTED_WITNESSES) {
                _db.modify(wpo, [&](worker_proposal_object& wpo) {
                    wpo.state = worker_proposal_state::closed;
                });
            }
        } else if (o.state == worker_techspec_approve_state::approve) {
            auto month_sec = fc::days(30).to_seconds();
            auto payments_period = int64_t(wto.payments_interval) * wto.payments_count;
            auto months = payments_period / month_sec;

            uint128_t cost(wto.development_cost.amount.value);
            cost += wto.specification_cost.amount.value;
            cost *= std::min(month_sec, payments_period);
            cost /= payments_period;
            auto consumption = asset(cost.to_uint64(), STEEM_SYMBOL);

            auto prognosis_funds = gpo.total_worker_fund_steem + (-gpo.worker_consumption_per_month - consumption + gpo.worker_revenue_per_month) * months;
            GOLOS_CHECK_LOGIC(prognosis_funds.amount >= 0,
                logic_exception::insufficient_funds_to_approve_worker_result,
                "Insufficient funds to approve worker result");

            auto approvers = count_approvers(worker_techspec_approve_state::approve);

            if (approvers >= STEEMIT_MAJOR_VOTED_WITNESSES) {
                _db.modify(wpo, [&](worker_proposal_object& wpo) {
                    wpo.state = worker_proposal_state::payment;
                });

                _db.modify(wto, [&](worker_techspec_object& wto) {
                    wto.month_consumption = consumption;
                    wto.next_cashout_time = _db.head_block_time() + wto.payments_interval;
                    wto.payment_beginning_time = wto.next_cashout_time;
                });

                _db.modify(gpo, [&](dynamic_global_property_object& gpo) {
                    gpo.worker_consumption_per_month += consumption;
                });
            }
        }
    }

    void worker_assign_evaluator::do_apply(const worker_assign_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1013, "worker_assign_operation");

        _db.get_account(o.worker);

        const auto& wto = _db.get_worker_techspec(o.worker_techspec_author, o.worker_techspec_permlink);

        const auto& wpo = _db.get_worker_proposal(wto.worker_proposal_author, wto.worker_proposal_permlink);

        if (!o.worker.size()) { // Unassign worker
            GOLOS_CHECK_LOGIC(wpo.state == worker_proposal_state::work,
                logic_exception::cannot_unassign_worker_from_finished_or_not_started_work,
                "Cannot unassign worker from finished or not started work");

            GOLOS_CHECK_LOGIC(o.assigner == wto.worker || o.assigner == wto.author,
                logic_exception::worker_can_be_unassigned_only_by_techspec_author_or_himself,
                "Worker can be unassigned only by techspec author or himself");

            _db.modify(wpo, [&](worker_proposal_object& wpo) {
                wpo.state = worker_proposal_state::techspec;
            });

            _db.modify(wto, [&](worker_techspec_object& wto) {
                wto.worker = account_name_type();
                wto.work_beginning_time = time_point_sec::min();
            });

            return;
        }

        GOLOS_CHECK_LOGIC(wpo.approved_techspec_author == wto.author && wpo.approved_techspec_permlink == wto.permlink
                && wpo.state == worker_proposal_state::techspec,
            logic_exception::worker_can_be_assigned_only_to_proposal_with_approved_techspec,
            "Worker can be assigned only to proposal with approved techspec");

        GOLOS_CHECK_LOGIC(wpo.type == worker_proposal_type::task,
            logic_exception::worker_cannot_be_assigned_to_premade_proposal,
            "Worker cannot be assigned to premade proposal");

        _db.modify(wpo, [&](worker_proposal_object& wpo) {
            wpo.state = worker_proposal_state::work;
        });

        _db.modify(wto, [&](worker_techspec_object& wto) {
            wto.worker = o.worker;
            wto.work_beginning_time = _db.head_block_time();
        });
    }

} } // golos::chain
