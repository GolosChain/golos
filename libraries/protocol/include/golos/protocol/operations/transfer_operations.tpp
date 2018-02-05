namespace golos {
    namespace protocol {
        template struct transfer_operation<0, 16, 0>;
        template struct transfer_operation<0, 17, 0>;

        template struct transfer_to_vesting_operation<0, 16, 0>;
        template struct transfer_to_vesting_operation<0, 17, 0>;

        template struct transfer_to_savings_operation<0, 16, 0>;
        template struct transfer_to_savings_operation<0, 17, 0>;

        template struct transfer_from_savings_operation<0, 16, 0>;
        template struct transfer_from_savings_operation<0, 17, 0>;

        template struct cancel_transfer_from_savings_operation<0, 16, 0>;
        template struct cancel_transfer_from_savings_operation<0, 17, 0>;

        template struct override_transfer_operation<0, 16, 0>;
        template struct override_transfer_operation<0, 17, 0>;
    }
}