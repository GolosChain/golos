file(GLOB HEADERS "include/golos/blockchain_statistics/*.hpp")

if(BUILD_SHARED_LIBRARIES)
    add_library(golos_blockchain_statistics SHARED
            blockchain_statistics_plugin.cpp
            blockchain_statistics_api.cpp
            statistics_sender.cpp
            )
else()
    add_library(golos_blockchain_statistics STATIC
            blockchain_statistics_plugin.cpp
            blockchain_statistics_api.cpp
            statistics_sender.cpp
            )
endif()

FIND_PACKAGE(Boost  REQUIRED COMPONENTS
        system
        thread
        )

target_link_libraries(
        golos_blockchain_statistics
        golos_chain
        golos_protocol
        golos::application
        ${CMAKE_THREAD_LIBS_INIT}
        ${CURRENT_TARGET}
        pthread
        ${Boost_INCLUDE_DIRS}
)


target_include_directories(golos_blockchain_statistics
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

install(TARGETS
        golos_blockchain_statistics

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
