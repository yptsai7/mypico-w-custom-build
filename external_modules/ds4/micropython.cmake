micropython_add_user_cmodule(ds4 INTERFACE SOURCES ${CMAKE_CURRENT_LIST_DIR}/ds4.c)

# 關鍵：必須正確連結 btstack 相關的硬體抽象層
target_link_libraries(usermod INTERFACE 
    pico_btstack_classic 
    pico_btstack_ble 
    pico_btstack_cyw43
    pico_multicore
)

target_include_directories(usermod INTERFACE ${CMAKE_CURRENT_LIST_DIR})
