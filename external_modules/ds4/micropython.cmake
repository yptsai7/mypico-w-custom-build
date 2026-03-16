target_sources(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ds4.c
)

target_include_directories(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# 連結 BTstack 和 Pico W 必要函式庫
target_link_libraries(usermod INTERFACE
    pico_btstack_classic
    pico_btstack_hid
    pico_btstack_ble
    pico_cyw43_arch_threadsafe_background
    pico_multicore
)
