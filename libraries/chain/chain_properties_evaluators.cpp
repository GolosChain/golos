#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/protocol/validate_helper.hpp>

namespace golos { namespace chain {

    void witness_update_evaluator::do_apply(const witness_update_operation& o) {
        _db.get_account(o.owner); // verify owner exists

        if (_db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
            GOLOS_CHECK_OP_PARAM(o, url, {
                GOLOS_CHECK_VALUE_MAX_SIZE(o.url, STEEMIT_MAX_WITNESS_URL_LENGTH);
            });
        } else if (o.url.size() > STEEMIT_MAX_WITNESS_URL_LENGTH) {
            // after HF, above check can be moved to validate() if reindex doesn't show this warning
            wlog("URL is too long in block ${b}", ("b", _db.head_block_num() + 1));
        }

        const bool has_hf18 = _db.has_hardfork(STEEMIT_HARDFORK_0_18__673);

        auto update_witness = [&](witness_object& w) {
            from_string(w.url, o.url);
            w.signing_key = o.block_signing_key;
            if (!has_hf18) {
                w.props = o.props;
            }
        };

        const auto& idx = _db.get_index<witness_index>().indices().get<by_name>();
        auto itr = idx.find(o.owner);
        if (itr != idx.end()) {
            _db.modify(*itr, update_witness);
        } else {
            _db.create<witness_object>([&](witness_object& w) {
                w.owner = o.owner;
                w.created = _db.head_block_time();
                update_witness(w);
            });
        }
    }

    struct chain_properties_update {
        using result_type = void;

        const database& _db;
        chain_properties& _wprops;

        chain_properties_update(const database& db, chain_properties& wprops)
                : _db(db), _wprops(wprops) {
        }

        result_type operator()(const chain_properties_19& props) const {
            GOLOS_CHECK_PARAM(props, {
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_22__1008)) {
                    auto max_delegated_vesting_interest_rate = props.max_delegated_vesting_interest_rate;
                    GOLOS_CHECK_VALUE_LE(max_delegated_vesting_interest_rate, STEEMIT_MAX_DELEGATED_VESTING_INTEREST_RATE_PRE_HF22);
                }
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_22__1009)) {
                    auto min_curation_percent = props.min_curation_percent;
                    auto max_curation_percent = props.max_curation_percent;
                    GOLOS_CHECK_VALUE_LEGE(min_curation_percent, STEEMIT_MIN_CURATION_PERCENT_PRE_HF22, max_curation_percent);
                }
            });
            _wprops = props;
        }

        result_type operator()(const chain_properties_22& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22, "chain_properties_22");
            _wprops = props;
        }

        template<typename Props>
        result_type operator()(Props&& props) const {
            _wprops = props;
        }
    };

    void chain_properties_update_evaluator::do_apply(const chain_properties_update_operation& o) {
        _db.get_account(o.owner); // verify owner exists

        const auto& idx = _db.get_index<witness_index>().indices().get<by_name>();
        auto itr = idx.find(o.owner);
        if (itr != idx.end()) {
            _db.modify(*itr, [&](witness_object& w) {
                o.props.visit(chain_properties_update(_db, w.props));
            });
        } else {
            _db.create<witness_object>([&](witness_object& w) {
                w.owner = o.owner;
                w.created = _db.head_block_time();
                o.props.visit(chain_properties_update(_db, w.props));
            });
        }
    }

    void transit_to_cyberway_evaluator::do_apply(const transit_to_cyberway_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1348, "transit_to_cyberway");

        _db.get_account(o.owner); // verify owner exists
        auto& witness = _db.get_witness(o.owner);

        GOLOS_CHECK_OP_PARAM(o, vote_to_transit, {
            GOLOS_CHECK_VALUE(o.vote_to_transit != (witness.transit_to_cyberway_vote != STEEMIT_GENESIS_TIME),
                "You should change your vote.");
        });

        _db.modify(witness, [&](witness_object& w) {
            if (o.vote_to_transit) {
                w.transit_to_cyberway_vote = _db.head_block_time();
            } else {
                w.transit_to_cyberway_vote = STEEMIT_GENESIS_TIME;
            }
        });
    }

} } // golos::chain
