# 將 ds4.c 加入為用戶模組
micropython_add_user_cmodule(ds4 INTERFACE SOURCES ${CMAKE_CURRENT_LIST_DIR}/ds4.c)

# 連結 Pico SDK 的藍牙與多核支援
target_link_libraries(usermod INTERFACE 
    pico_btstack_classic 
    pico_btstack_ble 
    pico_btstack_cyw43
    pico_multicore
)

target_include_directories(usermod INTERFACE ${CMAKE_CURRENT_LIST_DIR})
