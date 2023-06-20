project(examples)


file(GLOB EXAMPLES_SOURCES "*.c")


foreach(currentFile ${EXAMPLES_SOURCES})
    get_filename_component(currentName ${currentFile} NAME_WE)
    add_executable(${currentName} ${currentFile})
endforeach(currentFile)


target_link_libraries(ex2 pthread)
