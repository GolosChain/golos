namespace golos {
    namespace chain {
        template class witness_update_evaluator<0, 16, 0>;
        template class witness_update_evaluator<0, 17, 0>;

        template class account_witness_vote_evaluator<0, 16, 0>;
        template class account_witness_vote_evaluator<0, 17, 0>;

        template class account_witness_proxy_evaluator<0, 16, 0>;
        template class account_witness_proxy_evaluator<0, 17, 0>;
    }
}