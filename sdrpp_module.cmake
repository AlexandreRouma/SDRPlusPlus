# Get needed values depending on if this is in-tree or out-of-tree
if (NOT SDRPP_CORE_ROOT)
    set(SDRPP_CORE_ROOT "@SDRPP_CORE_ROOT@")
endif ()
if (NOT SDRPP_MODULE_COMPILER_FLAGS)
    set(SDRPP_MODULE_COMPILER_FLAGS @SDRPP_MODULE_COMPILER_FLAGS@)
endif ()

# Created shared lib and link to core
add_library(${PROJECT_NAME} SHARED ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE sdrpp_core)
target_include_directories(${PROJECT_NAME} PRIVATE "${SDRPP_CORE_ROOT}/src/")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

# Set compile arguments
target_compile_options(${PROJECT_NAME} PRIVATE ${SDRPP_MODULE_COMPILER_FLAGS})

# Install directives
install(TARGETS ${PROJECT_NAME} DESTINATION lib/sdrpp/plugins)