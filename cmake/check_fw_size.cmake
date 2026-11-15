# 检查固件体积没有越进资产区（GAL_ROM_BASE = 0x10120000，即 0x120000 偏移）
if(NOT EXISTS "${MAP}")
  message(FATAL_ERROR "固件 bin 不存在: ${MAP}")
endif()

file(SIZE "${MAP}" fw_size)
if(fw_size GREATER MAX)
  message(FATAL_ERROR "固件 ${fw_size} B 超过 ${MAX} B，会覆盖 0x10120000 的资产区")
endif()
message(STATUS "固件体积 ${fw_size} B / 上限 ${MAX} B")
