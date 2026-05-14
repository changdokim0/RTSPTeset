cmake_minimum_required(VERSION 3.2.2...3.29.3)

set(TARGET_LIB archive_manager)

file(GLOB SRC_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/osal/*.cc)

add_library(${TARGET_LIB} SHARED ${SRC_FILES})

# includes
target_include_directories(${TARGET_LIB} PRIVATE ${WISENET_PRODUCT_LAYER_PATH}/includes)
target_include_directories(${TARGET_LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
target_include_directories(${TARGET_LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/modules/includes)
target_include_directories(${TARGET_LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/osal/includes)
target_include_directories(${TARGET_LIB} PRIVATE ${WISENET_PRODUCT_LAYER_PATH}/technical/blaze_logger)
target_link_libraries(${TARGET_LIB} PRIVATE blaze_logger)
#compile options
target_compile_definitions(${TARGET_LIB} PRIVATE ARCHIVE_MANAGER_EXPORTS)
if(WIN32)
    target_compile_definitions(${TARGET_LIB} PRIVATE APPLICATION_NAME="${TARGET_LIB}.dll")
else()
    target_compile_definitions(${TARGET_LIB} PRIVATE APPLICATION_NAME="lib${TARGET_LIB}.so")
endif()
#install
if(WIN32)    
	target_link_libraries(${TARGET_LIB} PRIVATE ${CMAKE_OPENSSL_LIB_PATH}/libcrypto.lib ${CMAKE_OPENSSL_LIB_PATH}/libssl.lib)
	install(TARGETS ${TARGET_LIB} RUNTIME DESTINATION ${RFS_INSTALL_PATH}/${TARGET_PLATFORM})
else()
	target_link_libraries(${TARGET_LIB} PRIVATE crypto ssl)
	install(TARGETS ${TARGET_LIB} DESTINATION ${RFS_INSTALL_PATH}/${TARGET_PLATFORM}/lib)
endif()
