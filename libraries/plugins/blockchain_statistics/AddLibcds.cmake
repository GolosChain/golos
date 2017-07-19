function(add_libcds)
    add_subdirectory(${GOLOS_ROOT_DIR}/libraries/libcds/ ${GOLOS_ROOT_DIR}/libraries/libcds/)

    find_path(libcds_INCLUDE_DIRS NAMES cds
        HINTS   ${GOLOS_ROOT_DIR}/libraries/libcds/
        PATH_SUFFIXES ${ARGN} NO_DEFAULT_PATH
        )

    find_path(libcds_SO_FILE_PATH NAMES libcds_d.so
        HINTS   ${GOLOS_ROOT_DIR}/libraries/libcds/bin/
                # This one is very strange, but it works
                ${GOLOS_ROOT_DIR}/libraries/libcds/bin/bin/
        PATH_SUFFIXES ${ARGN} NO_DEFAULT_PATH
        )
endfunction()