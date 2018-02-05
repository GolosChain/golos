namespace golos {
    namespace chain {
    template class escrow_transfer_evaluator<0, 16, 0>;
    template class escrow_transfer_evaluator<0, 17, 0>;

    template class escrow_approve_evaluator<0, 16, 0>;
    template class escrow_approve_evaluator<0, 17, 0>;

    template class escrow_dispute_evaluator<0, 16, 0>;
    template class escrow_dispute_evaluator<0, 17, 0>;

    template class escrow_release_evaluator<0, 16, 0>;
    template class escrow_release_evaluator<0, 17, 0>;
    }
}