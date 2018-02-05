
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace golos {
    namespace application {

        class abstract_plugin;

        class application;

    }
}

namespace golos {
    namespace plugin {

        void initialize_plugin_factories();

        std::shared_ptr<golos::application::abstract_plugin> create_plugin(const std::string &name, golos::application::application *app);

        std::vector<std::string> get_available_plugins();

    }
}
