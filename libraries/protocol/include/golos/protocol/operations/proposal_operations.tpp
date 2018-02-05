namespace golos {
    namespace protocol {
        template struct proposal_create_operation<0, 16, 0>;
        template struct proposal_create_operation<0, 17, 0>;

        template struct proposal_update_operation<0, 16, 0>;
        template struct proposal_update_operation<0, 17, 0>;

        template struct proposal_delete_operation<0, 16, 0>;
        template struct proposal_delete_operation<0, 17, 0>;
    }
}