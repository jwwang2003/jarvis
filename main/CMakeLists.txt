idf_component_register(
    SRCS
        "jarvis_main.cpp"
        "motor_controller.cpp"
        "ble_service.cpp"
    PRIV_REQUIRES
        spi_flash
    REQUIRES
        bt
        esp_timer

        # Components (components/**)
        # BLE library
        esp-nimble-cpp
    INCLUDE_DIRS
        # Frontend output (../web/esp32svelteesp32.h)
        "./include/"
)
