# Mini STM32 平衡车（串口四路驱动板版）

基于 STM32F103xB 的小车工程。原方案由 STM32 直接输出 PWM 并读取编码器，现改为**通过 USART3 控制一块四路串口电机驱动板**：驱动板内部完成 PWM 输出、编码器采集和速度 PID，STM32 只负责下发指令、读取反馈。

```mermaid
flowchart LR
    STM32["STM32F103<br/>USART3 115200"] -->|"$spd / $pwm / 配置指令"| DRIVER["四路电机驱动板"]
    DRIVER --> M1["M1 左轮 520 电机"]
    DRIVER --> M3["M3 右轮 520 电机"]
    DRIVER -.->|"$MAll / $MTEP / $MSPD"| STM32
```

## 硬件连接

| STM32F103 | 驱动板 | 说明 |
| --- | --- | --- |
| `PB10` (USART3_TX) | `RX` | 交叉连接 |
| `PB11` (USART3_RX) | `TX` | 交叉连接 |
| `GND` | `GND` | 必须共地 |
| — | `VM` | 电池供电，禁止用 3.3V 驱动电机 |

- 电机通道：`M1` = 左轮，`M3` = 右轮，`M2/M4` 未使用
- 电机类型：520 编码器电机
- 串口参数：`115200`、8 数据位、无校验、1 停止位、无流控

### 实物照片

| 俯视图 | 侧视图 |
| --- | --- |
| ![硬件俯视图](docs/images/hardware_top.jpg) | ![硬件侧视图](docs/images/hardware_side.jpg) |

### B570 原板引脚分配（参考）

以下引脚来自 WHEELTEC B570 资源分配表与原理图。**当前工程使用 USART3（PB10/PB11）连接四路串口驱动板，并已驱动用户按键（PA5）与 OLED（PA15/PB3/PB4/PB5），其余外设暂未配置。**

| 引脚 | 功能 | 原外设 / 说明 | 当前状态 |
| --- | --- | --- | --- |
| `PB10` / `PB11` | 蓝牙模块串口 | USART3 | ✅ 已改作四路驱动板串口 |
| `PA9` / `PA10` | MicroUSB 串口 | USART1（CH340） | ⏳ 暂未配置 |
| `PA4` | LED | GPIO | ⏳ 暂未配置 |
| `PA5` | 用户按键 | GPIO | ✅ 已驱动（`key.c`） |
| `PA15` | OLED DC | GPIO | ✅ 已驱动（`oled.c`，需关 JTAG） |
| `PB3` | OLED RES | GPIO | ✅ 已驱动（`oled.c`，需关 JTAG） |
| `PB4` | OLED SDA | GPIO | ✅ 已驱动（`oled.c`，需关 JTAG） |
| `PB5` | OLED SCL | GPIO | ✅ 已驱动（`oled.c`） |
| `PA12` | MPU6050 INT | EXTI | ⏳ 暂未配置 |
| `PB8` / `PB9` | MPU6050 软件 IIC | SCL / SDA | ⏳ 暂未配置 |
| `PA0` / `PA1` | 左轮编码器 | TIM2_CH1 / TIM2_CH2 | ⏳ 暂未配置（现由驱动板采集） |
| `PB6` / `PB7` | 右轮编码器 | TIM4_CH1 / TIM4_CH2 | ⏳ 暂未配置（现由驱动板采集） |
| `PA8` | 右轮 PWM | TIM1_CH1 | ⏳ 暂未配置（现由驱动板输出） |
| `PB14` / `PB15` | 右轮方向 `AIN1` / `AIN2` | GPIO | ⏳ 暂未配置 |
| `PA11` | 左轮 PWM | TIM1_CH4 | ⏳ 暂未配置（现由驱动板输出） |
| `PB13` / `PB12` | 左轮方向 `BIN1` / `BIN2` | GPIO | ⏳ 暂未配置 |
| `PA6` | 电池电压检测 | ADC1_CH6 | ⏳ 暂未配置 |
| `PB0` / `PB1` | 超声波模块 | TIM3_CH3 + GPIO | ⏳ 暂未配置 |
| `PA13` / `PA14` | SWD | SWDIO / SWCLK | ✅ 下载与调试 |
| `PA2` / `PA3` / `PA7` / `PB2` / `PC13` / `PC14` / `PC15` | 闲置 | — | — |

说明：

- 原蓝牙模块接在 USART3，本工程将该串口改为四路驱动板通信，因此蓝牙功能不可同时使用
- 原电机 PWM/方向与编码器引脚均不再由 STM32 使用，由串口驱动板负责
- 按键与 OLED 的 GPIO 由驱动自己初始化（`Key_Init()` / `OLED_Init()`），没有写进 CubeMX 的 `MX_GPIO_Init()`，所以重新生成代码不会把它们覆盖掉
- LED、MPU6050、电压检测、超声波等仍未配置

原理图与资源分配表见 [`docs/schematics/`](docs/schematics/)。

## 按键与 OLED

按键（PA5）与 OLED（SSD1306 128×64，四线 SPI）两部分的驱动已经写好，互相独立，可以只用一个。

### 硬件连接

| STM32F103 | OLED 模块（7 针） | 说明 |
| --- | --- | --- |
| `3V3` | `VCC` | 3.3V 供电 |
| `GND` | `GND` | 共地 |
| `PB5` | `SCL` / `D0` | 软件 SPI 时钟 |
| `PB4` | `SDA` / `D1` | 软件 SPI 数据（MOSI） |
| `PB3` | `RES` | 复位，低电平有效 |
| `PA15` | `DC` | 0 = 命令，1 = 显存数据 |
| — | `CS` | 模块无 CS 或 CS 接地 |

| STM32F103 | 按键 | 说明 |
| --- | --- | --- |
| `PA5` | 按键一端 | 内部上拉，另一端接 `GND`，按下读到低电平 |

> ⚠️ **必要前提**：`PA15`、`PB3`、`PB4` 上电默认是 JTAG 的 `JTDI`/`JTDO`/`NJTRST`。
> `OLED_Init()` 里已经调用 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 关掉 JTAG 把它们释放成普通 GPIO，
> 关掉后 **SWD（PA13/PA14）仍然可以正常下载调试**，不用担心变砖。

### 怎么用（三步）

**1. 初始化**：在 `main()` 外设初始化之后调用一次

```c
Key_Init();    // 配置 PA5 为上拉输入
OLED_Init();   // 配置 4 个 OLED 引脚、关 JTAG、复位、发初始化序列、清屏
```

**2. 显示**：按「页」定位，一页 8 行像素，共 8 页（0~7），每页 16 个 ASCII 字符

```c
OLED_Clear();                              // 清屏
OLED_Show_String(0, 0, "Mini Balance Car", 1);   // 第 0 页，第 0 列，1 倍字
OLED_Show_String(1, 0, "Speed:", 1);             // 第 1 页
OLED_Show_Float(1, 56, 152.5f, 1, 6, 1);          // 1 位小数，占 6 字符宽
OLED_Show_Int(2, 0, -320, 4, 1);                 // 整数，左补空格到 4 位
OLED_Printf(3, 0, 1, "t=%lu ms", HAL_GetTick()); // 用法同 printf，最长 31 字符
OLED_Clear_Page(4);                              // 只清第 4 页
OLED_Show_Char(5, 0, 'A', 2);                    // 2 倍放大（占 2 页）
```

- `page`：0~7，从上到下
- `column`：0~127，**像素列**；1 倍字一个字符占 8 列，第 n 个字符的起点是 `n*8`
  （例如 `"Press:"` 占 0~47 列，后面的数字从 56 列开始画）
- `scale`：1~4，放大倍数；2 倍字占 `8*2` 列、`2` 页
- 字库是 **ASCII（0x20~0x7E）**，中文会显示成 `?`

**3. 扫描按键**：在主循环里反复调用，**不要用 `HAL_Delay()` 卡住主循环**，否则按键会漏采样

```c
while (1) {
  switch (Key_Scan()) {
    case KEY_EVENT_PRESS:      /* 消抖后的按下 */      break;
    case KEY_EVENT_RELEASE:    /* 消抖后的松开 */      break;
    case KEY_EVENT_LONG_PRESS: /* 按住满 800 ms */     break;
    case KEY_EVENT_REPEAT:     /* 长按后每 150 ms 连发 */ break;
    default: break;            /* KEY_EVENT_NONE */
  }
}
```

- 非阻塞，内部用 `HAL_GetTick()` 做 20 ms 消抖，主循环 5~10 ms 跑一圈就够灵敏
- `KEY_EVENT_PRESS` / `KEY_EVENT_RELEASE` 成对出现；长按会先给 `PRESS`，再给 `LONG_PRESS`，之后周期给 `REPEAT`
- 只想知道「此刻按下没有」也可以直接用 `Key_Is_Pressed()`

### 接口速查

```c
/* oled.h */
void OLED_Init(void);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_Set_Contrast(uint8_t contrast);      // 0~255
void OLED_Clear(void);
void OLED_Clear_Page(uint8_t page);
void OLED_Set_Cursor(uint8_t page, uint8_t column);
void OLED_Show_Char(uint8_t page, uint8_t column, char ch, uint8_t scale);
void OLED_Show_String(uint8_t page, uint8_t column, const char *str, uint8_t scale);
void OLED_Show_Int(uint8_t page, uint8_t column, int32_t value, uint8_t width, uint8_t scale);
void OLED_Show_Float(uint8_t page, uint8_t column, float value, uint8_t decimals,
                     uint8_t width, uint8_t scale);
void OLED_Printf(uint8_t page, uint8_t column, uint8_t scale, const char *format, ...);

/* key.h */
typedef enum { KEY_EVENT_NONE = 0, KEY_EVENT_PRESS, KEY_EVENT_RELEASE,
               KEY_EVENT_LONG_PRESS, KEY_EVENT_REPEAT } KeyEvent;
void     Key_Init(void);
KeyEvent Key_Scan(void);
uint8_t  Key_Is_Pressed(void);
```

驱动直接写显存，不占 1KB 的显存缓冲，`OLED_Clear()` 一次约几毫秒，可以放心在主循环里调用。

### 可调参数

`Core/Src/oled.c` 顶部：

| 宏 | 默认 | 作用 |
| --- | --- | --- |
| `OLED_SCK_PIN` / `OLED_SDA_PIN` / `OLED_RES_PIN` / `OLED_DC_PIN` | `PB5` / `PB4` / `PB3` / `PA15` | 换引脚只改这里 |
| `OLED_SEGMENT_REMAP` | 1 | 画面左右镜像时改成 0 |
| `OLED_COM_SCAN_DEC` | 1 | 画面上下颠倒时改成 0 |
| `OLED_CHARGE_PUMP` | 1 | 模块自带升压；屏不亮时先确认这里是 1 |

`Core/Src/key.c` 顶部：

| 宏 | 默认 | 作用 |
| --- | --- | --- |
| `KEY_PIN` | `PA5` | 按键引脚 |
| `KEY_ACTIVE_LEVEL` | 0 | 0 = 按下为低电平（内部上拉），1 = 按下为高电平（内部下拉） |
| `KEY_DEBOUNCE_MS` | 20 | 消抖时间 |
| `KEY_LONG_PRESS_MS` | 800 | 长按门限 |
| `KEY_REPEAT_MS` | 150 | 长按连发周期 |

### 排错

| 现象 | 处理 |
| --- | --- |
| 屏全黑 | 先查 `VCC`/`GND`；再把 `OLED_Set_Contrast()` 调到 `0xFF` 试；确认 `OLED_CHARGE_PUMP` 为 1 |
| 屏是亮的但没字 | 八成是 `DC`/`RES` 接反或 `PB3`/`PA15` 没被释放成 GPIO —— 确认 `OLED_Init()` 里调用了 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` |
| 文字上下颠倒 | `OLED_COM_SCAN_DEC` 改 0 |
| 文字左右镜像 | `OLED_SEGMENT_REMAP` 改 0 |
| 花屏/乱码 | `SCL`/`SDA` 接反，或杜邦线太长、接触不良；软件 SPI 不需要额外上拉 |
| 按键没反应 | 万用表量按下时 PA5 是否被拉到 GND；若板上是按键接 3V3，把 `KEY_ACTIVE_LEVEL` 改成 1 |
| 按键偶尔连跳 | 主循环里加了 `HAL_Delay()` 导致采样太慢，或者把 `KEY_DEBOUNCE_MS` 加大到 30~50 |

## 通信协议

### 下行指令（STM32 → 驱动板）

| 指令 | 作用 | 范围 / 默认值 |
| --- | --- | --- |
| `$mtype:x#` | 电机类型 | 1=520，2=310，3=TT 带编码器，4=TT 无编码器 |
| `$deadzone:x#` | PWM 死区 | 0~3600，默认 1600 |
| `$mline:x#` | 磁环线数 | 默认 13 |
| `$mphase:x#` | 减速比 | 默认 30 |
| `$wdiameter:x#` | 轮径 (mm) | 默认 67 |
| `$MPID:p,i,d#` | 速度环 PID | 默认 0.8, 0.06, 0.5 |
| `$upload:a,b,c#` | 上报开关 | a=总编码器，b=10ms 编码器，c=速度 |
| `$spd:m1,m2,m3,m4#` | 闭环速度 | -1000~1000 |
| `$pwm:m1,m2,m3,m4#` | 开环 PWM | -3600~3600 |
| `$read_vol#` | 查询电池电压 | — |
| `$flash_reset#` | 恢复出厂设置 | — |

注意：

- 配置类指令成功后驱动板会返回 `...OK`
- `$MPID` 会导致驱动板重启并停止电机，只应在初始化时发送
- 配置参数掉电保存，初始化时重发一次即可
- `$spd:0,0,0,0#` 是**锁轮**（PID 仍在工作）；需要自由滑行请用 `$pwm:0,0,0,0#`

### 上行数据（驱动板 → STM32）

| 帧 | 含义 |
| --- | --- |
| `$MAll:m1,m2,m3,m4#` | 累计编码器计数 |
| `$MTEP:m1,m2,m3,m4#` | 10ms 编码器增量 |
| `$MSPD:m1,m2,m3,m4#` | 速度，单位 mm/s |

## 软件结构

```text
Core/
├── Inc/
│   ├── motor.h        # 电机控制接口与通道定义
│   ├── key.h          # 按键事件与接口
│   ├── oled.h         # OLED 显示接口
│   ├── usart.h        # USART3 (PB10/PB11) 句柄
│   └── dma.h
└── Src/
    ├── motor.c        # 协议发送、接收解析、编码器读取
    ├── key.c          # PA5 按键扫描、消抖、长按/连发
    ├── oled.c         # SSD1306 四线软件 SPI 驱动与 8x8 字库
    ├── usart.c        # USART3 115200 8N1
    └── main.c         # 初始化与按键 + OLED 演示流程
```

### 电机接口

```c
void    Motor_Init(void);
void    Motor_Set_Pid(float p, float i, float d);
void    Motor_Set_Speed(int16_t left, int16_t right);  // -1000 ~ 1000
void    Motor_Set_Pwm(int16_t left, int16_t right);    // -3600 ~ 3600
void    Motor_Stop(void);

int32_t Motor_Get_Encoder(MotorChannel ch);        // 累计编码器
int32_t Motor_Get_Encoder_Delta(MotorChannel ch);  // 10ms 增量
float   Motor_Get_Speed(MotorChannel ch);          // mm/s
```

通道定义：

```c
typedef enum {
  MOTOR_M1 = 0,  // 左轮
  MOTOR_M2,      // 未使用
  MOTOR_M3,      // 右轮
  MOTOR_M4,      // 未使用
  MOTOR_CH_COUNT
} MotorChannel;
```

### 接收实现

- `Motor_Init()` 启动 `HAL_UART_Receive_IT()`，单字节中断接收
- `HAL_UART_RxCpltCallback()` 中按 `$...#` 组帧
- 帧解析结果写入 `volatile` 数组，主循环可直接读取
- 三路上报全部开启时，115200 下约 8KB/s

## 参数配置

集中在 `Core/Src/motor.c` 顶部：

```c
#define MOTOR_TYPE 1              // 520 电机
#define MOTOR_PHASE 30            // 减速比
#define MOTOR_LINE 11             // 磁环线数
#define MOTOR_WHEEL_DIAMETER 67.0f // 轮径 mm
#define MOTOR_DEADZONE 1900       // PWM 死区

#define MOTOR_LEFT_INVERT 1       // 左轮方向取反
#define MOTOR_RIGHT_INVERT 1      // 右轮方向取反

#define MOTOR_UPLOAD_TOTAL_ENCODER 1
#define MOTOR_UPLOAD_DELTA_ENCODER 1
#define MOTOR_UPLOAD_SPEED 1
```

- 直行正常但左右转反：交换 `Motor_Set_Speed()` 的左右参数或对调 M1/M3 接线
- 单边方向反：只把对应 `MOTOR_*_INVERT` 置 1
- 轮径/减速比/磁环线数不正确时，速度与里程会偏差，但原始编码器值不受影响

## 构建

### 环境要求

| 组件 | 说明 |
| --- | --- |
| VS Code + STM32 VS Code 扩展 | 提供 `cube-cmake`（推荐） |
| CMake ≥ 3.22 + Ninja | 没有 `cube-cmake` 时的回退方案 |
| `arm-none-eabi-gcc` | GNU Arm 工具链，需在 PATH 或常见安装目录 |
| OpenOCD | 推荐 `winget install xpack-dev-tools.openocd-xpack` |
| 调试器 | CMSIS-DAP；ST-Link 需设置 `OPENOCD_INTERFACE` |

`tools/build.ps1` 会自动查找 `cube-cmake`、`cmake`、`arm-none-eabi-gcc` 和 `openocd` 的常见安装位置，找不到时输出安装提示。

### 一键编译 + 烧录（Ctrl+Shift+B）

默认任务 **Build & Flash** 依次执行：

1. `tools/build.ps1`：查找工具链 → 首次自动 configure → 编译
2. `tools/flash.ps1`：查找 OpenOCD → 烧录 → 校验 → 复位

首次打开仓库时 `build/` 不存在，脚本会自动配置：

```text
cmake -S . -B build/Debug -G Ninja
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
      -DCMAKE_BUILD_TYPE=Debug
```

### 手动构建

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```

## 烧录

默认使用 CMSIS-DAP：

```powershell
powershell -ExecutionPolicy Bypass -File tools/flash.ps1
```

使用 ST-Link 时通过环境变量切换接口：

```powershell
$env:OPENOCD_INTERFACE = "interface/stlink.cfg"
powershell -ExecutionPolicy Bypass -File tools/flash.ps1
```

等效的 OpenOCD 命令：

```powershell
openocd -f interface/cmsis-dap.cfg `
        -c "transport select swd" `
        -f target/stm32f1x.cfg `
        -c "adapter speed 1000" `
        -c "program build/Debug/Mini_STM32_Blance_Car.elf verify reset exit"
```

注意：

- OpenOCD 的 `-c` 参数按 Tcl 解析，路径中的 `\` 会被当转义符，**必须使用相对路径或正斜杠**
- 若提示 `unable to find a matching CMSIS-DAP device`，检查 `E:` 盘是否存在，重新插拔调试器
- 不要同时使用 OpenOCD 和向 `E:` 复制文件的 DAPLink 拖拽烧录方式
- `tools/*.ps1` 为 Windows PowerShell 脚本；Linux/macOS 需手动执行等价的 CMake/OpenOCD 命令

VS Code 任务定义在 `.vscode/tasks.json`：

| 任务 | 说明 |
| --- | --- |
| `Build & Flash` | 编译 + 烧录（默认，`Ctrl+Shift+B`） |
| `Build` | 仅编译 |
| `Flash (OpenOCD)` | 仅烧录当前 ELF |

## 版本控制

`.gitignore` 已排除以下内容，克隆后无需手动清理：

- `build/`、`mx.scratch`、`.settings/`
- `*.elf`、`*.bin`、`*.hex`、`*.map`、`*.o`、`*.d`、`*.su`
- 本项目未使用的 CMSIS 组件：`Lib`、`DSP`、`NN`、`RTOS`、`RTOS2`、`Core_A`、`Core/Template`、`docs`

## 测试流程

`main.c` 内置一个按键 + OLED 演示（电机部分只做 `Motor_Init()`，不下发运动指令，烧录后车轮不会转）：

```text
OLED 开机画面：
  Mini Balance Car
  F103 + SSD1306
  KEY: PA5
  press to count

主循环：
  每轮调用 Key_Scan()
    短按 → Press 计数 +1
    长按 800 ms → Long 计数 +1，之后每 150 ms 连发
  每 100 ms 刷新一次：
    Press: <短按次数>
    Long : <长按次数>
    Up   : <上电秒数>
    KEY  DOWN / KEY  UP
```

同时在调试器中可观察：

```c
test_key_press_count / test_key_long_count  // 按键计数
test_left_enc / test_right_enc              // 累计编码器
test_left_delta / test_right_delta          // 10ms 增量
test_left_speed / test_right_speed          // mm/s
```

## 注意事项

1. `CMakeLists.txt` 中启用了 `-u _printf_float`，否则 `$MPID` 的 `%f` 格式化在 newlib-nano 下无效
2. `HAL_UART_RxCpltCallback()` 目前由 `motor.c` 独占；若增加其他串口，需在回调中判断 `Instance`
3. USART3 的 DMA 已由 CubeMX 配置，但当前接收使用中断模式，DMA 未启用
4. 驱动板配置保存在其 Flash 中，更换电机参数后需重新发送配置指令
5. `OLED_Init()` 会关闭 JTAG 以释放 `PA15`/`PB3`/`PB4`，SWD 仍可用；如果以后要用 JTAG 调试，需要改回 `__HAL_AFIO_REMAP_SWJ_ENABLE()`
6. OLED 直接写显存，帧率高时 `OLED_Clear()` + 全屏重绘会占用较多 CPU 时间；刷新频率控制在 10~30 Hz 即可
7. `Key_Scan()` 依赖 `HAL_GetTick()`，主循环里不要长时间 `HAL_Delay()`，否则按键采样变慢
