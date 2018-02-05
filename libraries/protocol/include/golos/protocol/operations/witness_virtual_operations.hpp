#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

#include <fc/utf8.hpp>

namespace golos {
    namespace protocol {

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct expire_witness_vote_operation : public virtual_operation<Major, Hardfork, Release> {
            expire_witness_vote_operation() {
            }

            expire_witness_vote_operation(const account_name_type &account, const account_name_type &wname,
                                          const fc::time_point_sec &created) : owner(account), witness(wname),
                    creation_time(created) {
            }

            account_name_type owner;
            account_name_type witness;

            fc::time_point_sec creation_time;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct shutdown_witness_operation : public virtual_operation<Major, Hardfork, Release> {
            shutdown_witness_operation() {
            }

            shutdown_witness_operation(const string &o) : owner(o) {
            }

            account_name_type owner;
        };
    }
}

FC_REFLECT((golos::protocol::expire_witness_vote_operation<0, 17, 0>), (owner)(witness)(creation_time))

FC_REFLECT((golos::protocol::shutdown_witness_operation<0, 16, 0>), (owner))
FC_REFLECT((golos::protocol::shutdown_witness_operation<0, 17, 0>), (owner))