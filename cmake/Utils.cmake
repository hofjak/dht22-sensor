
function(check_and_warn_tmpfs)

    find_program(STAT_EXECUTABLE stat)

    set(COLOR_RESET "")
    set(BOLD_YELLOW "")
    set(YELLOW "")

    if (NOT WIN32)
        string(ASCII 27 Esc)
        set(COLOR_RESET   "${Esc}[m")
        set(YELLOW        "${Esc}[33m")
        set(BOLD_YELLOW   "${Esc}[1;33m")
    endif()

    set(TMPFS_CHECK "none")

    execute_process(COMMAND ${STAT_EXECUTABLE} --file-system --format=%T ${PROJECT_BINARY_DIR}
        OUTPUT_VARIABLE TMPFS_CHECK
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT ("${TMPFS_CHECK}" STREQUAL "tmpfs"))
        string(CONCAT WARNING 
            "${BOLD_YELLOW}WARNING:${COLOR_RESET}${YELLOW} "
            "Build directory is not a tmpfs - "
            "Frequent compilation will be slow and bad for the hard disk${COLOR_RESET}"
        )
        message(STATUS "${WARNING}")
    endif()

endfunction()


function(symlink_files_to_current_binary_dir)

    execute_process(COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/Kbuild ${CMAKE_CURRENT_BINARY_DIR} ERROR_QUIET)

    file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

    foreach(currentSource ${SOURCES})
        execute_process(COMMAND ln -s ${currentSource} ${CMAKE_CURRENT_BINARY_DIR} ERROR_QUIET)
    endforeach()

endfunction()
