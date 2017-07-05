file(GLOB HEADERS "include/steemit/blockchain_statistics/*.hpp")

# libcds SHOULD only be included as the dynamic library!!!

find_path(libcds_INCLUDE_DIRS NAMES cds PATH_SUFFIXES ${ARGN})
set (libcds_INCLUDE_DIRS "${libcds_INCLUDE_DIRS}/libcds")
message ("${libcds_INCLUDE_DIRS}")

find_path(libcds_SO_FILE_PATH NAMES libcds_d.so
    HINTS   ${libcds_INCLUDE_DIRS}/libcds/bin/
            /usr/local/lib64/
    PATH_SUFFIXES ${ARGN} NO_DEFAULT_PATH
    )

if(NOT libcds_INCLUDE_DIRS OR NOT libcds_SO_FILE_PATH)
    message(FATAL_ERROR "libcds development files are required to build.")
else()
    message(STATUS "Found libcds. PATH: ${libcds_INCLUDE_DIRS}")
    message(STATUS "Found libcds_d.so PATH: ${libcds_SO_FILE_PATH}")
endif()

include_directories(SYSTEM ${libcds_INCLUDE_DIRS})
add_library(libcds SHARED IMPORTED)

set_target_properties(libcds PROPERTIES
    IMPORTED_LOCATION "${libcds_SO_FILE_PATH}/libcds_d.so"
    INTERFACE_INCLUDE_DIRECTORIES "${libcds_INCLUDE_DIRS}/cds"
)
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

# TODO !!!
# LINKING boost components should be fixed.
target_link_libraries(golos_blockchain_statistics golos_chain golos_protocol
                    golos::application ${CMAKE_THREAD_LIBS_INIT} libcds
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
