set(CURRENT_TARGET version)

list(APPEND ${CURRENT_TARGET}_HEADERS
     include/golos/${CURRENT_TARGET}/version.hpp
     include/golos/${CURRENT_TARGET}/version_state.hpp
     )

list(APPEND ${CURRENT_TARGET}_SOURCES
     version.cpp
     version_state.cpp
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

add_custom_target(golos_generate_${CURRENT_TARGET}
                  COMMAND "${CMAKE_COMMAND}" -DCURRENT_TARGET="${CURRENT_TARGET}" -DCURRENT_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}" -P "${PROJECT_SOURCE_DIR}/cmake/generate_hardfork.cmake")

add_dependencies(golos_${CURRENT_TARGET} golos_generate_${CURRENT_TARGET})

add_library(golos::${CURRENT_TARGET} ALIAS golos_${CURRENT_TARGET})
set_property(TARGET golos_${CURRENT_TARGET} PROPERTY EXPORT_NAME ${CURRENT_TARGET})

target_link_libraries(golos_${CURRENT_TARGET} fc chainbase graphene_schema ${PATCH_MERGE_LIB})
target_include_directories(golos_${CURRENT_TARGET}
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include")

install(TARGETS
        golos_${CURRENT_TARGET}

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )

install(FILES ${${CURRENT_TARGET}_HEADERS} DESTINATION "include/golos/${CURRENT_TARGET}")