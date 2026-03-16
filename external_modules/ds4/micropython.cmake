# 將 ds4 資料夾宣告為一個 MicroPython 用戶模組
micropython_add_user_cmodule(ds4 INTERFACE SOURCES ${CMAKE_CURRENT_LIST_DIR}/ds4.c)

# 連結 Pico SDK 的藍牙與多核支援至全域模組目標
target_link_libraries(usermod INTERFACE 
    pico_btstack_classic 
    pico_btstack_ble 
    pico_multicore
)

# 確保編譯器能找到當前目錄下的標頭檔
target_include_directories(usermod INTERFACE ${CMAKE_CURRENT_LIST_DIR})
