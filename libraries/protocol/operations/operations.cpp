#include <golos/protocol/operations/operations.hpp>

#include <golos/protocol/operations/operation_utilities_impl.hpp>

namespace golos {
    namespace protocol {

        struct is_market_op_visitor {
            typedef bool result_type;

            template<typename T>
            bool operator()(T &&v) const {
                return false;
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            bool operator()(const limit_order_create_operation<Major, Hardfork, Release> &) const {
                return true;
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            bool operator()(const limit_order_create2_operation<Major, Hardfork, Release> &) const {
                return true;
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            bool operator()(const limit_order_cancel_operation<Major, Hardfork, Release> &) const {
                return true;
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            bool operator()(const call_order_update_operation<Major, Hardfork, Release> &) const {
                return true;
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            bool operator()(const bid_collateral_operation<Major, Hardfork, Release> &) const {
                return true;
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            bool operator()(const transfer_operation<Major, Hardfork, Release> &) const {
                return true;
            }

            template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
            bool operator()(const transfer_to_vesting_operation<Major, Hardfork, Release> &) const {
                return true;
            }
        };

        bool is_market_operation(const operation &op) {
            return op.visit(is_market_op_visitor());
        }

        struct is_vop_visitor {
            typedef bool result_type;

            template<typename T>
            bool operator()(const T &v) const {
                return v.is_virtual();
            }
        };

        bool is_virtual_operation(const operation &op) {
            return op.visit(is_vop_visitor());
        }
    }
} // golos::protocol

namespace fc {

    void to_variant(const golos::protocol::operation &var, fc::variant &vo) {
        var.visit(from_operation<versioned_from_operation_policy>(vo));
    }

    void from_variant(const fc::variant &var, golos::protocol::operation &vo) {
        std::map<string, uint32_t> name_map;
        for (unsigned int i = 0; i < golos::protocol::operation::count(); ++i) {
            golos::protocol::operation tmp;
            tmp.set_which(i);
            string n;
            tmp.visit(get_operation_name(n));
            name_map[n] = i;
        }

        const variants &ar = var.get_array();
        if (ar.size() < 2) {
            return;
        }
        if (ar[0].is_uint64()) {
            vo.set_which(ar[0].as_uint64());
        } else {
            std::string operation_name;

            if (golos::version::state::instance().current_version.hardfork() <= 16) {
                operation_name = (boost::format(ar[0].as_string().append("_operation<%1%, %2%, %3%>")) % 0 % 16 %
                                  0).str();
            } else {
                std::stringstream major_stream;
                major_stream << std::dec << std::to_string(golos::version::state::instance().current_version.major());

                std::stringstream hardfork_stream;
                hardfork_stream << std::dec << std::to_string(golos::version::state::instance().current_version.hardfork());

                std::stringstream release_stream;
                release_stream << std::dec << std::to_string(golos::version::state::instance().current_version.release());

                operation_name = (boost::format(ar[0].as_string().append("_operation<%1%, %2%, %3%>")) %
                                  major_stream.str() % hardfork_stream.str() % release_stream.str()).str();
            }

            std::map<string, uint32_t>::const_iterator itr = name_map.find(operation_name);
            FC_ASSERT(itr != name_map.end(), "Invalid operation name: ${n}", ("n", operation_name));
            vo.set_which(name_map[operation_name]);
        }

        vo.visit(fc::to_static_variant(ar[1]));
    }
}

namespace golos {
    namespace protocol {
        void operation_validate(const operation &op) {
            op.visit(golos::protocol::operation_validate_visitor());
        }

        void operation_get_required_authorities(const operation &op, flat_set<protocol::account_name_type> &active,
                                                flat_set<protocol::account_name_type> &owner,
                                                flat_set<protocol::account_name_type> &posting,
                                                std::vector<authority> &other) {
            op.visit(golos::protocol::operation_get_required_auth_visitor(active, owner, posting, other));
        }
    }
}