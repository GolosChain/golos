#ifndef GOLOS_WITNESS_OPERATIONS_TPP_HPP
#define GOLOS_WITNESS_OPERATIONS_TPP_HPP

namespace golos {
    namespace protocol {
        template struct witness_update_operation<0, 16, 0>;
        template struct witness_update_operation<0, 17, 0>;

        template struct account_witness_vote_operation<0, 16, 0>;
        template struct account_witness_vote_operation<0, 17, 0>;

        template struct account_witness_proxy_operation<0, 16, 0>;
        template struct account_witness_proxy_operation<0, 17, 0>;
    }
}

#endif //GOLOS_WITNESS_OPERATIONS_TPP_HPP
