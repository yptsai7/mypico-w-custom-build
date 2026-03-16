# 定義用戶 C 模組
add_library(usermod_ds4 INTERFACE)

# 指定源文件路徑
target_sources(usermod_ds4 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ds4.c
)

# 指定包含目錄
target_include_directories(usermod_ds4 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# 將模組連結到 MicroPython
target_link_libraries(usermod INTERFACE usermod_ds4)

# 連結 Pico SDK 的藍牙與多核支援庫
target_link_libraries(usermod_ds4 INTERFACE
    pico_btstack_classic
    pico_btstack_ble
    pico_multicore
)
