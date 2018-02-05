namespace golos {
    namespace protocol {
        template struct convert_operation<0, 16, 0>;
        template struct convert_operation<0, 17, 0>;

        template struct limit_order_create_operation<0, 16, 0>;
        template struct limit_order_create_operation<0, 17, 0>;

        template struct limit_order_create2_operation<0, 16, 0>;
        template struct limit_order_create2_operation<0, 17, 0>;

        template struct limit_order_cancel_operation<0, 16, 0>;
        template struct limit_order_cancel_operation<0, 17, 0>;

        template struct call_order_update_operation<0, 17, 0>;

        template struct bid_collateral_operation<0, 17, 0>;

    }
}