# 使用 usermod 目標，這會由 MicroPython 的主要建構腳本自動處理
target_sources(usermod INTERFACE ${CMAKE_CURRENT_LIST_DIR}/ds4.c)
target_include_directories(usermod INTERFACE ${CMAKE_CURRENT_LIST_DIR})
