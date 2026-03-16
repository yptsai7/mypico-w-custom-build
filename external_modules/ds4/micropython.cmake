# 將當前目錄定義為名為 ds4 的用戶模組
micropython_add_user_cmodule(ds4 INTERFACE SOURCES ${CMAKE_CURRENT_LIST_DIR}/ds4.c)

# 連結 Pico SDK 的藍牙與多核庫
# 必須包含 pico_btstack_cyw43 以確保在 Pico W 上驅動無線晶片
target_link_libraries(usermod INTERFACE 
    pico_btstack_classic 
    pico_btstack_ble 
    pico_btstack_cyw43
    pico_multicore
)

target_include_directories(usermod INTERFACE ${CMAKE_CURRENT_LIST_DIR})
