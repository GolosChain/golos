set(CURRENT_TARGET worker_api)

list(APPEND CURRENT_TARGET_HEADERS
        include/golos/plugins/worker_api/worker_api_plugin.hpp
        include/golos/plugins/worker_api/worker_api_objects.hpp
        include/golos/plugins/worker_api/worker_api_queries.hpp
        include/golos/plugins/worker_api/worker_api_sorts.hpp
        )

list(APPEND CURRENT_TARGET_SOURCES
        worker_api_plugin.cpp
        worker_api_queries.cpp
        )

if(BUILD_SHARED_LIBRARIES)
    add_library(golos_${CURRENT_TARGET} SHARED
            ${CURRENT_TARGET_HEADERS}
            ${CURRENT_TARGET_SOURCES}
            )
else()
    add_library(golos_${CURRENT_TARGET} STATIC
            ${CURRENT_TARGET_HEADERS}
            ${CURRENT_TARGET_SOURCES}
            )
endif()

add_library(golos::${CURRENT_TARGET} ALIAS golos_${CURRENT_TARGET})
set_property(TARGET golos_${CURRENT_TARGET} PROPERTY EXPORT_NAME ${CURRENT_TARGET})

target_link_libraries(
        golos_${CURRENT_TARGET}
        golos::chain_plugin
        golos::protocol
        golos::network
        golos::json_rpc
        golos::api
        golos::follow
        golos::social_network
        graphene_utilities
        graphene_time
        appbase
)
target_include_directories(golos_${CURRENT_TARGET}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

install(TARGETS
        golos_${CURRENT_TARGET}

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

