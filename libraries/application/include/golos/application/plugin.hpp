#pragma once

#include <golos/application/application.hpp>
#include <golos/chain/evaluator.hpp>

#include <boost/program_options.hpp>

#include <fc/io/json.hpp>
#include <fc/static_variant.hpp>
#include <fc/unique_ptr.hpp>
#include <fc/vector.hpp>

#include <memory>

namespace golos {
    namespace application {

        using fc::static_variant;
        using fc::unique_ptr;
        using std::vector;

        class abstract_plugin {
        public:
            virtual ~abstract_plugin() {
            }

            virtual std::string plugin_name() const = 0;

            /**
             * @brief Perform early startup routines and register plugin indexes, callbacks, etc.
             *
             * Plugins MUST supply a method initialize() which will be called early in the application startup. This method
             * should contain early setup code such as initializing variables, adding indexes to the database, registering
             * callback methods from the database, adding APIs, etc., as well as applying any options in the @ref options map
             *
             * This method is called BEFORE the database is open, therefore any routines which require any chain state MUST
             * NOT be called by this method. These routines should be performed in startup() instead.
             *
             * @param options The options passed to the application, via configuration files or command line
             */
            virtual void plugin_initialize(const boost::program_options::variables_map &options) = 0;

            /**
             * @brief Begin normal runtime operations
             *
             * Plugins MUST supply a method startup() which will be called at the end of application startup. This method
             * should contain code which schedules any tasks, or requires chain state.
             */
            virtual void plugin_startup() = 0;

            /**
             * @brief Cleanly shut down the plugin.
             *
             * This is called to request a clean shutdown (e.g. due to SIGINT or SIGTERM).
             */
            virtual void plugin_shutdown() = 0;

            /**
             * @brief Fill in command line parameters used by the plugin.
             *
             * @param command_line_options All options this plugin supports taking on the command-line
             * @param config_file_options All options this plugin supports storing in a configuration file
             *
             * This method populates its arguments with any
             * command-line and configuration file options the plugin supports.
             * If a plugin does not need these options, it
             * may simply provide an empty implementation of this method.
             */
            virtual void plugin_set_program_options(
                    boost::program_options::options_description &command_line_options,
                    boost::program_options::options_description &config_file_options
            ) = 0;

        };

/**
 * Provides basic default implementations of abstract_plugin functions.
 */
        class plugin : public abstract_plugin {
        public:
            plugin(application *app);

            virtual ~plugin() override;

            virtual std::string plugin_name() const override;

            virtual void plugin_initialize(const boost::program_options::variables_map &options) override;

            virtual void plugin_startup() override;

            virtual void plugin_shutdown() override;

            virtual void plugin_set_program_options(
                    boost::program_options::options_description &command_line_options,
                    boost::program_options::options_description &config_file_options
            ) override;

            chain::database &database() {
                return *app().chain_database();
            }

            application &app() const {
                assert(_app);
                return *_app;
            }

        protected:
            network::node &p2p_node() {
                return *app().p2p_node();
            }

        private:
            application *_app = nullptr;
        };

/// @group Some useful tools for boost::program_options arguments using vectors of JSON strings
/// @{
        template<typename T>
        T dejsonify(const string &s) {
            return fc::json::from_string(s).as<T>();
        }

#define DEFAULT_VALUE_VECTOR(value) default_value({fc::json::to_string(value)}, fc::json::to_string(value))
#define LOAD_VALUE_SET(options, name, container, type) \
if( options.count(name) ) { \
      const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
      std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &golos::application::dejsonify<type>); \
}
/// @}

    }
} //golos::application

#define STEEMIT_DEFINE_PLUGIN(plugin_name, plugin_class) \
   namespace golos { namespace plugin { \
   std::shared_ptr< golos::application::abstract_plugin > create_ ## plugin_name ## _plugin( application::application* application )  \
   { return std::make_shared< plugin_class >( application ); } \
   } }

#define DEFINE_PLUGIN_EVALUATOR(PLUGIN, OPERATION, X)                     \
class X ## _evaluator : public golos::chain::evaluator< X ## _evaluator, OPERATION > \
{                                                                           \
   public:                                                                  \
      typedef X ## _operation operation_type;                               \
                                                                            \
      X ## _evaluator( chain::database& db, PLUGIN* plugin )                       \
         : golos::chain::evaluator< X ## _evaluator, OPERATION >( db ), \
           _plugin( plugin )                                                \
      {}                                                                    \
                                                                            \
      void do_apply( const X ## _operation& o );                            \
                                                                            \
      PLUGIN* _plugin;                                                      \
};
