#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/authority.hpp>
#include <golos/protocol/base.hpp>

namespace golos { namespace protocol {

    enum class worker_proposal_type {
        task,
        premade_work,
        _size
    };

    struct worker_proposal_operation : public base_operation {
        account_name_type author;
        std::string permlink;
        worker_proposal_type type = worker_proposal_type::task;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct worker_proposal_delete_operation : public base_operation {
        account_name_type author;
        std::string permlink;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct worker_techspec_operation : public base_operation {
        account_name_type author;
        std::string permlink;
        account_name_type worker_proposal_author;
        std::string worker_proposal_permlink;
        asset specification_cost;
        uint32_t specification_eta;
        asset development_cost;
        uint32_t development_eta;
        uint16_t payments_count;
        uint32_t payments_interval;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct worker_techspec_delete_operation : public base_operation {
        account_name_type author;
        std::string permlink;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    enum class worker_techspec_approve_state {
        approve,
        disapprove,
        abstain,
        _size
    };

    struct worker_techspec_approve_operation : public base_operation {
        account_name_type approver;
        account_name_type author;
        std::string permlink;
        worker_techspec_approve_state state;

        extensions_type extensions;

        void validate() const;

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            a.insert(approver);
        }
    };

    struct worker_result_fill_operation : public base_operation {
        account_name_type author;
        std::string permlink;
        std::string worker_techspec_permlink;
        time_point_sec completion_date = time_point_sec::min();

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct worker_result_clear_operation : public base_operation {
        account_name_type author;
        std::string permlink;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

} } // golos::protocol

FC_REFLECT_ENUM(golos::protocol::worker_proposal_type, (task)(premade_work)(_size))
FC_REFLECT(
    (golos::protocol::worker_proposal_operation),
    (author)(permlink)(type)(extensions))

FC_REFLECT(
    (golos::protocol::worker_proposal_delete_operation),
    (author)(permlink)(extensions))

FC_REFLECT(
    (golos::protocol::worker_techspec_operation),
    (author)(permlink)(worker_proposal_author)(worker_proposal_permlink)(specification_cost)(specification_eta)
    (development_cost)(development_eta)(payments_count)(payments_interval)(extensions))

FC_REFLECT(
    (golos::protocol::worker_techspec_delete_operation),
    (author)(permlink)(extensions))

FC_REFLECT_ENUM(golos::protocol::worker_techspec_approve_state, (approve)(disapprove)(abstain)(_size))
FC_REFLECT(
    (golos::protocol::worker_techspec_approve_operation),
    (approver)(author)(permlink)(state)(extensions))

FC_REFLECT(
    (golos::protocol::worker_result_fill_operation),
    (author)(permlink)(worker_techspec_permlink)(completion_date)(extensions))

FC_REFLECT(
    (golos::protocol::worker_result_clear_operation),
    (author)(permlink)(extensions))
