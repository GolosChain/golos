set(CURRENT_TARGET elastic_search)

list(APPEND CURRENT_TARGET_HEADERS
        include/golos/plugins/elastic_search/elastic_search_plugin.hpp
        include/golos/plugins/elastic_search/elastic_search_objects.hpp
        )

list(APPEND CURRENT_TARGET_SOURCES
        elastic_search_plugin.cpp
        )

if(BUILD_SHARED_LIBRARIES)
  add_library(golos_elastic_search SHARED
          ${CURRENT_TARGET_HEADERS}
          ${CURRENT_TARGET_SOURCES}
          )
else()
  add_library(golos_elastic_search STATIC
          ${CURRENT_TARGET_HEADERS}
          ${CURRENT_TARGET_SOURCES}
          )
endif()

add_library(golos::${CURRENT_TARGET} ALIAS golos_${CURRENT_TARGET})

target_link_libraries(
        golos_${CURRENT_TARGET}
        golos_chain
        golos_chain_plugin
        golos_protocol
        appbase
        fc
)

target_include_directories(golos_${CURRENT_TARGET}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../"
        )

install(TARGETS
        golos_${CURRENT_TARGET}

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

