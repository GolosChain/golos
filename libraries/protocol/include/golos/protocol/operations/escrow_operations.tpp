namespace golos {
    namespace protocol {
        template struct escrow_transfer_operation<0, 16, 0>;
        template struct escrow_transfer_operation<0, 17, 0>;

        template struct escrow_approve_operation<0, 16, 0>;
        template struct escrow_approve_operation<0, 17, 0>;

        template struct escrow_dispute_operation<0, 16, 0>;
        template struct escrow_dispute_operation<0, 17, 0>;

        template struct escrow_release_operation<0, 16, 0>;
        template struct escrow_release_operation<0, 17, 0>;
    }
}