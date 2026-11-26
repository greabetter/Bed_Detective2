# 病床监测系统（Bed_Detective）

基于 **STM32F407VET6 + FreeRTOS + LVGL 9.5** 的医用监护终端原型，目标是把传感器采集、告警判断和本地图形界面整合到一块 MCU 上。

当前处于**界面与任务框架搭建阶段**：FreeRTOS 三任务已跑通，LVGL 已在 2.4 寸 SPI 屏上点亮并可响应按键事件。

---

## 硬件

| 项目 | 型号 / 参数 |
|---|---|
| 主控 | STM32F407VET6（Cortex-M4F，168 MHz，512 KB Flash / 192 KB RAM） |
| 显示屏 | 2.4" TFT，240×320，ILI9341，SPI 接口 |
| 接线 | SCK=PA5，MOSI=PA7，CS=PA4，DC=PA6，RST=PC4 |
| 按键 | Key_1（上拉输入，低电平有效） |

---

## 软件架构

### 任务划分

| 任务 | 优先级 | 栈 | 职责 |
|---|---|---|---|
| `KEY_Task` | Low1 | 512 B | 30 ms 周期扫描按键，检测到按下后释放二值信号量 |
| `lv_Timerhandler_Task` | Low | 4 KB | LVGL 主循环：取信号量处理事件、`lv_timer_handler()`、5 ms 轮询 |
| `StartDefaultTask` | Normal | 512 B | CubeMX 默认任务，预留 |

```
KEY_Task ──(osSemaphoreRelease)──▶ [二值信号量] ──(osSemaphoreAcquire)──▶ lv_Timerhandler_Task
                                                                              │
                                                                     lv_timer_handler()
                                                                              │
                                                                        disp_flush()
                                                                              │
                                                                     SPI1 ─▶ ILI9341
```

### 关键设计

- **LVGL 只在 `lv_Timerhandler_Task` 中调用**，其他任务通过信号量通知，避免跨线程操作对象树
- **显示缓冲区**：240×40×2 = 19.2 KB，`LV_DISPLAY_RENDER_MODE_PARTIAL` 局部刷新
- **tick 来源**：`lv_tick_set_cb(HAL_GetTick)`，不额外占用定时器
- **`disp_flush`**：设置窗口 → 拉低 CS → SPI 发送 → `lv_display_flush_ready()`

---

## 目录结构

```
Bed_Detective/
├── Core/             CubeMX 生成的初始化与主函数
├── USER/
│   ├── LCD.c/.h      ILI9341 SPI 驱动（初始化序列、窗口设置、填充）
│   ├── lv_port.c/.h  LVGL 移植层（display、flush、tick）
│   ├── KEYs/         按键扫描（状态机，下降沿检测）
│   └── User_Task/    用户界面代码（按钮、标签、事件回调）
├── Middlewares/      FreeRTOS 与 CMSIS-RTOS2
├── Drivers/          HAL 与 CMSIS
└── lvgl/             LVGL 9.5（第三方，未纳入仓库）
```

---

## 构建

1. 下载 [LVGL 9.5.0](https://github.com/lvgl/lvgl/releases)，解压后把 `lvgl` 目录放到工程根目录
2. 删除 `lvgl/src/drivers`（PC 平台驱动，MCU 用不到）
3. 用 STM32CubeIDE 打开工程，`Project → Build`
4. ST-Link 下载

> 仓库不包含 `lvgl/` 目录，因为它是第三方库（400+ 文件、20 MB），会淹没本项目的代码。

---

## 踩过的坑

记录在这里，因为每个都花了不短时间才定位：

### 1. LVGL 任务栈溢出导致白屏

`lv_Timerthandler_Task` 初始栈为 `256 * 4 = 1 KB`，LVGL 渲染时调用链过深，直接栈溢出进 HardFault。
**现象**：程序看起来在运行，LED 正常，但屏幕永远停在初始化后的白屏。
**修复**：栈改为 `1024 * 4`（4 KB）。

### 2. 二值信号量初值为 1，上电自动触发一次

`osSemaphoreNew(1, 1, ...)` 的第二个参数是初始计数，设成 1 意味着"资源一开始就可用"，第一次 `osSemaphoreAcquire` 立即成功，回调在启动瞬间被执行。
**修复**：初始计数改为 0，只有真正 `Release` 才生效。

### 3. 按键扫描状态机写错

```c
HAL_GPIO_ReadPin(...);          /* 读了但没存 → Current 永远是初值 */
if (Previous == SET && Current == RESET) return 1;
Current = Previous;             /* 方向反了，应该 Previous = Current */
```
**现象**：按键完全无反应。
**修复**：读值赋给 `Current`，判断后执行 `Previous = Current`。

### 4. LVGL 的 `lv_conf.h` 模板默认是关闭的

模板文件顶部是 `#if 0`，只改名不改成 `#if 1`，所有配置项都不生效，导致 `LV_USE_LIBPNG` 等默认开启，去 include 已被删除的目录。
**修复**：改成 `#if 1`，并同步关闭未使用的解码器宏。

### 5. CubeIDE 增量构建的假错误

修改 `.cproject` 后不重新生成 makefile，会出现"上次的错误还在，但编译其实成功"的假象。
**修复**：`Project → Clean` 后重新 Build；或删除 `Debug/` 目录。

---

## 后续计划

- [ ] 接入传感器（体温、心率、体位），走 I2C
- [ ] 告警阈值判断与状态机
- [ ] SPI 刷屏改 DMA + 双缓冲，降低 CPU 占用
- [ ] 增加 CAN 上报，对接护士站
- [ ] 界面多页面切换（`lv_screen_load_anim`）

---

## 许可

仅用于学习与毕业设计，第三方库遵循各自许可（LVGL MIT、ST HAL BSD-3-Clause、FreeRTOS MIT）。