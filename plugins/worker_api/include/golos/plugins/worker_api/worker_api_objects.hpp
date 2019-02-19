#pragma once

#include <golos/chain/worker_objects.hpp>
#include <golos/api/comment_api_object.hpp>

namespace golos { namespace plugins { namespace worker_api {

    using namespace golos::chain;
    using golos::api::comment_api_object;

#ifndef WORKER_API_SPACE_ID
#define WORKER_API_SPACE_ID 15
#endif

    enum worker_api_plugin_object_type {
        worker_proposal_metadata_object_type = (WORKER_API_SPACE_ID << 8)
    };

    class worker_proposal_metadata_object : public object<worker_proposal_metadata_object_type, worker_proposal_metadata_object> {
    public:
        worker_proposal_metadata_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_proposal_metadata_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        account_name_type author;
        comment_id_type post;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
    };

    using worker_proposal_metadata_id_type = object_id<worker_proposal_metadata_object>;

    using worker_proposal_metadata_index = multi_index_container<
        worker_proposal_metadata_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_proposal_metadata_object, worker_proposal_metadata_id_type, &worker_proposal_metadata_object::id>>,
            ordered_unique<
                tag<by_post>,
                member<worker_proposal_metadata_object, comment_id_type, &worker_proposal_metadata_object::post>>,
            ordered_unique<
                tag<by_net_rshares>,
                composite_key<
                    worker_proposal_metadata_object,
                    member<worker_proposal_metadata_object, share_type, &worker_proposal_metadata_object::net_rshares>,
                    member<worker_proposal_metadata_object, worker_proposal_metadata_id_type, &worker_proposal_metadata_object::id>>,
                composite_key_compare<
                    std::greater<share_type>,
                    std::less<worker_proposal_metadata_id_type>>>>,
        allocator<worker_proposal_metadata_object>>;

    struct worker_proposal_api_object {
        worker_proposal_api_object(const worker_proposal_metadata_object& o, const comment_api_object& p)
            : post(p),
              created(o.created),
              modified(o.modified),
              net_rshares(o.net_rshares) {
        }

        worker_proposal_api_object() {
        }

        void fill_worker_proposal(const worker_proposal_object& wpo) {
            type = wpo.type;
            state = wpo.state;
            approved_techspec_author = wpo.approved_techspec_author;
            approved_techspec_permlink = to_string(wpo.approved_techspec_permlink);
        }

        comment_api_object post;
        worker_proposal_type type;
        worker_proposal_state state;
        account_name_type approved_techspec_author;
        std::string approved_techspec_permlink;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
    };

    struct worker_techspec_api_object {
        worker_techspec_api_object(const worker_techspec_object& o, const comment_api_object& p)
            : post(p),
              worker_proposal_author(o.worker_proposal_author),
              worker_proposal_permlink(to_string(o.worker_proposal_permlink)),
              state(o.state),
              created(o.created),
              modified(o.modified),
              net_rshares(o.net_rshares),
              specification_cost(o.specification_cost),
              development_cost(o.development_cost),
              approves(o.approves),
              disapproves(o.disapproves),
              worker(o.worker),
              work_beginning_time(o.work_beginning_time),
              worker_result_permlink(to_string(o.worker_result_permlink)),
              completion_date(o.completion_date),
              payments_count(o.payments_count),
              payments_interval(o.payments_interval),
              month_consumption(o.month_consumption),
              payment_beginning_time(o.payment_beginning_time),
              next_cashout_time(o.next_cashout_time),
              finished_payments_count(o.finished_payments_count) {
        }

        worker_techspec_api_object() {
        }

        comment_api_object post;
        account_name_type worker_proposal_author;
        std::string worker_proposal_permlink;
        worker_techspec_state state;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
        asset specification_cost;
        asset development_cost;
        uint16_t approves = 0;
        uint16_t disapproves = 0;
        account_name_type worker;
        time_point_sec work_beginning_time;
        std::string worker_result_permlink;
        time_point_sec completion_date;
        uint16_t payments_count = 0;
        uint32_t payments_interval = 0;
        asset month_consumption;
        time_point_sec payment_beginning_time;
        time_point_sec next_cashout_time = time_point_sec::maximum();
        uint16_t finished_payments_count = 0;
    };

} } } // golos::plugins::worker_api

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::worker_api::worker_proposal_metadata_object,
    golos::plugins::worker_api::worker_proposal_metadata_index)

FC_REFLECT((golos::plugins::worker_api::worker_proposal_api_object),
    (post)(type)(state)(approved_techspec_author)(approved_techspec_permlink)(created)(modified)(net_rshares)
)

FC_REFLECT((golos::plugins::worker_api::worker_techspec_api_object),
    (post)(worker_proposal_author)(worker_proposal_permlink)(state)(created)(modified)(net_rshares)(specification_cost)
    (development_cost)(approves)(disapproves)(worker)(work_beginning_time)(worker_result_permlink)(completion_date)(payments_count)
    (payments_interval)(month_consumption)(payment_beginning_time)(next_cashout_time)(finished_payments_count)
)
