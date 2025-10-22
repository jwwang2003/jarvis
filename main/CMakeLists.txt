idf_component_register(
    SRCS
        "jarvis_main.cpp"
        "services/wifi/wifi.cc"
        "services/web/http_server.cc"
    PRIV_REQUIRES
        spi_flash
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
    REQUIRES
        # Components (components/**)
    INCLUDE_DIRS
        # Frontend output (../web/esp32svelteesp32.h)
        "./include"
        "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>
)
