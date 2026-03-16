# 使用最新的 C 模組添加方法
micropython_add_user_cmodule(ds4 INTERFACE SOURCES ${CMAKE_CURRENT_LIST_DIR}/ds4.c)

# 連結 Pico SDK 的藍牙經典與多核支援
target_link_libraries(usermod INTERFACE 
    pico_btstack_classic 
    pico_btstack_ble 
    pico_multicore
)
