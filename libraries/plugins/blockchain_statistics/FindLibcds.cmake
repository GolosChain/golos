function(locate_libcds)
    if (libcds_INCLUDE_DIRS AND libcds_SO_FILE_PATH)
        set (LIBCDS_IS_BUILT TRUE PARENT_SCOPE)
        return()
    endif()

    find_path(libcds_INCLUDE_DIRS NAMES cds PATH_SUFFIXES ${ARGN})

    find_path(libcds_SO_FILE_PATH NAMES libcds_d.so
        HINTS   ${libcds_INCLUDE_DIRS}/libcds/bin/
                /usr/local/lib64/
        PATH_SUFFIXES ${ARGN} NO_DEFAULT_PATH
        )

    if(NOT libcds_INCLUDE_DIRS OR NOT libcds_SO_FILE_PATH)
        set (LIBCDS_IS_BUILT FALSE PARENT_SCOPE)
    else()
        message(STATUS "Found libcds. PATH: ${libcds_INCLUDE_DIRS}")
        message(STATUS "Found libcds_d.so PATH: ${libcds_SO_FILE_PATH}")
        set (LIBCDS_IS_BUILT TRUE PARENT_SCOPE)
    endif()
endfunction()