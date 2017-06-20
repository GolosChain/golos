file(GLOB HEADERS "include/steemit/blockchain_statistics/*.hpp")

# Has to be changed!!! Too nooby
set (LIBCDS_DIR /home/anton/code/libcds-test/libcds)

include_directories(SYSTEM ${LIBCDS_DIR})

# libcds SHOULD only be included as dynamic library!!!

add_library(libcds SHARED IMPORTED) 

set_target_properties(libcds PROPERTIES
  IMPORTED_LOCATION "${LIBCDS_DIR}/bin/libcds_d.so"
  INTERFACE_INCLUDE_DIRECTORIES "${LIBCDS_DIR}/cds"
)



if(BUILD_SHARED_LIBRARIES)
    add_library(golos_blockchain_statistics SHARED
            blockchain_statistics_plugin.cpp
            blockchain_statistics_api.cpp
            # statistics_sender.cpp
            )
else()
    add_library(golos_blockchain_statistics STATIC
            blockchain_statistics_plugin.cpp
            blockchain_statistics_api.cpp
            statistics_sender.cpp
            )
endif()

add_library(statistics_sender SHARED statistics_sender.cpp)

target_link_libraries(golos_blockchain_statistics golos_chain golos_protocol
                    golos::application ${CMAKE_THREAD_LIBS_INIT} libcds statistics_sender
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