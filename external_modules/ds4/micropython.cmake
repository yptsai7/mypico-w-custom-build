target_sources(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ds4.c
)

target_include_directories(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# pico_btstack_hid 不存在，HID 已包含在 pico_btstack_classic 裡
target_link_libraries(usermod INTERFACE
    pico_btstack_classic
    pico_multicore
)
