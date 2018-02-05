namespace golos {
namespace chain {
template class convert_evaluator<0, 16, 0>;
template class convert_evaluator<0, 17, 0>;

template class limit_order_create_evaluator<0, 16, 0>;
template class limit_order_create_evaluator<0, 17, 0>;

template class limit_order_create2_evaluator<0, 16, 0>;
template class limit_order_create2_evaluator<0, 17, 0>;

template class limit_order_cancel_evaluator<0, 16, 0>;
template class limit_order_cancel_evaluator<0, 17, 0>;

template class call_order_update_evaluator<0, 17, 0>;

template class bid_collateral_evaluator<0, 17, 0>;
}
}