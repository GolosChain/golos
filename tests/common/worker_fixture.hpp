#pragma once

#include <golos/chain/worker_objects.hpp>

#include "database_fixture.hpp"

namespace golos { namespace chain {

struct worker_fixture : public clean_database_fixture {

    void initialize(const plugin_options& opts = {}) {
        database_fixture::initialize(opts);
        open_database();
        startup();
    }

    private_key_type create_approvers(uint16_t first, uint16_t count) {
        auto private_key = generate_private_key("test");
        auto post_key = generate_private_key("test_post");
        for (auto i = first; i < count; ++i) {
            const auto name = "approver" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, private_key.get_public_key(), post_key.get_public_key()));
            GOLOS_CHECK_NO_THROW(witness_create(name, private_key, "foo.bar", private_key.get_public_key(), 1000));
        }
        return private_key;
    }

    const worker_proposal_object& worker_proposal(
            const string& author, const private_key_type& author_key, const string& permlink, worker_proposal_type type) {
        signed_transaction tx;

        worker_proposal_operation op;
        op.author = author;
        op.permlink = permlink;
        op.type = type;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, author_key, op));

        return db->get_worker_proposal(db->get_comment(author, permlink).id);
    }
};

} } // golos:chain
