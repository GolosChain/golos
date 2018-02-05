namespace golos {
    namespace protocol {
        template struct challenge_authority_operation<0, 16, 0>;
        template struct challenge_authority_operation<0, 17, 0>;

        template struct prove_authority_operation<0, 16, 0>;
        template struct prove_authority_operation<0, 17, 0>;

        template struct vote_operation<0, 16, 0>;
        template struct vote_operation<0, 17, 0>;

        template struct withdraw_vesting_operation<0, 16, 0>;
        template struct withdraw_vesting_operation<0, 17, 0>;

        template struct set_withdraw_vesting_route_operation<0, 16, 0>;
        template struct set_withdraw_vesting_route_operation<0, 17, 0>;

        template struct feed_publish_operation<0, 16, 0>;
        template struct feed_publish_operation<0, 17, 0>;

        template struct pow_operation<0, 16, 0>;
        template struct pow_operation<0, 17, 0>;

        template struct pow2_operation<0, 16, 0>;
        template struct pow2_operation<0, 17, 0>;

        template struct report_over_production_operation<0, 16, 0>;
        template struct report_over_production_operation<0, 17, 0>;

        template struct request_account_recovery_operation<0, 16, 0>;
        template struct request_account_recovery_operation<0, 17, 0>;

        template struct recover_account_operation<0, 16, 0>;
        template struct recover_account_operation<0, 17, 0>;

        template struct reset_account_operation<0, 16, 0>;
        template struct reset_account_operation<0, 17, 0>;

        template struct set_reset_account_operation<0, 16, 0>;
        template struct set_reset_account_operation<0, 17, 0>;

        template struct change_recovery_account_operation<0, 16, 0>;
        template struct change_recovery_account_operation<0, 17, 0>;

        template struct decline_voting_rights_operation<0, 16, 0>;
        template struct decline_voting_rights_operation<0, 17, 0>;

        template struct delegate_vesting_shares_operation<0, 16, 0>;
        template struct delegate_vesting_shares_operation<0, 17, 0>;
    }
}