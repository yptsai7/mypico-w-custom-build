target_sources(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ds4.c
)

target_include_directories(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# 覆蓋 btstack_config.h 的搜尋路徑（優先使用我們的設定）
target_compile_options(usermod INTERFACE
    -include ${CMAKE_CURRENT_LIST_DIR}/btstack_config.h
)

# 連結 BTstack 和 Pico W 必要函式庫
target_link_libraries(usermod INTERFACE
    pico_btstack_classic
    pico_btstack_hid
    pico_btstack_ble
    pico_cyw43_arch_threadsafe_background
    pico_multicore
)

