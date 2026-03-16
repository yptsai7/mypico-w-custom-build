target_sources(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ds4.c
)

target_include_directories(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# 只連結 BTstack Classic BT 相關
# 不連結 pico_cyw43_arch_threadsafe_background（MicroPython 主程式已處理）
target_link_libraries(usermod INTERFACE
    pico_btstack_classic
    pico_btstack_hid
    pico_multicore
)
