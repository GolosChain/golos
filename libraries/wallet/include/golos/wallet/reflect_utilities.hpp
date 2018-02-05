#pragma once

// This file contains various reflection methods that are used to
// support the wallet, e.g. allow specifying operations by name
// instead of ID.

namespace golos {
    namespace wallet {

        struct static_variant_map {
            flat_map<string, int> name_to_which;
            vector<string> which_to_name;
        };

        namespace impl {

            std::string clean_name(const std::string &name) {
                const static std::string prefix = "golos::protocol::";
                const static std::string suffix = "_operation";
                // golos::chain::.*_operation
                if (name.find('<') != std::string::npos && name.find('>') != std::string::npos) {
                    if ((name.size() >= prefix.size() + suffix.size()) && (name.substr(0, prefix.size()) == prefix) &&
                        (name.substr(name.size() - suffix.size() - name.find('>') + name.find('<') - 1,
                                     suffix.size()) == suffix)) {

                        return name.substr(prefix.size(), name.size() - prefix.size() - suffix.size() - name.find('>') + name.find('<'));
                    }
                } else if ((name.size() >= prefix.size() + suffix.size()) &&
                           (name.substr(0, prefix.size()) == prefix) &&
                           (name.substr(name.size() - suffix.size(), suffix.size()) == suffix)) {

                    return name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
                }

                // If this line spams the console, please don't just comment it out.
                // Instead, add code above to deal specifically with the names that are causing the spam.
                wlog("don't know how to clean name: ${name}, suffix: ${suffix}", ("name", name)("suffix", name.find('<') != std::string::npos && name.find('>') != std::string::npos ? name.substr(
                        name.size() - suffix.size() - name.find('>') + name.find('<') - 1, suffix.size()) : name.substr(name.size() - suffix.size(), suffix.size())));
                return name;
            }

            struct static_variant_map_visitor {
                static_variant_map_visitor() {
                }

                typedef void result_type;

                template<typename T>
                result_type operator()(const T &dummy) {
                    assert(which == m.which_to_name.size());
                    std::string name = clean_name(fc::get_typename<T>::name());
                    m.name_to_which[name] = which;
                    m.which_to_name.push_back(name);
                }

                static_variant_map m;
                int which;
            };

            template<typename StaticVariant>
            struct from_which_visitor {
                typedef StaticVariant result_type;

                template<typename Member>
                // Member is member of static_variant
                result_type operator()(const Member &dummy) {
                    Member result;
                    from_variant(v, result);
                    return result;    // converted from StaticVariant to Result automatically due to return type
                }

                const variant &v;

                from_which_visitor(const variant &_v) : v(_v) {
                }
            };

        } // namespace impl

        template<typename T>
        T from_which_variant(int which, const variant &v) {
            // Parse a variant for a known which()
            T dummy;
            dummy.set_which(which);
            impl::from_which_visitor<T> vtor(v);
            return dummy.visit(vtor);
        }

        template<typename T>
        static_variant_map create_static_variant_map() {
            T dummy;
            int n = dummy.count();
            impl::static_variant_map_visitor vtor;
            for (int i = 0; i < n; i++) {
                dummy.set_which(i);
                vtor.which = i;
                dummy.visit(vtor);
            }
            return vtor.m;
        }

    }
} // namespace golos::wallet
