file(GLOB HEADERS "include/steemit/blockchain_statistics/*.hpp" "include/*.hpp")
# Including statistics_sender.hpp

set (LIBCDS_DIR /home/anton/code/libcds-test/libcds/)

# libcds SHOULD be included as dynamic library!!!
include_directories(SYSTEM ${LIBCDS_DIR})
# add_library(${LIBCDS_DIR} SHARED IMPORTED) 


if(BUILD_SHARED_LIBRARIES)
    add_library(golos_blockchain_statistics SHARED
            blockchain_statistics_plugin.cpp
            blockchain_statistics_api.cpp
            statistics_sender.cpp
            ${LIBCDS_DIR}
            )
    set_target_properties(libcds PROPERTIES
        IMPORTED_LOCATION "${LIBCDS_DIR}bin/libcds_d.so"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBCDS_DIR}cds"
    )
else()
    add_library(golos_blockchain_statistics STATIC
            blockchain_statistics_plugin.cpp
            blockchain_statistics_api.cpp
            statistics_sender.cpp
            # ${LIBCDS_DIR} HOW TO ADD LIBCDS AS A STATIC LIB???
            )
endif()

target_link_libraries(golos_blockchain_statistics golos_chain golos_protocol
                    golos::application ${LIBCDS_DIR} ${CMAKE_THREAD_LIBS_INIT}
                    -lpthread -lboost_thread -lboost_system
                    )

target_include_directories(golos_blockchain_statistics
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

install(TARGETS
        golos_blockchain_statistics

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )