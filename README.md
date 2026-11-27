# 病床体征监测终端（Bed_Detective2）

基于 **STM32F407VET6 + FreeRTOS + LVGL 9.5** 的体征监测终端：3.5 寸触摸屏做本地交互，显示血氧、心率、呼吸、体温，超阈值时三级报警。

> 本工程是从 `Bed_Detective` 分出来的**显示与交互验证分支**，主攻 ST7796 + 电容触摸这条链路。

**当前进度**：显示、触摸、界面框架、RTC、页面切换已跑通；传感器（MAX30102 / MLX90614 / HX711）待接入，界面数值暂用模拟数据。

---

## 硬件

| 项目 | 型号 / 参数 |
|---|---|
| 主控 | STM32F407VET6（Cortex-M4F，168 MHz，512 KB Flash / 192 KB RAM） |
| 显示屏 | 3.5" TFT，320×480，**ST7796**，SPI 接口 |
| 触摸 | **FT6336U** 电容触摸，I2C 接口，地址 `0x38` |
| RTC | LSE 32.768 kHz 晶振 |
| 调试串口 | USART1（PA9/PA10），115200 |

### 接线

**显示（SPI1，最高 42 MHz）**

| ST7796 模块 | F407 |
|---|---|
| VCC / GND | 3.3V / GND |
| CS | PA4 |
| RESET | PC4 |
| DC | PA6 |
| SDI(MOSI) | PA7 |
| SCK | PA5 |
| LED | 3.3V |
| SDO(MISO) | 不接（Transmit Only） |

**触摸（I2C1，400 kHz）**

| 模块丝印 | 电容模式 | F407 |
|---|---|---|
| T_SDI | SDA | PB7 |
| T_CLK | SCL | PB6 |
| T_CS | RST | PC5 |
| T_IRQ | INT | 不接（轮询） |
| T_SDO | NC | 不接 |

> 注意：这块模块的 5 个触摸脚在**电阻模式下定义完全不同**，上表是电容模式。

---

## 软件架构

### 任务

| 任务 | 优先级 | 栈 | 职责 |
|---|---|---|---|
| `KEY_Task` | Low1 | 512 B | 30 ms 扫描按键，释放二值信号量 |
| `lv_Timertask` | Low | **6 KB** | LVGL 主循环：`lv_timer_handler()`，5 ms 一轮 |
| `StartDefaultTask` | Normal | 512 B | 预留 |

```
KEY_Task ──信号量──▶ lv_Timertask ──lv_timer_handler()──▶ disp_flush() ──SPI1──▶ ST7796
                          │
                          └── indev_read_cb() ◀── I2C1 ◀── FT6336U
```

**重要约定：LVGL 的所有 API 只在 `lv_Timertask` 中调用。** 其他任务只写共享变量或投递消息，不碰控件。

### LVGL 关键配置

| 配置 | 值 | 说明 |
|---|---|---|
| `LV_COLOR_DEPTH` | 16 | RGB565 |
| `LV_MEM_SIZE` | **64 KB** | 32/48 KB 会在创建控件时耗尽并触发断言 |
| `LV_USE_OS` | **`LV_OS_NONE`** | 单任务访问，不需要锁（配 `FREERTOS` 会死锁，见踩坑 5） |
| `LV_DEF_REFR_PERIOD` | 33 ms | ≈30 fps |
| 显示缓冲 | 320 × 40 × 2 = 25.6 KB | PARTIAL 局部刷新 |
| 颜色格式 | `LV_COLOR_FORMAT_RGB565_SWAPPED` | ILI9341/ST7796 走 SPI 需要高字节先发 |
| 编译优化 | **`-Os`** | Debug 用 `-O0` 会 Flash 溢出且渲染慢 3–5 倍 |

---

## 界面

```
┌────────────────────────┐
│ ● NORMAL      12:30    │  状态条：绿/黄/红 + LED + RTC 时间
├────────────────────────┤
│   SpO2       98   %    │  28 号字
│   HR         76   bpm  │
├────────────────────────┤
│   RR         16   /min │
│   TEMP       36.5 C    │  标注体表温度
├────────────────────────┤
│  [ DETAIL ] [ TREND ]  │  切换页面（带 400 ms 防重入）
└────────────────────────┘
```

- 文案全英文：Montserrat 只有 ASCII，中文需要单独做子集字体
- 颜色编码遵循医疗报警约定：`#00C853` / `#FFB300` / `#D50000`
- 切页用 `LV_SCR_LOAD_ANIM_NONE`：SPI 屏上全屏动画太慢，容易堆积触摸事件

---

## 构建

1. 下载 [LVGL 9.5](https://github.com/lvgl/lvgl)，把 `lvgl` 目录放到工程根目录
2. 删除 `lvgl/src/drivers`（PC 平台驱动）和 `lvgl/src/libs` 下依赖外部库的目录（thorvg、libpng、libjpeg_turbo、libwebp、freetype、ffmpeg、rlottie、gltf、svg）
3. `lv_conf.h` 顶部 `#if 0` 改为 `#if 1`
4. STM32CubeIDE 打开工程，确认优化等级为 `-Os`，Build
5. `Debug/Bed_Detective2.elf` 时间戳必须是新的，再下载

> 仓库不含 `lvgl/` 目录：第三方库 400+ 文件、20 MB。

---

## 踩坑记录

每一条都真实卡了一小时以上，记下来免得重犯。

### 1. 改了代码却没有任何变化

CubeIDE 在**编译失败时仍会把上一次成功的 ELF 烧进去**，看起来"下载成功、程序在跑"，其实跑的是旧代码。

**对策**：每次下载前确认 `Debug/*.elf` 的时间戳晚于最后一次修改。

### 2. `file format not recognized`

ELF 文件头变成 `00 00 00 00`（构建被中断）。删掉 ELF → `Project → Clean` → 重新 Build。

### 3. Flash 溢出 41 KB

Debug 配置默认 `-O0`，LVGL 在 `-O0` 下体积接近 `-Os` 的两倍。

**对策**：`Tool Settings → Optimization → -Os`。顺带渲染速度提升 3–5 倍。

### 4. `LVGL ASSERT: obj != NULL`（卡在 `lv_label_set_text_vfmt`）

LVGL 内存池不足，`lv_label_create()` 返回 NULL，后续 `lv_label_set_text_fmt()` 直接断言。

**对策**：`LV_MEM_SIZE` 提到 **64 KB**（32 KB / 48 KB 都不够这个界面）。

### 5. 卡在 `lv_timer_handler()`，RTOS 里任务显示"挂起"

`lv_timer_handler()` 第一件事是 `lv_lock()`。当 `LV_USE_OS = LV_OS_FREERTOS` 时，这个互斥量在移植后进了异常状态，任务永远等锁——**不是死循环，是死锁**。

**对策**：改成 `LV_USE_OS = LV_OS_NONE`，并严格遵守"LVGL 只在 LVGL 任务里调用"。

> 排查线索：任务状态是"挂起"而不是"运行"，说明它在阻塞，不是死循环。

### 6. 触摸假点导致界面自动切页

无手指时 FT6336U 仍偶发上报触摸点，触发 `CLICKED`，配合切页动画产生"自动切换又切回来"。

**对策**：三层防护

```c
/* ① 点数必须为 1~2 */
if (n == 0 || n > 2) return false;
/* ② 坐标必须在屏幕范围内 */
if (tx >= 320 || ty >= 480) return false;
/* ③ 连续 3 次按下才认为有效，连续 2 次松开才释放 */
```

### 7. 屏幕整体反色

蓝色显示成黄色、红色发青 → 面板默认处于反显状态。

**对策**：初始化末尾发 `0x21`（INVON）。不同批次模块可能相反，`0x20` / `0x21` 互换即可。

### 8. `lv_label_set_text_fmt` 里 `%f` 输出成 "f"

LVGL 自带 `lv_snprintf` 不支持浮点（`LV_USE_FLOAT 0`）。

**对策**：温度用整数放大十倍处理，`"%d.%d"` 拼接显示，全程不碰浮点。

---

## 进度

- [x] FreeRTOS 任务框架 + 信号量
- [x] LVGL 9.5 移植（SPI 显示、局部刷新）
- [x] ST7796 显示 + 颜色校正
- [x] FT6336U 触摸（含去抖）
- [x] 主页界面（状态条 + 四参数 + 按钮）
- [x] 页面切换（防重入）
- [x] RTC 时间（LSE + 备份寄存器）
- [x] 编译优化与内存调优
- [ ] 传感器接入（MAX30102 / MLX90614 / HX711）
- [ ] 报警状态机 + 声光报警
- [ ] 趋势页（`lv_chart`）
- [ ] SPI 改 DMA + 双缓冲
- [ ] 第二块屏（ILI9341 床旁显示，SPI2）
- [ ] WiFi 上报（ESP8266）

---

## 许可

仅用于学习与毕业设计。第三方库遵循各自许可（LVGL MIT、ST HAL BSD-3-Clause、FreeRTOS MIT）。