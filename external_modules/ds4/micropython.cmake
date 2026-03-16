# 定義模組名稱為 ds4
micropython_add_user_cmodule(ds4 INTERFACE SOURCES ${CMAKE_CURRENT_LIST_DIR}/ds4.c)

# 連結必要的藍牙與多核庫
target_link_libraries(usermod INTERFACE 
    pico_btstack_classic 
    pico_btstack_ble 
    pico_multicore
)
