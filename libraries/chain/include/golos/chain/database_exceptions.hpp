#pragma once

#include <golos/protocol/exceptions.hpp>

namespace golos {
    namespace chain {
        namespace exceptions {
            namespace chain {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
                template<uint32_t Code = 4000000,
                        typename What = boost::mpl::string<'blockchain exception'>> using basic = fc::basic_exception<
                        Code, What>;

                template<uint32_t Code = 4010000,
                        typename What = boost::mpl::string<'database query exception'>> using database_query = basic<
                        Code, What>;

                template<uint32_t Code = 4020000,
                        typename What = boost::mpl::string<'block validation exception'>> using block_validate = basic<
                        Code, What>;

                template<uint32_t Code = 4030000, typename What = boost::mpl::string<
                        'transaction validation exception'>> using transaction = basic<Code, What>;

                template<uint32_t Code = 4040000, typename What = boost::mpl::string<
                        'operation validation exception'>> using operation_validate = basic<Code, What>;

                template<uint32_t Code = 4050000, typename What = boost::mpl::string<
                        'operation evaluation exception'>> using operation_evaluate = basic<Code, What>;

                namespace utility {
                    template<uint32_t Code = 4060000,
                            typename What = boost::mpl::string<'utility method exception'>> using basic = chain::basic<
                            Code, What>;

                    template<uint32_t Code = 3060001, typename What = boost::mpl::string<
                            'invalid pts address'>> using invalid_pts_address = basic<Code, What>;
                }

                namespace undo_database {
                    template<uint32_t Code = 4070000,
                            typename What = boost::mpl::string<'undo database exception'>> using basic = chain::basic<
                            Code, What>;

                    template<uint32_t Code = 4070001, typename What = boost::mpl::string<
                            'there are no blocks to pop'>> using pop_empty_chain = basic<Code, What>;
                }

                template<uint32_t Code = 4080000, typename What = boost::mpl::string<
                        'unlinkable block exception'>> using unlinkable_block = basic<Code, What>;

                template<uint32_t Code = 4090000, typename What = boost::mpl::string<
                        'chain attempted to apply unknown hardfork'>> using unknown_hardfork = basic<Code, What>;

                template<uint32_t Code = 4100000,
                        typename What = boost::mpl::string<'plugin exception'>> using plugin = basic<Code, What>;

                template<uint32_t Code = 4110000,
                        typename What = boost::mpl::string<'block log exception'>> using block_log = basic<Code, What>;

                template<uint32_t Code = 37006,
                        typename What = boost::mpl::string<'insuffucient feeds'>> using insufficient_feeds = basic<Code,
                        What>;

                template<uint32_t Code = 3090000,
                        typename What = boost::mpl::string<'black swan'>> using black_swan = basic<Code, What>;

                namespace internal {
                    template<uint32_t IncrementalCode = 0,
                            typename What = boost::mpl::string<'internal exception'>> using basic = chain::basic<
                            4990000 + IncrementalCode, What>;
                }
            }

            namespace operations {
                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'transfer_operation validation exception'>> using transfer_validate_exception = chain::operation_validate<
                        4040000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::transfer_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'transfer_operation evaluation exception'>> using transfer_evaluate_exception = chain::operation_evaluate<
                        4050000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::transfer_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                namespace transfer {
                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 1,
                            typename What = boost::mpl::string<
                                    'owner mismatch'>> using from_account_not_whitelisted = operations::transfer_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 2,
                            typename What = boost::mpl::string<
                                    'owner mismatch'>> using to_account_not_whitelisted = operations::transfer_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 3,
                            typename What = boost::mpl::string<
                                    'restricted transfer asset'>> using restricted_transfer_asset = operations::transfer_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'account_create_operation validation exception'>> using account_create_validate_exception = chain::operation_validate<
                        4040000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::account_create_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'account_create_operation evaluation exception'>> using account_create_evaluate_exception = chain::operation_evaluate<
                        4050000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::account_create_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                namespace account_create {
                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 1,
                            typename What = boost::mpl::string<
                                    'Exceeds max authority fan-out'>> using to_max_auth_exceeded = operations::account_create_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 2,
                            typename What = boost::mpl::string<
                                    'Auth account not found'>> using auth_account_not_found = operations::account_create_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'account_update_operation validation exception'>> using account_update_validate_exception = chain::operation_validate<
                        4040000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::account_update_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'account_update_operation evaluation exception'>> using account_update_evaluate_exception = chain::operation_evaluate<
                        4050000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::account_update_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                namespace account_update {
                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 1,
                            typename What = boost::mpl::string<
                                    'Exceeds max authority fan-out'>> using max_auth_exceeded = operations::account_update_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 2,
                            typename What = boost::mpl::string<
                                    'Auth account not found'>> using auth_account_not_found = operations::account_update_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;
                }
            }

            namespace internal {
                template<uint32_t IncrementalCode = 1, typename What = boost::mpl::string<
                        'Exceeds max authority fan-out'>> using verify_auth_max_auth_exceeded = chain::internal::basic<
                        IncrementalCode, What>;

                template<uint32_t IncrementalCode = 2, typename What = boost::mpl::string<
                        'Auth account not found'>> using verify_auth_account_not_found = chain::internal::basic<
                        IncrementalCode, What>;
            }

            namespace operations {
                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'proposal_create_operation validation exception'>> using proposal_create_validate_exception = chain::operation_validate<
                        4040000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::proposal_create_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'proposal_create_operation evaluation exception'>> using proposal_create_evaluate_exception = chain::operation_evaluate<
                        4050000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::proposal_create_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                namespace proposal_create {
                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 1,
                            typename What = boost::mpl::string<
                                    'review_period required'>> using review_period_required = operations::proposal_create_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;

                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 2,
                            typename What = boost::mpl::string<
                                    'review_period insufficient'>> using review_period_insufficient = operations::proposal_create_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'asset_reserve_operation validation exception'>> using asset_reserve_validate_exception = chain::operation_validate<
                        4040000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::asset_reserve_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'asset_reserve_operation evaluation exception'>> using asset_reserve_evaluate_exception = chain::operation_evaluate<
                        4050000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::asset_reserve_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                namespace asset_reserve {
                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 1,
                            typename What = boost::mpl::string<
                                    'invalid on mia'>> using invalid_on_mia = operations::asset_reserve_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'call_order_update_operation validation exception'>> using call_order_update_validate_exception = chain::operation_validate<
                        4040000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::call_order_update_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'call_order_update_operation evaluation exception'>> using call_order_update_evaluate_exception = chain::operation_evaluate<
                        4050000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::call_order_update_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                namespace call_order_update {
                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 1,
                            typename What = boost::mpl::string<
                                    'Updating call order would trigger a margin call that cannot be fully filled'>> using unfilled_margin_call = operations::call_order_update_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;
                }

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'override_transfer_operation validation exception'>> using override_transfer_validate_exception = chain::operation_validate<
                        4040000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::override_transfer_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 0,
                        typename What = boost::mpl::string<
                                'override_transfer_operation evaluation exception'>> using override_transfer_evaluate_exception = chain::operation_evaluate<
                        4050000 + 100 * golos::protocol::operation::tag<
                                golos::protocol::override_transfer_operation<Major, Hardfork, Release>>::value +
                        IncrementalCode, What>;

                namespace override_transfer {
                    template<uint8_t Major, uint8_t Hardfork, uint16_t Release, uint32_t IncrementalCode = 1,
                            typename What = boost::mpl::string<
                                    'not permitted'>> using not_permitted = operations::override_transfer_evaluate_exception<
                            Major, Hardfork, Release, IncrementalCode, What>;
                }
#pragma GCC diagnostic pop
            }
        }
    }
} // golos::chain

#define STEEMIT_TRY_NOTIFY(signal, ...)                                     \
                try                                                                        \
{                                                                          \
    signal( __VA_ARGS__ );                                                  \
}                                                                          \
            catch( const golos::chain::exceptions::chain::plugin<> &e )                         \
{                                                                          \
    elog( "Caught plugin exception: ${e}", ("e", e.to_detail_string() ) );  \
    throw;                                                                  \
}                                                                          \
            catch( const fc::exception& e )                                            \
{                                                                          \
    elog( "Caught exception in plugin: ${e}", ("e", e.to_detail_string() ) ); \
}                                                                          \
            catch( ... )                                                               \
{                                                                          \
    wlog( "Caught unexpected exception in plugin" );                        \
}
