file(GLOB HEADERS "include/steemit/blockchain_statistics/*.hpp")
# 
# libcds SHOULD only be included as the dynamic library!!!
#
# First find it in system. Default searching path is cds "make install" path:
# for .SO file: usr/local/lib64 and for cds headers: usr/local/include
# 
include (FindLibcds.cmake)
locate_libcds()
# 
# If cds has not built yet, then it has to be added with git submodule
# 
if (NOT ${LIBCDS_IS_BUILT}) 
# Building git submoudle
    include(AddLibcds.cmake)
    add_libcds()
endif()
# 
# Now everything should be fine.
# 
# Linking CDS to the plugin.
# 
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
