namespace golos {
namespace chain {
template class account_create_evaluator<0, 16, 0>;
template class account_create_evaluator<0, 17, 0>;

template class account_create_with_delegation_evaluator<0, 17, 0>;

template class account_update_evaluator<0, 16, 0>;
template class account_update_evaluator<0, 17, 0>;

template class account_whitelist_evaluator<0, 17, 0>;
}
}