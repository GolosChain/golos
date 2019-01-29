#pragma once

#include <golos/chain/worker_objects.hpp>

namespace golos { namespace plugins { namespace worker_api {

    using namespace golos::chain;

    struct worker_proposal_api_object {
        worker_proposal_api_object(const worker_proposal_object& o)
            : author(o.author),
              permlink(to_string(o.permlink)),
              type(o.type),
              state(o.state),
              approved_techspec_author(o.approved_techspec_author),
              approved_techspec_permlink(to_string(o.approved_techspec_permlink)),
              created(o.created),
              modified(o.modified),
              net_rshares(o.net_rshares) {
        }

        worker_proposal_api_object() {
        }

        account_name_type author;
        std::string permlink;
        worker_proposal_type type;
        worker_proposal_state state;
        account_name_type approved_techspec_author;
        std::string approved_techspec_permlink;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
    };

    struct worker_techspec_api_object {
        worker_techspec_api_object(const worker_techspec_object& o)
            : author(o.author),
              permlink(to_string(o.permlink)),
              worker_proposal_author(o.worker_proposal_author),
              worker_proposal_permlink(to_string(o.worker_proposal_permlink)),
              created(o.created),
              modified(o.modified),
              net_rshares(o.net_rshares),
              specification_cost(o.specification_cost),
              specification_eta(o.specification_eta),
              development_cost(o.development_cost),
              development_eta(o.development_eta),
              approves(o.approves),
              disapproves(o.disapproves),
              worker(o.worker),
              work_beginning_time(o.work_beginning_time),
              worker_result_permlink(to_string(o.worker_result_permlink)),
              completion_date(o.completion_date),
              payments_count(o.payments_count),
              payments_interval(o.payments_interval),
              payment_beginning_time(o.payment_beginning_time),
              next_cashout_time(o.next_cashout_time),
              finished_payments_count(o.finished_payments_count) {
        }

        worker_techspec_api_object() {
        }

        account_name_type author;
        std::string permlink;
        account_name_type worker_proposal_author;
        std::string worker_proposal_permlink;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
        asset specification_cost;
        uint32_t specification_eta = 0;
        asset development_cost;
        uint32_t development_eta = 0;
        uint16_t approves = 0;
        uint16_t disapproves = 0;
        account_name_type worker;
        time_point_sec work_beginning_time;
        std::string worker_result_permlink;
        time_point_sec completion_date;
        uint16_t payments_count = 0;
        uint32_t payments_interval = 0;
        time_point_sec payment_beginning_time;
        time_point_sec next_cashout_time = time_point_sec::maximum();
        uint8_t finished_payments_count = 0;
    };

    struct worker_intermediate_api_object {
        worker_intermediate_api_object(const worker_intermediate_object& o)
            : author(o.author),
              permlink(to_string(o.permlink)),
              worker_techspec_permlink(to_string(o.worker_techspec_permlink)),
              created(o.created) {
        }

        worker_intermediate_api_object() {
        }

        account_name_type author;
        std::string permlink;
        std::string worker_techspec_permlink;
        time_point_sec created;
    };

} } } // golos::plugins::worker_api

FC_REFLECT((golos::plugins::worker_api::worker_proposal_api_object),
    (author)(permlink)(type)(state)(approved_techspec_author)(approved_techspec_permlink)(created)(modified)(net_rshares)
)

FC_REFLECT((golos::plugins::worker_api::worker_techspec_api_object),
    (author)(permlink)(worker_proposal_author)(worker_proposal_permlink)(created)(modified)(net_rshares)(specification_cost)
    (specification_eta)(development_cost)(development_eta)(approves)(disapproves)(worker)(work_beginning_time)
    (worker_result_permlink)(completion_date)(payments_count)(payments_interval)(payment_beginning_time)(next_cashout_time)
    (finished_payments_count)
)

FC_REFLECT((golos::plugins::worker_api::worker_intermediate_api_object),
    (author)(permlink)(worker_techspec_permlink)(created)
)
