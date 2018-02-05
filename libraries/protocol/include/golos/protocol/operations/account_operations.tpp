namespace golos {
    namespace protocol {
        template struct account_create_operation<0, 16, 0>;
        template struct account_create_operation<0, 17, 0>;

        template struct account_create_with_delegation_operation<0, 17, 0>;

        template struct account_update_operation<0, 16, 0>;
        template struct account_update_operation<0, 17, 0>;

        template struct account_whitelist_operation<0, 17, 0>;
    }
}