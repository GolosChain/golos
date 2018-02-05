#pragma once

#include <golos/version/version.hpp>

#include <golos/protocol/types.hpp>
#include <golos/protocol/config.hpp>

namespace golos {
    namespace protocol {

        typedef uint64_t asset_symbol_type;

        extern const int64_t scaled_precision_lut[];

        bool operator==(const asset_symbol_type &a, const asset_name_type &b);

        bool operator==(const asset_name_type &b, const asset_symbol_type &a);

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release, typename StorageType, typename AmountType>
        struct asset_interface : public static_version<Major, Hardfork, Release> {
            typedef StorageType asset_container_type;
            typedef AmountType amount_container_type;

            asset_interface() {

            }

            asset_interface(amount_container_type a, asset_container_type s) : amount(a), symbol(s) {

            }

            virtual ~asset_interface() {

            }

            virtual uint8_t get_decimals() const = 0;

            virtual void set_decimals(uint8_t d) = 0;

            amount_container_type amount;
            asset_container_type symbol;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release, typename = type_traits::static_range<true>>
        struct asset : public asset_interface<Major, Hardfork, Release, type_traits::void_t, type_traits::void_t> {

        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct asset<Major, Hardfork, Release, type_traits::static_range<Hardfork <= 16>> : public asset_interface<
                Major, Hardfork, Release, asset_symbol_type, share_type> {
            asset();

            asset(share_type a, asset_symbol_type id = STEEM_SYMBOL);

            asset(share_type a, asset_name_type name);

            virtual ~asset() override {

            }

            double to_real() const;

            virtual uint8_t get_decimals() const override;

            virtual void set_decimals(uint8_t d) override;

            asset_name_type symbol_name() const;

            asset_symbol_type symbol_type_value() const;

            int64_t precision() const;

            static asset<Major, Hardfork, Release> from_string(const string &from);

            string to_string() const;

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            asset<Major, Hardfork, Release> &operator+=(
                    const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &o) {
                FC_ASSERT(this->symbol == o.symbol);
                this->amount += o.amount;
                return *this;
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            asset<Major, Hardfork, Release> &operator-=(
                    const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &o) {
                FC_ASSERT(this->symbol == o.symbol);
                this->amount -= o.amount;
                return *this;
            }

            asset<Major, Hardfork, Release> operator-() const {
                return asset<Major, Hardfork, Release>(-this->amount, this->symbol);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator==(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return std::tie(a.symbol, a.amount) == std::tie(b.symbol, b.amount);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator<(const asset<Major, Hardfork, Release> &a,
                                  const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                FC_ASSERT(a.symbol == b.symbol);
                return a.amount < b.amount;
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator<=(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return (a == b) || (a < b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator!=(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return !(a == b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator>(const asset<Major, Hardfork, Release> &a,
                                  const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return !(a <= b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator>=(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return !(a < b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend asset<Major, Hardfork, Release> operator-(const asset<Major, Hardfork, Release> &a,
                                                             const asset<ArgumentMajor, ArgumentHardfork,
                                                                     ArgumentRelease> &b) {
                FC_ASSERT(a.symbol == b.symbol && a.get_decimals() == b.get_decimals());
                asset<Major, Hardfork, Release> amount(a.amount - b.amount, a.symbol);
                amount.set_decimals(a.get_decimals());
                return amount;
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend asset<Major, Hardfork, Release> operator+(const asset<Major, Hardfork, Release> &a,
                                                             const asset<ArgumentMajor, ArgumentHardfork,
                                                                     ArgumentRelease> &b) {
                FC_ASSERT(a.symbol == b.symbol && a.get_decimals() == b.get_decimals());
                asset<Major, Hardfork, Release> amount(a.amount + b.amount, a.symbol);
                amount.set_decimals(a.get_decimals());
                return amount;
            }

            static share_type scaled_precision(uint8_t precision) {
                FC_ASSERT(precision < 19);
                return scaled_precision_lut[precision];
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct asset<Major, Hardfork, Release, type_traits::static_range<Hardfork >= 17>> : public asset_interface<
                Major, Hardfork, Release, asset_name_type, share_type> {
            asset();

            asset(share_type a, asset_symbol_type name);

            asset(share_type a, asset_name_type name = STEEM_SYMBOL_NAME, uint8_t d = 3);

            virtual ~asset() override {

            }

            virtual uint8_t get_decimals() const override;

            virtual void set_decimals(uint8_t d) override;

            uint8_t decimals;

            double to_real() const;

            asset_symbol_type symbol_type_value() const;

            asset_name_type symbol_name() const;

            int64_t precision() const;

            static asset<Major, Hardfork, Release> from_string(const string &from);

            string to_string() const;

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            asset<Major, Hardfork, Release> &operator+=(
                    const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &o) {
                FC_ASSERT(this->symbol == o.symbol);
                this->amount += o.amount;
                return *this;
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            asset<Major, Hardfork, Release> &operator-=(
                    const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &o) {
                FC_ASSERT(this->symbol == o.symbol);
                this->amount -= o.amount;
                return *this;
            }

            asset<Major, Hardfork, Release> operator-() const {
                return asset<Major, Hardfork, Release>(-this->amount, this->symbol);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator==(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return std::tie(a.symbol, a.amount) == std::tie(b.symbol, b.amount);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator<(const asset<Major, Hardfork, Release> &a,
                                  const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                FC_ASSERT(a.symbol == b.symbol);
                return a.amount < b.amount;
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator<=(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return (a == b) || (a < b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator!=(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return !(a == b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator>(const asset<Major, Hardfork, Release> &a,
                                  const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return !(a <= b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend bool operator>=(const asset<Major, Hardfork, Release> &a,
                                   const asset<ArgumentMajor, ArgumentHardfork, ArgumentRelease> &b) {
                return !(a < b);
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend asset<Major, Hardfork, Release> operator-(const asset<Major, Hardfork, Release> &a,
                                                             const asset<ArgumentMajor, ArgumentHardfork,
                                                                     ArgumentRelease> &b) {
                FC_ASSERT(a.symbol == b.symbol);
                FC_ASSERT(a.get_decimals() == b.get_decimals());
                asset<Major, Hardfork, Release> amount(a.amount - b.amount, a.symbol);
                amount.set_decimals(a.get_decimals());
                return amount;
            }

            template<uint8_t ArgumentMajor, uint8_t ArgumentHardfork, uint16_t ArgumentRelease>
            friend asset<Major, Hardfork, Release> operator+(const asset<Major, Hardfork, Release> &a,
                                                             const asset<ArgumentMajor, ArgumentHardfork,
                                                                     ArgumentRelease> &b) {
                FC_ASSERT(a.symbol == b.symbol);
                FC_ASSERT(a.get_decimals() == b.get_decimals());
                asset<Major, Hardfork, Release> amount(a.amount + b.amount, a.symbol);
                amount.set_decimals(a.get_decimals());
                return amount;
            }

            static share_type scaled_precision(uint8_t precision) {
                FC_ASSERT(precision < 19);
                return scaled_precision_lut[precision];
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct price : public static_version<Major, Hardfork, Release> {
            price(const asset<Major, Hardfork, Release> &input_base = asset<Major, Hardfork, Release>(0,
                                                                                                      STEEM_SYMBOL_NAME),
                  const asset<Major, Hardfork, Release> &input_quote = asset<Major, Hardfork, Release>(0,
                                                                                                       STEEM_SYMBOL_NAME))
                    : base(input_base), quote(input_quote) {
            }

            asset<Major, Hardfork, Release> base;
            asset<Major, Hardfork, Release> quote;

            static price max(asset_name_type base, asset_name_type quote);

            static price min(asset_name_type base, asset_name_type quote);

            price max() const {
                return price::max(base.symbol_name(), quote.symbol_name());
            }

            price min() const {
                return price::min(base.symbol_name(), quote.symbol_name());
            }

            double to_real() const {
                return base.to_real() / quote.to_real();
            }

            bool is_null() const;

            void validate() const;

            /**
             *  The black swan price is defined as debt/collateral, we want to perform a margin call
             *  before debt == collateral. Given a debt/collateral ratio of 1 USD / CORE and
             *  a maintenance collateral requirement of 2x we can define the call price to be
             *  2 USD / CORE.
             *
             *  This method divides the collateral by the maintenance collateral ratio to derive
             *  a call price for the given black swan ratio.
             *
             *  There exists some cases where the debt and collateral values are so small that
             *  dividing by the collateral ratio will result in a 0 price or really poor
             *  rounding errors.   No matter what the collateral part of the price ratio can
             *  never go to 0 and the debt can never go more than STEEMIT_MAX_SHARE_SUPPLY
             *
             *  CR * DEBT/COLLAT or DEBT/(COLLAT/CR)
             *
             * @param debt
             * @param collateral
             * @param collateral_ratio
             * @return
             */

            static price<Major, Hardfork, Release> call_price(const asset<Major, Hardfork, Release> &debt,
                                                              const asset<Major, Hardfork, Release> &collateral,
                                                              uint16_t collateral_ratio);

            /// The unit price for an asset type A is defined to be a price such that for any asset m, m*A=m
            static price<Major, Hardfork, Release> unit_price(asset_name_type a = STEEM_SYMBOL_NAME) {
                return price(asset<Major, Hardfork, Release>(1, a), asset<Major, Hardfork, Release>(1, a));
            }
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        asset<Major, Hardfork, Release> operator*(const asset<Major, Hardfork, Release> &a,
                                                  const price<Major, Hardfork, Release> &b) {
            if (a.symbol == b.base.symbol) {
                FC_ASSERT(b.base.amount.value > 0);
                uint128_t result = (uint128_t(a.amount.value) * b.quote.amount.value) / b.base.amount.value;
                FC_ASSERT(result.hi == 0);
                return asset<Major, Hardfork, Release>(result.to_uint64(), b.quote.symbol);
            } else if (a.symbol == b.quote.symbol) {
                FC_ASSERT(b.quote.amount.value > 0);
                uint128_t result = (uint128_t(a.amount.value) * b.base.amount.value) / b.quote.amount.value;
                FC_ASSERT(result.hi == 0);
                return asset<Major, Hardfork, Release>(result.to_uint64(), b.base.symbol);
            }
            FC_THROW_EXCEPTION(fc::assert_exception, "invalid asset * price", ("asset", a)("price", b));
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        price<Major, Hardfork, Release> operator/(const asset<Major, Hardfork, Release> &base,
                                                  const asset<Major, Hardfork, Release> &quote) {
            try {
                FC_ASSERT(base.symbol != quote.symbol);
                return price<Major, Hardfork, Release>(base, quote);
            } FC_CAPTURE_AND_RETHROW((base)(quote))
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        inline price<Major, Hardfork, Release> operator~(const price<Major, Hardfork, Release> &p) {
            return {p.quote, p.base};
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool operator==(const price<Major, Hardfork, Release> &a, const price<Major, Hardfork, Release> &b) {
            if (std::tie(a.base.symbol, a.quote.symbol) != std::tie(b.base.symbol, b.quote.symbol)) {
                return false;
            }

            const auto amult = uint128_t(b.quote.amount.value) * a.base.amount.value;
            const auto bmult = uint128_t(a.quote.amount.value) * b.base.amount.value;

            return amult == bmult;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool operator<(const price<Major, Hardfork, Release> &a, const price<Major, Hardfork, Release> &b) {
            if (a.base.symbol < b.base.symbol) {
                return true;
            }
            if (a.base.symbol > b.base.symbol) {
                return false;
            }
            if (a.quote.symbol < b.quote.symbol) {
                return true;
            }
            if (a.quote.symbol > b.quote.symbol) {
                return false;
            }

            const auto amult = uint128_t(b.quote.amount.value) * a.base.amount.value;
            const auto bmult = uint128_t(a.quote.amount.value) * b.base.amount.value;

            return amult < bmult;
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool operator<=(const price<Major, Hardfork, Release> &a, const price<Major, Hardfork, Release> &b) {
            return (a == b) || (a < b);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool operator!=(const price<Major, Hardfork, Release> &a, const price<Major, Hardfork, Release> &b) {
            return !(a == b);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool operator>(const price<Major, Hardfork, Release> &a, const price<Major, Hardfork, Release> &b) {
            return !(a <= b);
        }

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        bool operator>=(const price<Major, Hardfork, Release> &a, const price<Major, Hardfork, Release> &b) {
            return !(a < b);
        }

        /**
         *  @class price_feed
         *  @brief defines market parameters for margin positions
         */
        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct price_feed : public static_version<Major, Hardfork, Release> {
            /**
             *  Required maintenance collateral is defined
             *  as a fixed point number with a maximum value of 10.000
             *  and a minimum value of 1.000.  (denominated in GRAPHENE_COLLATERAL_RATIO_DENOM)
             *
             *  A black swan event occurs when value_of_collateral equals
             *  value_of_debt, to avoid a black swan a margin call is
             *  executed when value_of_debt * required_maintenance_collateral
             *  equals value_of_collateral using rate.
             *
             *  Default requirement is $1.75 of collateral per $1 of debt
             *
             *  BlackSwan ---> SQR ---> MCR ----> SP
             */
            ///@{
            /**
             * Forced settlements will evaluate using this price, defined as BITASSET / COLLATERAL
             */
            price<Major, Hardfork, Release> settlement_price;

            /// Price at which automatically exchanging this asset for CORE from fee pool occurs (used for paying fees)
            price<Major, Hardfork, Release> core_exchange_rate;

            /** Fixed point between 1.000 and 10.000, implied fixed point denominator is GRAPHENE_COLLATERAL_RATIO_DENOM */
            uint16_t maintenance_collateral_ratio = STEEMIT_DEFAULT_MAINTENANCE_COLLATERAL_RATIO;

            /** Fixed point between 1.000 and 10.000, implied fixed point denominator is GRAPHENE_COLLATERAL_RATIO_DENOM */
            uint16_t maximum_short_squeeze_ratio = STEEMIT_DEFAULT_MAX_SHORT_SQUEEZE_RATIO;

            /**
             *  When updating a call order the following condition must be maintained:
             *
             *  debt * maintenance_price() < collateral
             *  debt * settlement_price    < debt * maintenance
             *  debt * maintenance_price() < debt * max_short_squeeze_price()
            price maintenance_price()const;
             */

            /** When selling collateral to pay off debt, the least amount of debt to receive should be
             *  min_usd = max_short_squeeze_price() * collateral
             *
             *  This is provided to ensure that a black swan cannot be trigged due to poor liquidity alone, it
             *  must be confirmed by having the max_short_squeeze_price() move below the black swan price.
             */
            price<Major, Hardfork, Release> max_short_squeeze_price() const;
            ///@}

            friend bool operator==(const price_feed<Major, Hardfork, Release> &a,
                                   const price_feed<Major, Hardfork, Release> &b) {
                return std::tie(a.settlement_price, a.maintenance_collateral_ratio, a.maximum_short_squeeze_ratio) ==
                       std::tie(b.settlement_price, b.maintenance_collateral_ratio, b.maximum_short_squeeze_ratio);
            }

            void validate() const;

            bool is_for(asset_name_type asset_name) const;
        };

    }
} // golos::protocol

namespace fc {
    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
    inline void to_variant(const golos::protocol::asset<Major, Hardfork, Release> &var, fc::variant &vo) {
        vo = var.to_string();
    }

    template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
    inline void from_variant(const fc::variant &var, golos::protocol::asset<Major, Hardfork, Release> &vo) {
        vo = golos::protocol::asset<Major, Hardfork, Release>::from_string(var.as_string());
    }
}

FC_REFLECT((golos::protocol::asset_interface<0, 16, 0, golos::protocol::asset_symbol_type,
        golos::protocol::share_type>), (amount)(symbol))

FC_REFLECT((golos::protocol::asset_interface<0, 17, 0, golos::protocol::asset_name_type,
        golos::protocol::share_type>), (amount)(symbol))

FC_REFLECT_DERIVED((golos::protocol::asset<0, 16, 0>),
                   ((golos::protocol::asset_interface<0, 16, 0, golos::protocol::asset_symbol_type,
                           golos::protocol::share_type>)),)
FC_REFLECT_DERIVED((golos::protocol::asset<0, 17, 0>),
                   ((golos::protocol::asset_interface<0, 17, 0, golos::protocol::asset_name_type,
                           golos::protocol::share_type>)), (decimals))

FC_REFLECT((golos::protocol::price<0, 16, 0>), (base)(quote))
FC_REFLECT((golos::protocol::price<0, 17, 0>), (base)(quote))

FC_REFLECT((golos::protocol::price_feed<0, 16, 0>),
           (settlement_price)(maintenance_collateral_ratio)(maximum_short_squeeze_ratio)(core_exchange_rate))
FC_REFLECT((golos::protocol::price_feed<0, 17, 0>),
           (settlement_price)(maintenance_collateral_ratio)(maximum_short_squeeze_ratio)(core_exchange_rate))