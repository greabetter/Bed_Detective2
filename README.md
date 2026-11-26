# 病床体征监测终端（Bed_Detective）

基于 **STM32F407VET6 + FreeRTOS + LVGL 9.5** 的体征监测终端原型：本地图形界面显示血氧、心率、呼吸、体温，超阈值时给出三级视觉报警，并把采集与显示拆成独立任务。

**当前进度**：界面框架、任务调度、RTC 时间均已跑通；传感器模块待接入（界面数据暂用模拟值）。

---

## 硬件

| 项目 | 型号 / 参数 |
|---|---|
| 主控 | STM32F407VET6（Cortex-M4F，168 MHz，512 KB Flash / 192 KB RAM） |
| 显示屏 | 2.4" TFT，240×320，ILI9341，SPI 接口 |
| 接线 | SCK=PA5，MOSI=PA7，CS=PA4，DC=PA6，RST=PC4 |
| 按键 | Key_1（上拉输入，低电平有效） |
| 时钟源 | LSE 32.768 kHz 晶振（RTC） |

---

## 软件架构

### 任务划分

| 任务 | 优先级 | 栈 | 职责 |
|---|---|---|---|
| `KEY_Task` | Low1 | 512 B | 30 ms 周期扫描按键，检测到按下后释放二值信号量 |
| `lv_Timerhandler_Task` | Low | 4 KB | LVGL 主循环：处理事件、`lv_timer_handler()`、5 ms 轮询 |
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

- **LVGL 只在 `lv_Timerhandler_Task` 内调用**，其他任务通过信号量传递事件，避免跨任务操作对象树
- **显示缓冲区** 240×40×2 = 19.2 KB，`LV_DISPLAY_RENDER_MODE_PARTIAL` 局部刷新
- **tick 来源** `lv_tick_set_cb(HAL_GetTick)`，不额外占用定时器
- **`disp_flush`** 设置窗口 → 拉低 CS → SPI 发送 → `lv_display_flush_ready()`

---

## 界面设计

240×320 单页布局，只放临床上最关键的三项体征，避免信息过载：

```
┌────────────────────────┐
│ ● NORMAL      12:30    │  ← 状态条：绿/黄/红
├────────────────────────┤
│   SpO2       98   %    │  ← 28 号字
│   HR         76   bpm  │
├────────────────────────┤
│   RR         16   /min │
│   TEMP       36.5 C    │
├────────────────────────┤
│  [ DETAIL ] [ TREND ]  │
└────────────────────────┘
```

**设计要点**

- **颜色编码**：绿 `0x00C853` / 黄 `0xFFB300` / 红 `0xD50000`，符合医疗报警三色约定
- **数值优先**：主数值 28 号字，标签与单位 14 号字
- **纯 ASCII**：界面文案用 `SpO2 / HR / RR / TEMP`，避免中文字体占用 Flash（完整中文字库数 MB，子集字体需额外工具生成）
- **占位提示**：体温标注为体表温度，不与核心体温混用

---

## 时间

- RTC 时钟源 **LSE（32.768 kHz）**，分频 127/255 得到 1 Hz
- CubeMX 生成的初始化会**无条件重设时间**，已用备份寄存器规避：

```c
/* 已初始化过则保留当前时间 */
if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0x32F2) {
    return;
}
/* ...设置初始时间后... */
HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);
```

- 按 RST 不会重置时间（备份域不断电）；完全断电且无纽扣电池才会回到初始值
- 调试需要改时间时，可按住 Key_1 上电强制重设

> 注意：`HAL_RTC_GetTime()` 之后**必须**再调一次 `HAL_RTC_GetDate()`，否则影子寄存器不更新，读到的永远是旧值。

---

## 目录结构

```
Bed_Detective/
├── Core/             CubeMX 生成的初始化与主函数（含 RTC）
├── USER/
│   ├── LCD.c/.h      ILI9341 SPI 驱动（初始化序列、窗口设置、填充）
│   ├── lv_port.c/.h  LVGL 移植层（display、flush、tick）
│   ├── KEYs/         按键扫描（状态机，下降沿检测）
│   ├── ui/           界面：布局、状态条、数值更新、页面切换
│   └── User_Task/    用户任务与事件回调
├── Middlewares/      FreeRTOS 与 CMSIS-RTOS2
├── Drivers/          HAL 与 CMSIS
└── lvgl/             LVGL 9.5（第三方，未纳入仓库）
```

---

## 构建

1. 下载 [LVGL 9.5.0](https://github.com/lvgl/lvgl/releases)，把 `lvgl` 目录放到工程根目录
2. 删除 `lvgl/src/drivers`（PC 平台驱动，MCU 用不到）
3. `lv_conf.h` 顶部 `#if 0` 改为 `#if 1`，并关闭未使用的解码器
4. 用 STM32CubeIDE 打开工程，`Project → Build`
5. ST-Link 下载

> 仓库不含 `lvgl/` 目录：它是第三方库（400+ 文件、20 MB），会淹没本项目的代码。

---

## 踩过的坑

每个都花了不短时间才定位，记录下来：

### 1. LVGL 任务栈溢出导致白屏

初始栈 `256 * 4 = 1 KB`，LVGL 渲染调用链过深直接溢出进 HardFault。
**现象**：程序在跑、LED 正常，但屏幕永远停在初始化后的状态。
**修复**：栈改为 `1024 * 4`（4 KB）。可用 `uxTaskGetStackHighWaterMark()` 定量确认余量。

### 2. 二值信号量初值为 1，上电自动触发

`osSemaphoreNew(1, 1, ...)` 第二个参数是初始计数，为 1 表示"资源一开始就可用"，第一次 `Acquire` 立即成功。
**修复**：初始计数改为 0，只有真正 `Release` 才生效。

### 3. 按键扫描状态机写错

```c
HAL_GPIO_ReadPin(...);                              /* 读了但没存，Current 永远是初值 */
if (Previous == SET && Current == RESET) return 1;
Current = Previous;                                  /* 方向反了 */
```
**修复**：读值赋给 `Current`，判断后执行 `Previous = Current`，并补上 `return`。

### 4. 屏幕整体反色

按钮蓝色显示成黄色、红色区域发青 —— 面板处于反显状态。
**修复**：初始化序列末尾发送 `0x21`（INVON）。不同批次模块可能相反，`0x20` / `0x21` 互换即可。

### 5. LVGL 的 `lv_conf.h` 模板默认关闭

模板顶部是 `#if 0`，只改文件名不改这行，所有配置都不生效，导致 `LV_USE_LIBPNG` 等默认开启、去 include 已删除的目录。
**修复**：改成 `#if 1`，同步关闭未使用的解码器宏。

### 6. CubeIDE 增量构建的假错误

修改 `.cproject` 后不重新生成 makefile，会出现"上次的错误还在、但编译其实成功"的假象。
**修复**：`Project → Clean` 后重新 Build；或删除 `Debug/` 目录。

---

## 进度

- [x] FreeRTOS 任务框架 + 信号量事件
- [x] LVGL 9.5 移植（SPI 显示、局部刷新）
- [x] ILI9341 驱动与颜色校正
- [x] 主页界面（状态条 + 三参数 + 按钮）
- [x] RTC 时间（LSE + 备份寄存器）
- [ ] 传感器接入（心率 / 血氧 / 体表温度）
- [ ] 报警阈值与状态机
- [ ] 趋势页（`lv_chart`）
- [ ] SPI 改 DMA + 双缓冲
- [ ] 呼吸频率（称重传感器阵列方案）
- [ ] CAN 上报

---

## 许可

仅用于学习与毕业设计，第三方库遵循各自许可（LVGL MIT、ST HAL BSD-3-Clause、FreeRTOS MIT）。