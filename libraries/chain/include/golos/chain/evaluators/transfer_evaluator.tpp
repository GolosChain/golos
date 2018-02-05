namespace golos {
    namespace chain {
    template class transfer_evaluator<0, 16, 0>;
    template class transfer_evaluator<0, 17, 0>;

    template class transfer_to_vesting_evaluator<0, 16, 0>;
    template class transfer_to_vesting_evaluator<0, 17, 0>;

    template class transfer_to_savings_evaluator<0, 16, 0>;
    template class transfer_to_savings_evaluator<0, 17, 0>;

    template class transfer_from_savings_evaluator<0, 16, 0>;
    template class transfer_from_savings_evaluator<0, 17, 0>;

    template class cancel_transfer_from_savings_evaluator<0, 16, 0>;
    template class cancel_transfer_from_savings_evaluator<0, 17, 0>;

    template class override_transfer_evaluator<0, 17, 0>;
    }
}