namespace golos {
namespace chain {
template class asset_create_evaluator<0, 17, 0>;

template class asset_issue_evaluator<0, 17, 0>;

template class asset_reserve_evaluator<0, 17, 0>;

template class asset_update_evaluator<0, 17, 0>;

template class asset_update_bitasset_evaluator<0, 17, 0>;

template class asset_update_feed_producers_evaluator<0, 17, 0>;

template class asset_fund_fee_pool_evaluator<0, 17, 0>;

template class asset_global_settle_evaluator<0, 17, 0>;

template class asset_settle_evaluator<0, 17, 0>;

template class asset_force_settle_evaluator<0, 17, 0>;

template class asset_publish_feeds_evaluator<0, 17, 0>;

template class asset_claim_fees_evaluator<0, 17, 0>;
}
}