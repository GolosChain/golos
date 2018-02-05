#ifndef GOLOS_ASSET_VIRTUAL_OPERATIONS_HPP
#define GOLOS_ASSET_VIRTUAL_OPERATIONS_HPP

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>

namespace golos {
    namespace protocol {
        /**
         * Virtual op generated when force settlement is cancelled.
         */

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        struct asset_settle_cancel_operation : public virtual_operation<Major, Hardfork, Release> {
            integral_id_type settlement;
            /// Account requesting the force settlement. This account pays the fee
            account_name_type account;
            /// Amount of asset to force settle. This must be a market-issued asset
            asset <Major, Hardfork, Release> amount;
            extensions_type extensions;

            void validate() const {
                FC_ASSERT(amount.amount > 0, "Must settle at least 1 unit");
            }
        };
    }
}

FC_REFLECT((golos::protocol::asset_settle_cancel_operation<0, 16, 0>), (settlement)(account)(amount)(extensions))
FC_REFLECT((golos::protocol::asset_settle_cancel_operation<0, 17, 0>), (settlement)(account)(amount)(extensions))

#endif //GOLOS_ASSET_VIRTUAL_OPERATIONS_HPP
