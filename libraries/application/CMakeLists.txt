set(CURRENT_TARGET application)

list(APPEND ${CURRENT_TARGET}_HEADERS
     include/golos/application/api.hpp
     include/golos/application/api_access.hpp
     include/golos/application/api_context.hpp
     include/golos/application/application.hpp
     include/golos/application/applied_operation.hpp
     include/golos/application/database_api.hpp
     include/golos/application/discussion_query.hpp
     include/golos/application/impacted.hpp
     include/golos/application/plugin.hpp
     include/golos/application/state.hpp
     include/golos/application/api_objects/steem_api_objects.hpp
     include/golos/application/api_objects/comment_api_object.hpp
        )

list(APPEND ${CURRENT_TARGET}_SOURCES
        database_api.cpp
        api.cpp
        application.cpp
        impacted.cpp
        plugin.cpp
        )

if(BUILD_SHARED_LIBRARIES)
    add_library(golos_${CURRENT_TARGET} SHARED
            ${${CURRENT_TARGET}_HEADERS}
            ${${CURRENT_TARGET}_SOURCES}
            )
else()
    add_library(golos_${CURRENT_TARGET} STATIC
            ${${CURRENT_TARGET}_HEADERS}
            ${${CURRENT_TARGET}_SOURCES}
            )
endif()

add_library(golos::${CURRENT_TARGET} ALIAS golos_${CURRENT_TARGET} )
set_property(TARGET golos_${CURRENT_TARGET} PROPERTY EXPORT_NAME ${CURRENT_TARGET})

target_link_libraries(golos_${CURRENT_TARGET} golos::chain golos::protocol golos::snapshot golos::tags golos::languages golos_follow golos_mf_plugins fc golos::network graphene_utilities golos::market_history)

target_include_directories(golos_${CURRENT_TARGET}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
    set_source_files_properties(application.cpp api.cpp database_api.cpp PROPERTIES COMPILE_FLAGS "/bigobj")
endif(MSVC)

install(TARGETS
        golos_${CURRENT_TARGET}

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
install(FILES ${${CURRENT_TARGET}_HEADERS} DESTINATION "include/golos/${CURRENT_TARGET}")