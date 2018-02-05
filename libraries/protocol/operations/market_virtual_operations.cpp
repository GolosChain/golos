#include <golos/protocol/operations/market_virtual_operations.hpp>
#include <golos/protocol/operations/operation_utilities_impl.hpp>

namespace fc {
    void to_variant(const golos::protocol::market_virtual_operations &var, fc::variant &vo) {
        var.visit(from_operation<versioned_from_operation_policy>(vo));
    }

    void from_variant(const fc::variant &var, golos::protocol::market_virtual_operations &vo) {
        static std::map<string, uint32_t> to_tag = []() {
            std::map<string, uint32_t> name_map;
            for (int i = 0; i < golos::protocol::market_virtual_operations::count(); ++i) {
                golos::protocol::market_virtual_operations tmp;
                tmp.set_which(i);
                string n;
                tmp.visit(get_operation_name(n));
                name_map[n] = i;
            }
            return name_map;
        }();

        auto ar = var.get_array();
        if (ar.size() < 2) {
            return;
        }
        if (ar[0].is_uint64()) {
            vo.set_which(ar[0].as_uint64());
        } else {
            std::string operation_name = golos::version::state::instance().current_version.hardfork() <= 16 ? (
                    boost::format(ar[0].as_string().append("<%1%, %2%, %3%>")) % 0 % 16 % 0).str() : (
                                                 boost::format(ar[0].as_string().append("<%1%, %2%, %3%>")) %
                                                 golos::version::state::instance().current_version.major() %
                                                 golos::version::state::instance().current_version.hardfork() %
                                                 golos::version::state::instance().current_version.release()).str();
            auto itr = to_tag.find(operation_name);
            FC_ASSERT(itr != to_tag.end(), "Invalid operation name: ${n}", ("n", ar[0]));
            vo.set_which(to_tag[operation_name]);
        }
        vo.visit(fc::to_static_variant(ar[1]));
    }
}
