#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>

namespace golos {
    namespace protocol {
        /**
         *  This operation instructs the blockchain to start a conversion between STEEM and SBD,
         *  The funds are deposited after STEEMIT_CONVERSION_DELAY
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct convert_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type owner;
            uint32_t request_id = 0;
            asset<Major, Hardfork, Release> amount;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };

        /**
         *  @class limit_order_create_operation
         *  @brief instructs the blockchain to attempt to sell one asset for another
         *  @ingroup operations
         *
         *  The blockchain will perform an attempt to sell amount_to_sell.asset_id for as
         *  much min_to_receive.asset_id as possible.  The fee will be paid by
         *  the seller's account.  Market fees will apply as specified by the
         *  issuer of both the selling asset and the receiving asset as
         *  a percentage of the amount exchanged.
         *
         *  If either the selling asset or the receiving asset is white list
         *  restricted, the order will only be created if the seller is on
         *  the white list of the restricted asset type.
         *
         *  Market orders are matched in the order they are included
         *  in the block chain.
         */

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct limit_order_create_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type owner;
            integral_id_type order_id = 0; /// an ID assigned by owner, must be unique
            asset<Major, Hardfork, Release> amount_to_sell;
            asset<Major, Hardfork, Release> min_to_receive;
            bool fill_or_kill = false;
            time_point_sec expiration = time_point_sec::maximum();

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }

            price<Major, Hardfork, Release> get_price() const {
                return amount_to_sell / min_to_receive;
            }

            pair<typename asset<Major, Hardfork, Release>::asset_container_type,
                    typename asset<Major, Hardfork, Release>::asset_container_type> get_market() const {
                return amount_to_sell.symbol < min_to_receive.symbol ? std::make_pair(amount_to_sell.symbol,
                                                                                      min_to_receive.symbol)
                                                                     : std::make_pair(min_to_receive.symbol,
                                                                                      amount_to_sell.symbol);
            }
        };

        /**
         *  This operation is identical to limit_order_create except it serializes the price rather
         *  than calculating it from other fields.
         */

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct limit_order_create2_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type owner;
            integral_id_type order_id = 0; /// an ID assigned by owner, must be unique
            asset<Major, Hardfork, Release> amount_to_sell;
            bool fill_or_kill = false;
            price<Major, Hardfork, Release> exchange_rate;
            time_point_sec expiration = time_point_sec::maximum();

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }

            price<Major, Hardfork, Release> get_price() const {
                return exchange_rate;
            }

            pair<typename asset<Major, Hardfork, Release>::asset_container_type,
                    typename asset<Major, Hardfork, Release>::asset_container_type> get_market() const {
                return exchange_rate.base.symbol < exchange_rate.quote.symbol ? std::make_pair(
                        exchange_rate.base.symbol, exchange_rate.quote.symbol) : std::make_pair(
                        exchange_rate.quote.symbol, exchange_rate.base.symbol);
            }
        };

        /**
         *  @ingroup operations
         *  Used to cancel an existing limit order. Both fee_pay_account and the
         *  account to receive the proceeds must be the same as order->seller.
         *
         *  @return the amount actually refunded
         */

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct limit_order_cancel_operation : public base_operation<Major, Hardfork, Release> {
            account_name_type owner;
            integral_id_type order_id = 0;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };

        /**
         *  @ingroup operations
         *
         *  This operation can be used to add collateral, cover, and adjust the margin call price for a particular user.
         *
         *  For prediction markets the collateral and debt must always be equal.
         *
         *  This operation will fail if it would trigger a margin call that couldn't be filled.  If the margin call hits
         *  the call price limit then it will fail if the call price is above the settlement price.
         *
         *  @note this operation can be used to force a market order using the collateral without requiring outside funds.
         */

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct call_order_update_operation : public base_operation<Major, Hardfork, Release> {
            integral_id_type order_id = 0;
            account_name_type funding_account; ///< pays fee, collateral, and cover
            asset<Major, Hardfork, Release> delta_collateral; ///< the amount of collateral to add to the margin position
            asset<Major, Hardfork, Release> delta_debt; ///< the amount of the debt to be paid off, may be negative to issue new debt

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(funding_account);
            }
        };

        /**
         *  @ingroup operations
         *
         *  This operation can be used after a black swan to bid collateral for
         *  taking over part of the debt and the settlement_fund (see BSIP-0018).
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct bid_collateral_operation : public base_operation<Major, Hardfork, Release> {
            /** should be equivalent to call_order_update fee */
            account_name_type bidder; ///< pays fee and additional collateral
            asset<Major, Hardfork, Release> additional_collateral; ///< the amount of collateral to bid for the debt
            asset<Major, Hardfork, Release> debt_covered; ///< the amount of debt to take over
            extensions_type extensions;

            void validate() const;
        };
    }
}

FC_REFLECT((golos::protocol::convert_operation<0, 16, 0>), (owner)(request_id)(amount));
FC_REFLECT((golos::protocol::convert_operation<0, 17, 0>), (owner)(request_id)(amount));

FC_REFLECT((golos::protocol::limit_order_create_operation<0, 16, 0>),
                   (owner)(order_id)(amount_to_sell)(min_to_receive)(fill_or_kill)(expiration))
FC_REFLECT((golos::protocol::limit_order_create_operation<0, 17, 0>),
           (owner)(order_id)(amount_to_sell)(min_to_receive)(fill_or_kill)(expiration))

FC_REFLECT((golos::protocol::limit_order_create2_operation<0, 16, 0>),
           (owner)(order_id)(amount_to_sell)(exchange_rate)(fill_or_kill)(expiration))
FC_REFLECT((golos::protocol::limit_order_create2_operation<0, 17, 0>),
           (owner)(order_id)(amount_to_sell)(exchange_rate)(fill_or_kill)(expiration))

FC_REFLECT((golos::protocol::limit_order_cancel_operation<0, 16, 0>), (owner)(order_id))
FC_REFLECT((golos::protocol::limit_order_cancel_operation<0, 17, 0>), (owner)(order_id))

FC_REFLECT((golos::protocol::call_order_update_operation<0, 17, 0>), (funding_account)(delta_collateral)(delta_debt))

FC_REFLECT((golos::protocol::bid_collateral_operation<0, 17, 0>), (bidder)(additional_collateral)(debt_covered)(extensions))