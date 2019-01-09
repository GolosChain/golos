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

} } // golos::chain
