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
        worker_proposal_metadata_object_type = (WORKER_API_SPACE_ID << 8),
        worker_techspec_metadata_object_type = (WORKER_API_SPACE_ID << 8) + 1
    };

    class worker_proposal_metadata_object : public object<worker_proposal_metadata_object_type, worker_proposal_metadata_object> {
    public:
        worker_proposal_metadata_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_proposal_metadata_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        comment_id_type post;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
    };

    class worker_techspec_metadata_object : public object<worker_techspec_metadata_object_type, worker_techspec_metadata_object> {
    public:
        worker_techspec_metadata_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_techspec_metadata_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        comment_id_type post;
        time_point_sec modified;
        share_type net_rshares;
        uint16_t approves = 0;
        uint16_t disapproves = 0;
        time_point_sec work_beginning_time;
        uint16_t worker_payment_approves = 0;
        uint16_t worker_payment_disapproves = 0;
        time_point_sec payment_beginning_time;
        asset consumption_per_day;
    };

    struct by_net_rshares;

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

    struct by_approves;
    struct by_disapproves;
    struct by_worker_payment_approves;
    struct by_worker_payment_disapproves;

    using worker_techspec_metadata_id_type = object_id<worker_techspec_metadata_object>;

    using worker_techspec_metadata_index = multi_index_container<
        worker_techspec_metadata_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_techspec_metadata_object, worker_techspec_metadata_id_type, &worker_techspec_metadata_object::id>>,
            ordered_unique<
                tag<by_post>,
                member<worker_techspec_metadata_object, comment_id_type, &worker_techspec_metadata_object::post>>,
            ordered_unique<
                tag<by_net_rshares>,
                composite_key<
                    worker_techspec_metadata_object,
                    member<worker_techspec_metadata_object, share_type, &worker_techspec_metadata_object::net_rshares>,
                    member<worker_techspec_metadata_object, worker_techspec_metadata_id_type, &worker_techspec_metadata_object::id>>,
                composite_key_compare<
                    std::greater<share_type>,
                    std::less<worker_techspec_metadata_id_type>>>,
            ordered_unique<
                tag<by_approves>,
                composite_key<
                    worker_techspec_metadata_object,
                    member<worker_techspec_metadata_object, uint16_t, &worker_techspec_metadata_object::approves>,
                    member<worker_techspec_metadata_object, worker_techspec_metadata_id_type, &worker_techspec_metadata_object::id>>,
                composite_key_compare<
                    std::greater<uint16_t>,
                    std::less<worker_techspec_metadata_id_type>>>,
            ordered_unique<
                tag<by_disapproves>,
                composite_key<
                    worker_techspec_metadata_object,
                    member<worker_techspec_metadata_object, uint16_t, &worker_techspec_metadata_object::disapproves>,
                    member<worker_techspec_metadata_object, worker_techspec_metadata_id_type, &worker_techspec_metadata_object::id>>,
                composite_key_compare<
                    std::greater<uint16_t>,
                    std::less<worker_techspec_metadata_id_type>>>,
            ordered_unique<
                tag<by_worker_payment_approves>,
                composite_key<
                    worker_techspec_metadata_object,
                    member<worker_techspec_metadata_object, uint16_t, &worker_techspec_metadata_object::worker_payment_approves>,
                    member<worker_techspec_metadata_object, worker_techspec_metadata_id_type, &worker_techspec_metadata_object::id>>,
                composite_key_compare<
                    std::greater<uint16_t>,
                    std::less<worker_techspec_metadata_id_type>>>,
            ordered_unique<
                tag<by_worker_payment_disapproves>,
                composite_key<
                    worker_techspec_metadata_object,
                    member<worker_techspec_metadata_object, uint16_t, &worker_techspec_metadata_object::worker_payment_disapproves>,
                    member<worker_techspec_metadata_object, worker_techspec_metadata_id_type, &worker_techspec_metadata_object::id>>,
                composite_key_compare<
                    std::greater<uint16_t>,
                    std::less<worker_techspec_metadata_id_type>>>>,
        allocator<worker_techspec_metadata_object>>;

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
        }

        comment_api_object post;
        worker_proposal_type type;
        worker_proposal_state state;
        comment_api_object approved_techspec_post;
        time_point_sec created;
        time_point_sec modified;
        share_type net_rshares;
    };

    struct worker_techspec_api_object {
        worker_techspec_api_object(const worker_techspec_metadata_object& o, const comment_api_object& p)
            : post(p),
              modified(o.modified),
              net_rshares(o.net_rshares),
              approves(o.approves),
              disapproves(o.disapproves),
              work_beginning_time(o.work_beginning_time),
              worker_payment_approves(o.worker_payment_approves),
              worker_payment_disapproves(o.worker_payment_disapproves),
              consumption_per_day(o.consumption_per_day),
              payment_beginning_time(o.payment_beginning_time) {
        }

        worker_techspec_api_object() {
        }

        void fill_worker_techspec(const worker_techspec_object& wto) {
            state = wto.state;
            specification_cost = wto.specification_cost;
            development_cost = wto.development_cost;
            worker = wto.worker;
            payments_count = wto.payments_count;
            payments_interval = wto.payments_interval;
            next_cashout_time = wto.next_cashout_time;
            finished_payments_count = wto.finished_payments_count;
        }

        comment_api_object post;
        comment_api_object worker_proposal_post;
        worker_techspec_state state;
        time_point_sec modified;
        share_type net_rshares;
        asset specification_cost;
        asset development_cost;
        uint16_t approves = 0;
        uint16_t disapproves = 0;
        account_name_type worker;
        time_point_sec work_beginning_time;
        comment_api_object worker_result_post;
        uint16_t worker_payment_approves = 0;
        uint16_t worker_payment_disapproves = 0;
        uint16_t payments_count = 0;
        uint32_t payments_interval = 0;
        asset consumption_per_day;
        time_point_sec payment_beginning_time;
        time_point_sec next_cashout_time = time_point_sec::maximum();
        uint16_t finished_payments_count = 0;
    };

} } } // golos::plugins::worker_api

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::worker_api::worker_proposal_metadata_object,
    golos::plugins::worker_api::worker_proposal_metadata_index)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::worker_api::worker_techspec_metadata_object,
    golos::plugins::worker_api::worker_techspec_metadata_index)

FC_REFLECT((golos::plugins::worker_api::worker_proposal_api_object),
    (post)(type)(state)(approved_techspec_post)(created)(modified)(net_rshares)
)

FC_REFLECT((golos::plugins::worker_api::worker_techspec_api_object),
    (post)(worker_proposal_post)(state)(modified)(net_rshares)(specification_cost)(development_cost)(approves)(disapproves)
    (worker)(work_beginning_time)(worker_result_post)(worker_payment_approves)(worker_payment_disapproves)(payments_count)
    (payments_interval)(consumption_per_day)(payment_beginning_time)(next_cashout_time)(finished_payments_count)
)
