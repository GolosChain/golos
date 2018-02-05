#ifndef GOLOS_SNAPSHOT_PLUGIN_HPP
#define GOLOS_SNAPSHOT_PLUGIN_HPP

#include <golos/application/application.hpp>
#include <golos/application/plugin.hpp>

#include <boost/bimap.hpp>

#include <sstream>
#include <string>

#define SNAPSHOT_PLUGIN_NAME "snapshot"

namespace golos {
    namespace plugin {
        namespace snapshot {
            namespace detail {
                class snapshot_plugin_impl;
            }

            class snapshot_plugin : public golos::application::plugin {
            public:
                /**
                 * The plugin requires a constructor which takes app.  This is called regardless of whether the plugin is loaded.
                 * The app parameter should be passed up to the superclass constructor.
                 */
                snapshot_plugin(golos::application::application *app);

                /**
                 * Plugin is destroyed via base class pointer, so a virtual destructor must be provided.
                 */
                virtual ~snapshot_plugin();

                /**
                 * Every plugin needs a name.
                 */
                virtual std::string plugin_name() const override;

                /**
                 * Called when the plugin is enabled, but before the database has been created.
                 */
                virtual void plugin_initialize(const boost::program_options::variables_map &options) override;

                virtual void plugin_set_program_options(
                        boost::program_options::options_description &command_line_options,
                        boost::program_options::options_description &config_file_options) override;

                /**
                 * Called when the plugin is enabled.
                 */
                virtual void plugin_startup() override;

                const boost::bimap<std::string, std::string> &get_loaded_snapshots() const;

            private:
                void load_snapshots(const std::vector<std::string> &snapshots);

                boost::program_options::variables_map options;

                boost::bimap<std::string, std::string> loaded_snapshots;

                friend class detail::snapshot_plugin_impl;

                golos::application::application *application;

                std::unique_ptr<detail::snapshot_plugin_impl> impl;
            };
        }
    }
}

#endif //GOLOS_SNAPSHOT_PLUGIN_HPP