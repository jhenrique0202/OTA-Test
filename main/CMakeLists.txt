include("${CMAKE_CURRENT_LIST_DIR}/../project/CMakeLists.txt")

set(srcs_dirs
    "."
    "application/"
    "application/logging/"
    "application/peripheral/"
    "application/peripheral/gpio"
    "application/peripheral/usart"
    "application/peripheral/memory"
    "application/memory/nvm/ota"
    "application/module"
    "application/module/simcom"
    "application/ota"
    "application/ota/sha256"
)

set(includes_dirs
    "."
    "application/"
    "application/logging/"
    "application/peripheral/"
    "application/peripheral/gpio"
    "application/peripheral/usart"
    "application/peripheral/memory"
    "application/memory/nvm/ota"
    "application/module"
    "application/module/simcom"
    "application/ota"
    "application/ota/sha256"
)

if(SALVUS_ESP32_FAMILY_MICROCONTROLLER_SELECTED)
    if(CONFIG_USE_Y70XX_SIMCOM_MODULE STREQUAL "y")
        set(SALVUS_Y70XX_SIMCOM_SELECTED ON CACHE BOOL "" FORCE)
    endif()

    list(APPEND srcs_dirs "application/logging/esp_log")
    list(APPEND includes_dirs "application/logging/esp_log")
    
    if(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
        message(STATUS "Microcontroller: ESP32S3")

        list(APPEND srcs_dirs "application/peripheral/gpio/esp32s3_gpio")
        list(APPEND srcs_dirs "application/peripheral/usart/esp32s3_usart")
        list(APPEND srcs_dirs "application/peripheral/memory/esp32s3_nvm")

        list(APPEND includes_dirs "application/peripheral/gpio/esp32s3_gpio")
        list(APPEND includes_dirs "application/peripheral/usart/esp32s3_usart")
        list(APPEND includes_dirs "application/peripheral/memory/esp32s3_nvm")
    endif()
endif()

if(SALVUS_Y70XX_SIMCOM_SELECTED)
    message(STATUS "Communication module selected: Y70XX")

    list(APPEND srcs_dirs "application/module/simcom/y70xx") 
    list(APPEND includes_dirs "application/module/simcom/y70xx") 
endif()

if(SALVUS_ESP32_FAMILY_MICROCONTROLLER_SELECTED)
    idf_component_register( SRC_DIRS        "${srcs_dirs}"
                            INCLUDE_DIRS    "${includes_dirs}"
                            PRIV_REQUIRES   "mbedtls" "esp_app_format" "app_update"
    )

    target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DSALVUS_ESP32_FAMILY_MICROCONTROLLER_SELECTED")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DSALVUS_ESP32S3_MICROCONTROLLER_SELECTED")

    if(SALVUS_Y70XX_SIMCOM_SELECTED)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DSALVUS_ENABLE_SIMCOM_MODULE_RESET_PIN")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DSALVUS_ENABLE_SIMCOM_MODULE_USART")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DSALVUS_Y70XX_SIMCOM_SELECTED")
    endif()

    return()
endif()
