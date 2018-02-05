namespace golos {
    namespace protocol {
        template struct comment_operation<0, 16, 0>;
        template struct comment_operation<0, 17, 0>;

        template struct comment_options_operation<0, 16, 0>;
        template struct comment_options_operation<0, 17, 0>;

        template struct comment_payout_extension_operation<0, 17, 0>;

        template struct delete_comment_operation<0, 16, 0>;
        template struct delete_comment_operation<0, 17, 0>;
    }
}