# 进度记录（2026-11-15 夜）

目标：让 1943 / contra / xevious（sxevious 最重）在现有 RP2040 PicoBoy 上跑顺，先不换 RP2350。

---

## 一、已经做完的

### 1. 帧耗时打点（`GALAGINO_FRAME_TIMING`，默认 OFF）

`src/main.cpp` 里 `#ifdef GAL_FRAME_TIMING` 段，每 30 帧打一行：

```
[timing] n=30 frame=38013us sim=1 render=14590 push=22358 audio=12 wait=844 pr=1294 pw=1285 ring=576 fps=263.0
```

读数注意（两个已知缺陷，还没修）：

- `sim` 恒为 0~2 µs：`emulation_videoRendered()` 只是通知函数，真正 CPU 推进在 core1 的 `run_frame()` 里。**`render` 一列实际含「Z80 模拟 + 绘制」**。
- `fps` 小数点错位：`10000000UL/avg` 得到的是 fps×10。显示 `303.2` = 每秒 **30.3 次**刷屏 → **逻辑帧率 60.6 fps**。

### 2. 音频合成搬到 core1（`GALAGINO_AUDIO_CORE1`，默认 ON → `AUDIO_RENDER_ON_EMU_CORE`）

架构事实：core0 跑 video+audio 循环，core1 跑 emulation task（`run_frame()` 后空等帧信号）。

- `audio.h`：`RING_SAMPLES = AUDIO_CHUNK*16`（1024 样本 ≈42 ms）、`ringSamples[]`、`std::atomic ringHead/ringTail`、诊断计数 `chunksIn/chunksOut`、`ringFill()`。
- `audio.cpp`：`init()/stop()` 清零；`produce()` 由 core1 在 `run_frame()` 之后调用；`transmit()` 只从环里拷成品喂 PWM（原合成逻辑保留在 `#else`）。
- `emulation.cpp`：`run_frame()` 后 `audio.produce()`。

**实测收益（sxevious）**

| | 改前 | 改后（好状态） |
|---|---|---|
| audio | 87–105 ms | **1.1 ms**（core0 侧只剩拷贝+write，合成在 core1 并行） |
| frame | 130–140 ms | **33 ms** |
| 逻辑帧率 | ~25 fps | **60.6 fps** |
| 实时音频供给 | — | **24.0 k 样本/秒**（正常） |

### 3. 修掉的 bug（重要）

`produce()` 里空/满判断写反：

```cpp
// 错误（head==tail 时得 0，被当成"满"，一帧都没合成过 → 全程静音）
freeSlots = (tail + RING - head) % RING;
// 正确
used      = (head + RING - tail) % RING;
freeSlots = RING - used - 1;   // 留一格区分空/满
```

同时把 `produce()` 的单次上限从 8 块放开成「填满环」——8 块（512 样本）小于一帧需求（33–41 ms × 24 kHz ≈ 800–980 样本），会欠载、声音断续。

### 4. 工作台 `tools/pico_arcade_studio.py`

「② 固件编译」按钮改为「开始编译（编完自动合并）」，编译成功后自动 `step_merge()`，`dist/galagino_16m.uf2` 永远是含资产区的合并包。

### 5. 搞清楚了「每次烧完固件要重传 ROM」

- `build-diag/galagino_arcade.uf2`（纯固件）：写 `0x10000000 .. 0x10037400`，**不含资产区**。
- `dist/galagino_16m_*.uf2`（合并包）：写 `0x10000000 .. 0x1041FD7C`，含 `0x10120000` 起的 ROM。
- 固件里的资产指针是**编译期常量**（`GAL_ROM_BASE + 固定偏移`，由 `mkromregion.py extract` 生成），所以固件与板上 ROM 必须同源。**烧合并包就不需要再传 ROM**。

---

## 二、当前测试版

| 文件 | 说明 |
|---|---|
| `dist/galagino_16m_audio2core_fix2.uf2` | **最新**：环填满为止 + 诊断计数（`pr/pw/ring`） |
| `dist/galagino_16m_audio2core_fix.uf2` | 上一版：只修了空/满判断，单次仍限 8 块（会欠载） |
| `dist/galagino_16m.uf2` | 常规基线（无打点、无并行），回滚用 |

`fix2` **还没上机验收**。

---

## 三、明天第一件事

烧 `fix2`，听 sxevious，看 `[timing]` 三个新字段：

| 现象 | 判断 |
|---|---|
| `pr` 每行涨 370–460，`ring` 稳定 300–1000 | 正常，音频 24 kHz 实时 |
| `ring` 长期 0、`pr` 每行 < 350 | core1 供不上，声音断续 |
| `frame` 涨到 45–50 ms 以上 | core1 被合成占满，到 RP2040 天花板 |

要确认的两件事：**① 声音回来了吗 ② 有没有断续/爆音/音调偏低。**

如果音频过关，再进 1943 / contra 复测（预期同样落到 33–40 ms）。

---

## 四、下一步可选的优化（都还没动手）

### 已试过、结论明确

- **LCD 位时钟 62.5 → 83.3 MHz（失败）**：`-DPB_SPI_HZ_OVERRIDE=83333333` 编出 `dist/galagino_16m_push83.uf2`，实测画面出现**黑色干扰纹路**，且 `push` 反而从 22.3 ms 涨到 23.4–24.7 ms。原因：PIO 分频 = 250/(2×83.3) = 1.5，非整数导致 SCK 高低电平占空比不对称；SCK 周期只剩 12 ns，远低于 ST7789 的 66 ns 规格。**这条线到头了，不要再往上调**。SPI 已退回 62.5 MHz。

- **swap 批量 v1（失败）**：`memcpy(&w, strip + i, 4)` 两像素一次，实测 `push` 从 22.3 ms 涨到 **27.3–28.5 ms**。原因：`strip` 不是 4 字节对齐，`memcpy` 退化成逐字节复制，CPU 侧准备时间从 4.6 ms 涨到 9.6 ms。

- **swap 批量 v2（REV16）**：`rev16` 一条指令翻两个像素，16 位访问不要求对齐；对齐时走 32 位路径、不对齐时逐像素回退。`dist/galagino_16m_rev16.uf2`，反汇编已确认 `rev16` 在 `submit` 里。待实测。

1. **push 剩下的空间**：62.5 MHz 下理论传输 17.7 ms，实测 22.3 ms，差 4.6 ms = swap(≈1.4) + BSP 每行 `mk_pair`(≈0.8) + 288 次 `dma_channel_wait_for_finish_blocking`(≈2)。后两项都在 BSP 里（`E:\PicoBoy\bsp\src\pb_display.cpp` 的 `pb_display_push_rect` 逐行 push_words 且每行阻塞等待）。**想再压就得改 BSP**：把整条带（8 行）拼进一块缓冲后一次性 DMA（288 次等待 → 36 次）。但 galagino 侧 RAM 只剩 1.5 KB（Free=1592），加不了 3.8 KB 的条带缓冲 → 需要在 BSP 里加静态缓冲或用较小的分段（4 行）。
2. **异步双缓冲推送**：DMA 推屏与渲染重叠，frame 可降到 15 ms 量级；但屏幕刷新仍是 30 Hz（62.5 MHz SPI + 全屏 130 KB 的物理上限），要 60 Hz 得提 SPI 或只推变化区。
3. **音频降采样**（24 k → 16 k）：如果 core1 顶不住，CPU 需求直接砍三分之一，掌机喇叭上听感损失很小。
4. 修打点的两个缺陷：`sim` 挪到真正的模拟处、`fps` 小数点。

---

## 五、编译 / 出包命令

```powershell
$sdk='E:\Program Files\Raspberry Pi\Pico SDK v1.5.1'
$env:PICO_SDK_PATH="$sdk\pico-sdk"
$env:PATH="$sdk\cmake\bin;$sdk\ninja;$sdk\gcc-arm-none-eabi\bin;C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;"+$env:PATH
Set-Location 'C:\Users\bobo\Doubao\chats\2026-11-14\new-chat-2\galagino-pico'
& "$sdk\cmake\bin\cmake.exe" --build build-diag            # 现在 build-diag 处于 FRAME_TIMING=ON
& 'E:\Program Files\python313\python.exe' tools\mkromregion.py merge --fw build-diag\galagino_arcade.uf2 --out dist\galagino_16m_audio2core_fix2.uf2
```

- 讲量：`-j1`/`-j2`，`-j3+` 会 cc1 out of memory。
- 要出「无打点」的常规版：把 `GALAGINO_FRAME_TIMING` 改回 `OFF` 重编。
- 增量编译若提示 CMake 缓存变量不对，先 `cmake -S . -B build-diag -DGALAGINO_FRAME_TIMING=ON`。

---

## 六、本轮改过的文件

- `src/main.cpp` — `GAL_FRAME_TIMING` 段 + 诊断字段（`pr/pw/ring`）
- `src/emulation/audio.h` — 环形缓冲、`produce()`、诊断访问器
- `src/emulation/audio.cpp` — `produce()`（含空/满修复）、`transmit()` 消费环
- `src/emulation/emulation.cpp` — `run_frame()` 后调 `audio.produce()`
- `CMakeLists.txt` — `GALAGINO_FRAME_TIMING`、`GALAGINO_AUDIO_CORE1`
- `tools/pico_arcade_studio.py` — 编完自动合并
- `PROGRESS.md` — 本文件

---

## 七、push 侧：双缓冲异步推送（2026-11-16）

### 先纠正一个错误结论：RAM 不是瓶颈

早期据串口 `Heap: Free=1592 MaxAlloc=1592` 判定「RAM 只剩 1.5 KB，加不下条带缓冲」。
**这个读数是错的**：`hal::memInfo()` 取的是 newlib `mallinfo().fordblks`，只反映「已经向系统 sbrk 出来的那一小段里还剩多少空闲块」，随 malloc 需求继续 sbrk 扩展，不代表整片 SRAM 的余量。

用链接符号核对：

| 符号 | 地址 | 含义 |
|---|---|---|
| `__bss_end__` | `0x20005254` | 静态区结束（改动前 20,156 B） |
| `__StackLimit` | `0x20040000` | malloc（sbrk）可扩展到的上限 |
| 间隙 | 240,044 B | 全部可用 |

结论：条带级缓冲完全加得下。这条把「异步推送」从不可行变成可行。

### 实现

面板链路原本是：渲染 1 条带 → `pb_display_push_rect()` 阻塞等到 SPI 传完 → 渲染下一条。整屏 69,120 像素要扫两遍（galagino 侧换字节序一遍，BSP 侧 `mk_pair` 打包一遍），并且 288 次行间 `dma_channel_wait_for_finish_blocking` 全部串在关键路径上。

改成：**先攒够半个画面，再交给 DMA，边传边渲染下一半。**

- `src/hal/rp2040/video_picoboy.cpp`
  - `packPairs()`：`asm volatile("rev %0,%1")` 一条指令同时完成「两个像素换字节序」和「拼成面板要的 `(c0<<16)|c1`」。验证过：小端下 `REV` 正好等于 `mk_pair(swap(a), swap(b))`，所以整屏只扫一遍。
  - `static uint32_t s_blockBuf[2][17280]` —— 双缓冲，一块 = 半个画面（18 条带 × 8 行 × 240 列 / 2），合计 138,240 B。
  - `submit()` 只把条带打包进当前块，不再碰 SPI；块满或 `endFrame()` 时由 `pushBlock()` 交给 DMA。
  - `setViewport()` / `flip()` 前先 `pushBlock()` + `pb_display_push_flush()`，避免 MADCTL 命令与在飞的 DMA 抢 SPI。
  - 奇数宽度等病态调用保留原来的同步直推路径兜底。
- `src/main.cpp`：`endFrame()` 的耗时拆成两段计入——真正等 SPI 的算 `push`（`video.lastPushUs()`），补节拍睡到 16/33 ms 的算 `wait`。`push` 字段语义随之变化，读数时注意。
- BSP（`E:\PicoBoy\bsp`，纯增量，未改动任何现有函数）：
  - `pb_display_push_start(x,y,w,h)`：设窗口 + 0x2C + 拉片选，内部先等上一块传完。
  - `pb_display_push_words(w,nwords)`：启动 DMA 立即返回。
  - `pb_display_push_flush()`：等 DMA 传完并抬片选。

### 内存与体积

- 静态区 20,156 → 158,392 B；`__bss_end__ = 0x20026e60`，距 `__StackLimit` 仍余 **102,816 B**。
- 固件 226,412 → 227,268 B（bss 不进 flash，只有 +912 B 代码）。
- 产物 `dist/galagino_16m_async.uf2`（6,745,088 B）。

### 预期

SPI 传输 17.7 ms 与「渲染 + 打包」完全重叠 → 整屏 ~17.7 ms 起，远低于 33 ms 的 60 fps 预算；`endFrame` 的节拍睡眠会把帧率锁回 60 fps，不再靠降低帧率换时间。

### 验证要点（真机）

- `push` 应该很小（打包 + 等上一块剩余的传输），`wait` 变大（补节拍）——这是预期的语义变化，不是变慢。
- `frame` 应稳定在 33 ms 附近（`fps` 读数 ~333 即逻辑 60 fps）。
- 画面不能有撕裂：块与块之间是不同半屏的不同窗口，若出现上下半屏错位/滚动，说明 `block_row` 算错。
### 实测（sxevious / contra，打点版）

| 机型 | frame | render 峰值 | push | wait | 逻辑帧率 |
|---|---|---|---|---|---|
| sxevious | 32.97–33.07 ms | 17.3 ms | 1.78–1.95 ms | 12.5–22.2 | 60.6 fps |
| contra | 32.96–33.11 ms | 21.5 ms | 2.34–2.55 ms | 5.7–22.3 | 60 fps |

两款均被节拍锁死在 33 ms，`wait` 里是真·空余（sxevious 剩 ~13 ms）。**SPI 传输已完全藏进渲染时间**。

### 事故：切 1943 时 PANIC "Out of memory"

根因：静态区不是独立的，pico 的 `_sbrk` 用 `__StackLimit (0x20040000)` 作上限、`end (__bss_end__)` 作起点，所以**静态区每多 1 字节，malloc 就少 1 字节**。138 KB 的双缓冲把可用堆从 240,044 B 压到 102,816 B，1943 的堆需求（`memory` 16,384 + `frame_buffer` 3,840 + `sprite_buffer` + Arena 69,632 + 资源加载）撞上上限。

早期"Free=1592 所以 RAM 只剩 1.5 KB"的推论也是被同一件事误导：`Free` 是 newlib arena 的空闲块，而 `PICO_USE_OPTIMISTIC_SBRK` 会一次把剩余全给 newlib，读数跟着可用量走，不代表真实需求。

修复：块粒度从 18 条带（半个画面）降到 **2 条带（16 行）**。
- 静态区 20,156 → 35,512 B，`__bss_end__ = 0x20008e60`，可用堆 **236,192 B**（比改造前只少 3.8 KB）。
- 16 行一块的传输约 0.98 ms、渲染约 0.93 ms，仍然互相覆盖，整屏 17.7 ms 的 SPI 时间照样被藏住。
- 产物 `dist/galagino_16m_async2.uf2`（6,745,088 B）。

另外把 `switchTo()` 的诊断行加了 `avail=`（`__StackLimit - end`），以后判断"RAM 紧不紧"要同时看 `Free` 和 `avail`：`avail` 才是 malloc 的总余量。
### 三款全部达标（2 条带/块版，打点版读数）

| 机型 | frame | render 峰值 | push | wait | 逻辑帧率 | avail |
|---|---|---|---|---|---|---|
| sxevious | 33.0 ms | 17.3 ms | 1.8 ms | 12.5–22.2 | 60.6 | — |
| contra | 33.0 ms | 21.5 ms | 2.4 ms | 5.7–22.3 | 60 | — |
| 1943 | 33.0 ms | 20.6 ms | 4.7–9.3 ms | 6.0–15.1 | 60.6 | 225,696 |

1943 的 `Free=7256` 是三款里最小的，印证它堆需求最大；`avail=225696` 说明还有 220 KB 总量可伸。

`push` 从 1.8（18 条带版）升到 4.7 ms，是块变细后每屏 18 次 DMA 启动各等前一尾巴的累积损耗，仍在 33 ms 预算内无感。

正式版（关打点）：`dist/galagino_16m_async2_release.uf2`（6,744,064 B）。
## 八、正式版双形态：有声 / 静音极速（2026-11-16）

### 开关

新增 CMake option `GALAGINO_AUDIO`（默认 `ON`），关闭时定义 `GAL_AUDIO_OFF`。
它与既有的 `GALAGINO_FRAME_TIMING`（默认 OFF）、`GALAGINO_AUDIO_CORE1`（默认 ON）是三个独立维度，可任意组合。

`GAL_AUDIO_OFF` 时 `audio.cpp` 里四个入口各自早返回：
- `init()` 不配置 PWM/DMA —— 所以每 chunk 一次的 DMA 完成中断（24 kHz / 64 样本 ≈ 375 次/秒）不会来
- `produce()` 不调用 `renderAudio()`，FM/AY 合成完全不跑
- `transmit()` 不往 sink 写样本
- `stop()` 不做 flush

声音芯片的寄存器写入仍发生在 `run_frame()` 内（那是模拟的一部分），因此关音频**不影响游戏逻辑**，只是不出声。

### 体积与内存

| | 有声版 | 静音版 |
|---|---|---|
| text | 226,684 B | 224,660 B |
| bss | 35,484 B | 30,072 B |
| 固件 | 886 块 | 878 块 |
| UF2 | 6,744,064 B | 6,739,968 B |

静态区少 5,412 B 来自环缓冲、pending、DMA 双缓冲整块不再分配。

### 结论：静音版在当前节点不会更快

三款已经被节拍锁在 33 ms，省下的约 1.5 ms 只会变成 `wait` 里的空睡，帧率不变（已是 60 fps 上限）。
它的真实价值在三处：**发热/功耗**、**给更重机型留余量**（1943 的 render 峰值 20.6 → 约 19 ms）、**为将来超频或扩 ROM 白拿 CPU 时间**。

### 构建命令（在设好 PICO_SDK_PATH 与 PATH 后）

```
cmake -B build-diag -DGALAGINO_FRAME_TIMING=OFF -DGALAGINO_AUDIO=ON      ; 有声版
cmake -B build-diag -DGALAGINO_FRAME_TIMING=OFF -DGALAGINO_AUDIO=OFF     ; 静音版
cmake --build build-diag
python tools\mkromregion.py merge --fw build-diag\galagino_arcade.uf2 --out dist\<name>.uf2
```
注意两次切换会全量重编（宏变了），须 `-j1`/`-j2`。

### 产物清单（dist）

| 文件 | 说明 |
|---|---|
| `galagino_16m_async2_audio.uf2` | **有声正式版（当前推荐）** |
| `galagino_16m_async2_silent.uf2` | 静音极速版 |
| `galagino_16m_async2.uf2` | 打点版（有声 + 帧耗时打印），调优时用 |
| `galagino_16m_async2_release.uf2` | 打点关闭、音频开启（与 audio 版等价，保留作存档） |
| `galagino_16m_async.uf2` | 18 条带/块版，**1943 会 panic，勿用** |
| `galagino_16m_rev16.uf2` | 异步前的最后同步版，push 21.5–22.2 ms，回滚点 |
| `galagino_16m_audio2core_fix2.uf2` | 音频搬核 + 静音 bug 修复完成时的版本 |
| `galagino_16m_push83.uf2` / `galagino_16m_pushswap.uf2` | 失败实验（83 MHz 黑纹 / memcpy 换序变慢） |
| `galagino_16m.uf2` | 常规基线 |

### 下一步可做的（按价值排序）

1. ~~同步仓库~~ → **已完成**，见第九节。
2. ~~提交 BSP~~ → **已完成**，见第九节。
3. **块粒度微调（可选）**：若嫌 `push` 4.7 ms 偏高，试 3–4 条带/块（静态 23–31 KB，`avail` 余量足够），预计回到 3 ms 左右；不建议，4.7 ms 在 33 ms 预算内无感。
4. **少量遗留**：`fps` 打印是 fps×10（`10000000UL/avg`），`sim` 打点位置错（恒 0~2 µs，真模拟计入 `render`）。都只影响读数观感，不影响性能。

### 别再踩的坑

- **不要靠静态数组换性能而不看堆余量**：`_sbrk` 上限是 `__StackLimit`、起点是 `__bss_end__`，静态与 malloc 共用一片 RAM。
- **不要用 `Heap: Free` 判断 RAM 松紧**：`PICO_USE_OPTIMISTIC_SBRK` 会把剩余一次全给 newlib，读 `avail=`。
- **不要往上调 LCD SPI 频率**：83.3 MHz 出黑色干扰纹且 push 反升。
- **不要用 `memcpy` 做批量换序**：strip 无 4 字节对齐保证，会退化成逐字节。用 `rev16` / `rev`。

## 九、2026-11-16 状态：仓库同步完成，转向 PicoBoy 的 GBC / NES

### 三个仓库的提交（本轮全部落地）

| 仓库 | 提交 | 内容 |
|---|---|---|
| `sunnybona/galagino-pico`（公开） | `66f5df7` → **`184a984`** | `3d2f59b` 11 个文件：`video_picoboy.cpp/.h`（双缓冲异步）、`audio.cpp/.h` + `emulation.cpp`（core1 + 环 + 静音 bug）、`main.cpp`（push/wait 拆分、`avail=`）、`CMakeLists.txt`（`GALAGINO_AUDIO`）、两个 tools、新增 `tools/load.bat`、README；`184a984` 只补 README 的 BSP 依赖提示 |
| `sunnybona/galagino-pico-assets`（私有） | `ef35e99` → **`904be5c`** | 14 项 / 47.26 MB：新增 `galagino_16m_async2_audio.uf2`（6,744,064 B）与 `_silent.uf2`（6,739,968 B），基线 uf2 刷新，`rom/romregion.bin` 3,145,084 B，`rom/plain_src` 12 款 |
| `E:\PicoBoy`（本地，git 根 `E:/PicoBoy`） | `0166dcc` → **`e7181a5`** | `dc6574f` BSP 三函数（纯增量，另把被隔开的 MADCTL 注释移回函数上方）；`e7181a5` `PICOBOY_FRAME_TIMING` option + gbc/nes 打点 + `.gitignore` 加 `build-*/` |

验证：`E:\PicoBoy` 增量编译四步全过（`pb_display.cpp` → `libpicoboy.a` → `picoboy_gbc.elf` / `pico_nes.elf`），`git status --short` 干净。`E:\PicoBoy` **没有配 remote**，目前只是本地记录。

`README.md` 新增第八节记录四步改造表、三款实测表、两条 RAM 约束、双形态开关与体积、两项做不通的、开关一览；第一节 BSP 接口从 2 个扩到 5 个并标注旧版 BSP 会链接失败。

### PicoBoy GBC / NES 支线：先量化，不盲改

勘察 `E:\PicoBoy` 后的关键结论——**这轮 galagino 的四步，PicoBoy 已经用掉大部分**：

| 做法 | PicoBoy 现状 |
|---|---|
| 双缓冲异步推送 | **已有**。`pb_display.cpp` 第 396 行起 GB「半幅乒乓 + core1 推带」（`GB_BAND_ROWS`=108、`GB_BAND_SLOTS`=2，带池复用 `pb_fb` 103,680 B）；第 510 行起 NES（`NES_BAND_SLOTS`=4、`NES_BAND_ROWS`=60，带池 122,880 B）。`pb_display_start_pusher()` 起 core1 worker |
| 整带一次 DMA | **已有**。`push_words()` 一次传完整带（`rows * words_per_row`），不是 galagino 那种逐行 288 次阻塞 |
| rev16 批量换序 | **不需要**。`pb_push_gb_line` / `pb_push_rgb_line` 在采样时就 `dst[x/2] = mk_pair(...)` 拼好，本来没有两遍扫描 |
| 音频搬核 | **没做**，唯一真正可搬的。`gbc/main.cpp` 在 core0 跑 `minigb_apu_audio_callback` + 立体声混单声道；`nes/main.cpp` 的 APU 混音在 `InfoNES_RunFrame()` 内部 |

所以只加了打点，先量清楚再决定动哪里：

```
cmake -S . -B build-t -G Ninja -DPICOBOY_FRAME_TIMING=ON
cmake --build build-t -j2
```

- `gbc/main.cpp` 五段：`emu`（模拟 + 填带）、`apu`、`mix`（立体声混单声道）、`sub`（喂 PWM）、`wait`（补 59.7275 fps 节拍）；每 60 帧一行 `[timing] n=… frame=…us emu=… apu=… mix=… sub=… wait=… fps=…`
- `nes/main.cpp` 两段：`emu`（`InfoNES_RunFrame` 全长）、`wait`
- 产物 `build-t\picoboy_gbc.uf2` 189,440 B / `build-t\pico_nes.uf2` 412,672 B，**已交付等真机日志**。两个 uf2 都是纯固件（PicoBoy 运行时从 flash ROM 区读 ROM），烧了不擦 ROM。
- 体积：`picoboy_gbc.elf` text 94,680 / bss 218,012；`pico_nes.elf` text 206,232 / bss 241,844。

### 下一步

1. **等 GBC / NES 的 `[timing]` 日志**。判据：若 GBC 的 `apu` ≥ 1.5 ms 且 `wait` 已经很小，才值得把 APU 搬 core1；NES 同理看 `emu` 离 16,650 µs 还有多少。
2. galagino 侧可选收尾：块粒度 3–4 条带/块（push 4.7 → 约 3 ms，不建议，33 ms 预算内无感）；`fps` 是 fps×10 的读数瑕疵、`sim` 打点位置错（只影响观感）。
3. `E:\PicoBoy` 若要备份，需先 `git remote add`。

### 新增的坑

- **PicoBoy 首次 `cmake -B build` 会落到 NMake 生成器**且配置 incomplete（`Generator: execution of make failed`），必须显式 `-G Ninja`。
- `gbc/main.cpp` 里 `printf` 必须自带 `#include <stdio.h>`（原文件只引 `pico/stdlib.h`），`time_us_64` 要 `#include "pico/time.h"`，两个都放在 `#ifdef PB_FRAME_TIMING` 内，否则默认构建会多引头文件。
## 2026-11-17 定型：galagino_16m_mixed71d.uf2

### 定型结论

- **音频失真根因已定位并修复**：BSP `pb_audio_submit` 在环形缓冲满时静默丢弃超出样本（`if(room<=0) return;` + `n = min(count, room)`）。三层缓冲叠加（上游 Audio 环 42.7 ms + `LEAD` 超前 10.7 ms + BSP 环 33 ms）一旦帧节奏稍快就溢出，周期性波形断裂 = 破音、听不出旋律。
- **修法**：新增 `pb_audio_free()` 供上游查询剩余空间；`audio_pwm.cpp` 的 `AudioOut::write()` 在重采样前先查，装不下就整块退回。`Audio::transmit()` 不前进 `ringTail`，样本留在上游环绕下次再送，**全链路零丢弃**。
- 用户真机验收：`mixed62b`（62.5 + 全半速 + 音频修复）→"声音已经基本正常"，确认音频问题闭环。
- **定型版** `mixed71d`：71.4 MHz SPI + 1943/xevious/contra/commando 半速 + 其余 8 款全幅 60 Hz + 音频不丢样本。用户判定"基本流畅，屏幕上有干扰纹路"，接受为定型版本。

### 本轮代码改动

| 文件 | 改动 |
|---|---|
| `E:\PicoBoy\bsp\include\picoboy\pb_audio.h` | 新增 `size_t pb_audio_free(void)` |
| `E:\PicoBoy\bsp\src\pb_audio.cpp` | 实现 `pb_audio_free()`，按 ring-wrap 版用 `read_position()` 算积压，欠载时放行让 submit 自行 resync |
| `src\hal\rp2040\audio_pwm.cpp` | `write()` 增加 `needOut` + `pb_audio_free()` 空间检查，装不下整块退回 |
| `E:\PicoBoy\bsp\include\picoboy\pb_display.h` | 重新声明 `pb_display_push_start/push_words/push_flush` |
| `E:\PicoBoy\bsp\src\pb_display.cpp` | 补回这三个异步推屏函数（新 BSP 只保留阻塞 `pb_display_push_rect`） |

### 重要环境变化：BSP 换了版本

`E:\PicoBoy` 现为 HEAD `39c0c1c`，内容已不是此前记录的 `e7181a5`：

- 新增 PIO I2S 音频后端（`PB_AUDIO_I2S=1` 时走外部 DAC，默认 0=PWM）
- 音频驱动改成连续 ring-mode PWM DMA（无限传输计数 + 地址折回 + 读指针跟踪 + 欠载静音 resync），不再用 DMA IRQ ISR 续流
- 新增 boot menu（GBC/NES 合盘，共享 ROM 区 0x1012000）
- **移除了异步推屏三函数**，只剩阻塞版 `pb_display_push_rect`，galagino 的双缓冲推屏因此编译不过，已按原语义补回。新 BSP 自带的 core1 pusher 未被触发，不冲突。

### 固件对照

| 文件 | 说明 |
|---|---|
| `dist\galagino_16m_mixed71d.uf2` | **定型版**，71.4 MHz + 按机型半速 + 音频不丢样本，text 227,604 B |
| `dist\galagino_16m_mixed62c.uf2` | 对照版，仅 SPI 降回 62.5，其余与 mixed71d 相同，text 227,612 B |
| `dist\galagino_16m_mixed62b.uf2` | 音频修复验收版（旧 BSP），用户验收"声音基本正常" |

### 待办

1. **干扰纹路归因**：mixed71d（71.4）与 mixed62c（62.5）对照。若 62.5 无纹路，即确认纹路来自 71.4 MHz 的 SPI 时钟边限（面板规格上界通常 62.5，部分模组 80）。
2. **重采样去除法**：`audio_pwm.cpp` 内循环 `delta * j / m` 每个输出样本一次 32 位软件除法（RP2040 无硬件除法），改成定点累加可回收一截 core0 时间，用于抵消"不再丢样本"带来的额外搬运开销。
3. 两仓 git 未同步（公开仓缺 FULL_RATE/半速分流/commando 投币位/parts 打点/音频修复；BSP 本轮改动未提交）。