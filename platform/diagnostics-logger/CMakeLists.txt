cmake_minimum_required(VERSION 3.6)                                                                 
                                                                                                    
if (VICOS)

project(diagnostics-logger)

configure_file(diagnostics-logger ${CMAKE_CURRENT_BINARY_DIR}/diagnostics-logger @ONLY)

# install script into bin
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/diagnostics-logger
        DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                    GROUP_EXECUTE GROUP_READ
)

endif(VICOS)
