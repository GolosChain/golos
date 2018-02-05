#pragma once

#include <golos/version/version_state.hpp>

#include <fc/static_variant.hpp>

namespace fc {
    std::string name_from_type(const std::string &type_name);

    class from_operation_policy_interface {

    };

    class from_operation_policy : public from_operation_policy_interface {

    };

    class versioned_from_operation_policy : public from_operation_policy_interface {

    };

    template<typename Policy,
            typename = typename std::enable_if<std::is_base_of<from_operation_policy_interface, Policy>::value>::type>
    class from_operation {
    public:
        variant &var;

        from_operation(variant &dv) : var(dv) {
        }

        typedef void result_type;

        template<typename T>
        void operator()(const T &v) const {
            std::string name = name_from_type(fc::get_typename<T>::name());
            var = variant(std::make_pair(name, v));
        }
    };

    template<>
    class from_operation<versioned_from_operation_policy> {
    public:
        variant &var;

        from_operation(variant &dv) : var(dv) {
        }

        typedef void result_type;

        template<typename T>
        void operator()(const T &v) const {
            std::string name = name_from_type(fc::get_typename<T>::name());
            var = variant(std::make_pair(name.substr(0, name.find_last_of('_')), v));
        }
    };

    class get_operation_name {
    public:
        string &name;

        get_operation_name(string &dv) : name(dv) {
        }

        typedef void result_type;

        template<typename T>
        void operator()(const T &v) const {
            name = name_from_type(fc::get_typename<T>::name());
        }
    };
}

namespace golos {
    namespace protocol {

        struct operation_validate_visitor {
            typedef void result_type;

            template<typename T>
            void operator()(const T &v) const {
                v.validate();
            }
        };

        struct operation_get_required_auth_visitor {
            typedef void result_type;

            flat_set<protocol::account_name_type> &active;
            flat_set<protocol::account_name_type> &owner;
            flat_set<protocol::account_name_type> &posting;
            std::vector<authority> &other;

            operation_get_required_auth_visitor(flat_set<protocol::account_name_type> &a,
                                                flat_set<protocol::account_name_type> &own,
                                                flat_set<protocol::account_name_type> &post,
                                                std::vector<authority> &oth) : active(a), owner(own), posting(post),
                    other(oth) {
            }

            template<typename T>
            void operator()(const T &v) const {
                v.get_required_active_authorities(active);
                v.get_required_owner_authorities(owner);
                v.get_required_posting_authorities(posting);
                v.get_required_authorities(other);
            }
        };

    }
} // golos::protocol
