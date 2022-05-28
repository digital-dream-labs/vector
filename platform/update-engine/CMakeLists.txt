cmake_minimum_required(VERSION 3.6)                                                                 
                                                                                                    
if (VICOS)

project(update-engine)

# copy update-engine.py to rename it to update-engine
configure_file(update-engine.py ${CMAKE_CURRENT_BINARY_DIR}/update-engine @ONLY)

# install script into bin
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/update-engine
        DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                    GROUP_EXECUTE GROUP_READ
)

endif(VICOS)
