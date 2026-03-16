# 將當前目錄定義為名為 ds4 的用戶模組
micropython_add_user_cmodule(ds4 INTERFACE SOURCES ${CMAKE_CURRENT_LIST_DIR}/ds4.c)

# 連結 Pico SDK 的藍牙與多核庫到 usermod 全域目標
target_link_libraries(usermod INTERFACE 
    pico_btstack_classic 
    pico_btstack_ble 
    pico_multicore
)

# 確保包含路徑正確
target_include_directories(usermod INTERFACE ${CMAKE_CURRENT_LIST_DIR})
