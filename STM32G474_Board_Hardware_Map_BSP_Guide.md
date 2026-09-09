# STM32G474 扩展板硬件映射与 BSP / CubeMX 配置依据

> 依据文件：`SCH_G474拓展板_2026-09-09.pdf`\
> 用途：供 AI / 开发者修改 STM32CubeMX `.ioc` 文件、生成初始化代码、设计
> BSP 层和检查引脚冲突。\
> 原则：本文优先记录原理图中**明确标注**的信息。无法从原理图可靠确定的
> GPIO/接口对应关系标记为 `TODO/待确认`，不得由 AI 自行猜测。

------------------------------------------------------------------------

## 1. 板卡定位

该 PCB 是围绕 **STM32G474 核心板**设计的智能车/机器人扩展底板。

STM32G474 MCU 位于外接核心板上，通过两组 `HC-PM254-8.5H-2x22PZ`
排母（U4/U5）与扩展板连接。

扩展板主要提供：

-   3 路 USART
-   2 路 I2C
-   SPI1
-   FDCAN2 + TJA1044 CAN PHY
-   TIM1/TIM2/TIM3/TIM4 多路定时器通道
-   ADC4 多路模拟输入
-   NRF24L01 接口
-   IMU660RA 接口
-   舵机 PWM 接口
-   减速电机信号/编码器接口
-   预留 ADC / I2C 接口
-   4 个按键
-   LED
-   蜂鸣器
-   电源电压采样
-   12V / 5V / 3.3V / VCCSERVO 电源系统

------------------------------------------------------------------------

## 2. MCU 总引脚功能映射

这是修改 `.ioc` 时最重要的基础表。

  MCU Pin   原理图网络/功能   STM32 外设                建议 BSP 名称
  --------- ----------------- ------------------------- ----------------
  PA0       LED               GPIO                      `BSP_LED`
  PA1       BUZZER            GPIO / Timer 输出待配置   `BSP_BUZZER`
  PA2       USART2_TX         USART2 TX                 `BSP_UART2`
  PA3       USART2_RX         USART2 RX                 `BSP_UART2`
  PA4       KEY1              GPIO Input                `BSP_KEY1`
  PA5       KEY2              GPIO Input                `BSP_KEY2`
  PA6       KEY3              GPIO Input                `BSP_KEY3`
  PA7       KEY4              GPIO Input                `BSP_KEY4`
  PA8       I2C2_SDA          I2C2 SDA                  `BSP_I2C2`
  PA9       I2C2_SCL          I2C2 SCL                  `BSP_I2C2`
  PB3       SPI1_SCK          SPI1 SCK                  `BSP_SPI1`
  PB4       SPI1_MISO         SPI1 MISO                 `BSP_SPI1`
  PB5       SPI1_MOSI         SPI1 MOSI                 `BSP_SPI1`
  PB10      USART3_TX         USART3 TX                 `BSP_UART3`
  PB11      USART3_RX         USART3 RX                 `BSP_UART3`
  PB12      FDCAN2_RX         FDCAN2 RX                 `BSP_CAN`
  PB13      FDCAN2_TX         FDCAN2 TX                 `BSP_CAN`
  PB14      ADC4_IN4          ADC4                      `BSP_ADC`
  PB15      ADC4_IN5          ADC4                      `BSP_ADC`
  PC0       TIM1_CH1          TIM1 CH1                  `BSP_TIM1_CH1`
  PC1       TIM1_CH2          TIM1 CH2                  `BSP_TIM1_CH2`
  PC2       TIM1_CH3          TIM1 CH3                  `BSP_TIM1_CH3`
  PC3       TIM1_CH4          TIM1 CH4                  `BSP_TIM1_CH4`
  PC4       USART1_TX         USART1 TX                 `BSP_UART1`
  PC5       USART1_RX         USART1 RX                 `BSP_UART1`
  PC6       I2C4_SCL          I2C4 SCL                  `BSP_I2C4`
  PC7       I2C4_SDA          I2C4 SDA                  `BSP_I2C4`
  PD3       TIM2_CH1          TIM2 CH1                  `BSP_TIM2_CH1`
  PD4       TIM2_CH2          TIM2 CH2                  `BSP_TIM2_CH2`
  PD7       TIM2_CH3          TIM2 CH3                  `BSP_TIM2_CH3`
  PD6       TIM2_CH4          TIM2 CH4                  `BSP_TIM2_CH4`
  PD8       ADC4_IN12         ADC4                      `BSP_ADC`
  PD9       ADC4_IN13         ADC4                      `BSP_ADC`
  PD10      ADC4_IN7          ADC4                      `BSP_ADC`
  PD11      ADC4_IN8          ADC4                      `BSP_ADC`
  PD12      TIM4_CH1          TIM4 CH1                  `BSP_TIM4_CH1`
  PD13      TIM4_CH2          TIM4 CH2                  `BSP_TIM4_CH2`
  PD14      TIM4_CH3          TIM4 CH3                  `BSP_TIM4_CH3`
  PD15      TIM4_CH4          TIM4 CH4                  `BSP_TIM4_CH4`
  PE2       TIM3_CH1          TIM3 CH1                  `BSP_TIM3_CH1`
  PE3       TIM3_CH2          TIM3 CH2                  `BSP_TIM3_CH2`
  PE4       TIM3_CH3          TIM3 CH3                  `BSP_TIM3_CH3`
  PE5       TIM3_CH4          TIM3 CH4                  `BSP_TIM3_CH4`

### 2.1 重要注意事项

1.  上表表示**原理图网络定义**，不代表所有外设都必须在 `.ioc`
    中立即启用。
2.  修改 `.ioc` 前必须检查 STM32G474 具体封装的 AF 映射是否合法。
3.  不得为了消除 CubeMX 冲突而私自更改 PCB 已固定的 GPIO。
4.  如果 CubeMX 显示某个映射不可用，应优先检查：
    -   MCU 型号/封装是否选错；
    -   Alternate Function 是否理解错误；
    -   原理图是否存在设计错误；
    -   不要直接选择其他 GPIO 替代。

------------------------------------------------------------------------

## 3. USART

板上定义三套 UART。

### USART1

  信号        GPIO
  ----------- ------
  USART1_TX   PC4
  USART1_RX   PC5

CubeMX 目标配置：

``` text
USART1
Mode: Asynchronous
TX: PC4
RX: PC5
```

波特率、DMA、IRQ 优先级无法仅根据原理图确定，应由上层应用需求决定。

### USART2

  信号        GPIO
  ----------- ------
  USART2_TX   PA2
  USART2_RX   PA3

``` text
USART2
Mode: Asynchronous
TX: PA2
RX: PA3
```

### USART3

  信号        GPIO
  ----------- ------
  USART3_TX   PB10
  USART3_RX   PB11

``` text
USART3
Mode: Asynchronous
TX: PB10
RX: PB11
```

### BSP 建议

统一提供：

``` c
bsp_uart_init();
bsp_uart_send();
bsp_uart_receive();
bsp_uart_receive_it();
bsp_uart_receive_dma();
```

设备层不要直接依赖 `huartX`，建议 BSP 内部完成实例映射。

------------------------------------------------------------------------

## 4. I2C

### I2C2

  信号   GPIO
  ------ ------
  SDA    PA8
  SCL    PA9

注意：此处必须严格按照原理图网络名配置，不要凭 STM32 常见默认引脚猜测。

### I2C4

  信号   GPIO
  ------ ------
  SCL    PC6
  SDA    PC7

板上存在"预留 I2C 接口"。

建议 BSP：

``` c
bsp_i2c_init();
bsp_i2c_read();
bsp_i2c_write();
bsp_i2c_mem_read();
bsp_i2c_mem_write();
```

具体 I2C 时钟频率无法由原理图确定。

------------------------------------------------------------------------

## 5. SPI1 / NRF24L01

SPI1 硬件映射：

  SPI1   GPIO
  ------ ------
  SCK    PB3
  MISO   PB4
  MOSI   PB5

NRF24L01 接口还包含：

-   `CSN`
-   `CE`
-   `IRQ`
-   `3V3`
-   GND

### TODO

**CSN / CE / IRQ 的具体 STM32 GPIO 必须进一步根据原理图网络连线确认。**

在未确认之前：

``` text
NRF_CSN_PIN = TODO
NRF_CE_PIN  = TODO
NRF_IRQ_PIN = TODO
```

AI 不得自行分配。

建议 BSP 分层：

``` text
BSP SPI
   ↓
NRF24L01 Driver
   ↓
Wireless Application
```

例如：

``` c
bsp_spi_transfer();
bsp_nrf_csn_write();
bsp_nrf_ce_write();
```

------------------------------------------------------------------------

## 6. FDCAN2 / CAN 总线

MCU 与 CAN PHY：

  信号        GPIO
  ----------- ------
  FDCAN2_RX   PB12
  FDCAN2_TX   PB13

PHY：

``` text
TJA1044GT/3Z
```

总线侧：

``` text
CANH
CANL
```

结构：

``` text
STM32G474
   │
 FDCAN2
PB12/PB13
   │
TJA1044GT/3Z
   │
CANH / CANL
```

CubeMX 应启用：

``` text
FDCAN2
RX = PB12
TX = PB13
```

### TODO

以下参数无法从原理图确定：

-   CAN nominal bitrate
-   data bitrate
-   classic CAN / CAN FD
-   sample point
-   filter
-   message RAM
-   interrupt strategy

不得猜测。

------------------------------------------------------------------------

## 7. Timer / PWM / Encoder 资源

### TIM1

  Channel    GPIO
  ---------- ------
  TIM1_CH1   PC0
  TIM1_CH2   PC1
  TIM1_CH3   PC2
  TIM1_CH4   PC3

### TIM2

  Channel    GPIO
  ---------- ------
  TIM2_CH1   PD3
  TIM2_CH2   PD4
  TIM2_CH3   PD7
  TIM2_CH4   PD6

### TIM3

  Channel    GPIO
  ---------- ------
  TIM3_CH1   PE2
  TIM3_CH2   PE3
  TIM3_CH3   PE4
  TIM3_CH4   PE5

### TIM4

  Channel    GPIO
  ---------- ------
  TIM4_CH1   PD12
  TIM4_CH2   PD13
  TIM4_CH3   PD14
  TIM4_CH4   PD15

板上存在：

-   PWM / 舵机接口
-   减速电机信号和编码器接口

因此这些 Timer 很可能分别承担 PWM 和 Encoder 功能，但**具体哪个 Timer
对应哪个电机/编码器/舵机接口，必须根据连接器网络继续确认，不能只凭用途推断。**

CubeMX 修改时不要一次性把所有通道配置成 PWM。

应根据真实接口连接决定：

``` text
PWM Generation
Input Capture
Output Compare
Encoder Mode
GPIO
```

------------------------------------------------------------------------

## 8. ADC4

原理图定义：

  ADC Channel   GPIO
  ------------- ------
  ADC4_IN4      PB14
  ADC4_IN5      PB15
  ADC4_IN7      PD10
  ADC4_IN8      PD11
  ADC4_IN12     PD8
  ADC4_IN13     PD9

板上存在"预留 ADC 采样接口"。

建议 BSP：

``` c
bsp_adc_init();
bsp_adc_read_raw();
bsp_adc_read_voltage();
```

如果采用 DMA：

``` c
bsp_adc_start_dma();
```

### ADC 参数待应用确认

以下内容不得从原理图猜测：

-   ADC resolution
-   sampling time
-   continuous conversion
-   scan mode
-   DMA circular mode
-   oversampling

------------------------------------------------------------------------

## 9. 电源电压检测

板上包含基于：

``` text
OPA2188AIDR
```

的电源电压采样电路。

原理图注明：

> 电源电压采样，5.1 倍缩放

因此软件层应预留：

``` c
float bsp_power_get_voltage(void);
```

概念关系为：

``` text
ADC Voltage × 5.1 ≈ 被测电源电压
```

但在编写最终计算公式之前，应进一步确认：

-   对应 ADC GPIO；
-   ADC Vref；
-   电阻实际误差；
-   运放拓扑；
-   是否需要校准。

不得仅根据"5.1 倍缩放"直接硬编码最终工程参数。

------------------------------------------------------------------------

## 10. 按键

映射：

  按键   GPIO
  ------ ------
  KEY1   PA4
  KEY2   PA5
  KEY3   PA6
  KEY4   PA7

硬件中每个按键附近存在：

``` text
10 kΩ
100 nF
```

RC 网络。

CubeMX 建议先配置为：

``` text
GPIO Input
```

是否启用 EXTI 由软件架构决定，原理图不能决定。

BSP：

``` c
typedef enum {
    BSP_KEY1,
    BSP_KEY2,
    BSP_KEY3,
    BSP_KEY4
} bsp_key_t;

bool bsp_key_is_pressed(bsp_key_t key);
```

### TODO

必须确认实际按下电平是 HIGH 还是 LOW，再定义：

``` c
BSP_KEY_ACTIVE_LEVEL
```

不要猜测。

------------------------------------------------------------------------

## 11. LED

``` text
PA0 → LED
```

建议：

``` c
void bsp_led_on(void);
void bsp_led_off(void);
void bsp_led_toggle(void);
```

### TODO

LED 的有效电平应根据 LED、电阻和 GPIO 的实际连接方向确认。

------------------------------------------------------------------------

## 12. 蜂鸣器

``` text
PA1 → BUZZER
```

硬件使用晶体管 `Q1` 驱动蜂鸣器，并标有：

``` text
BUZZER1
3 kHz
```

因此 BSP 可设计：

``` c
void bsp_buzzer_on(void);
void bsp_buzzer_off(void);
void bsp_buzzer_beep(uint32_t duration_ms);
```

如果实际需要 MCU 产生 3 kHz 激励，则应进一步确认 PA1 对应 Timer AF
并配置 PWM。

**不要仅根据"3 kHz"字样直接认定 PA1 必须工作在 PWM 模式。**

------------------------------------------------------------------------

## 13. IMU660RA

扩展板存在：

``` text
IMU660RA 陀螺仪
```

连接器/模块接口在原理图第 2 页。

### TODO

目前应进一步确认：

-   使用 SPI 还是 I2C；
-   SCK/SDA/SCL/MOSI/MISO；
-   CS；
-   INT；
-   供电；
-   对应 STM32 GPIO。

在确认之前 BSP 只建立抽象层：

``` c
bsp_imu_bus_init();
```

设备驱动：

``` text
drivers/
└── imu660ra/
```

不得自行推断 IMU660RA 的总线连接。

------------------------------------------------------------------------

## 14. 舵机接口

板上有专门的：

``` text
PWM
舵机
```

接口，并使用：

``` text
VCCSERVO
```

独立电源轨。

已在接口附近出现：

``` text
TIM4_CH1 / PD12
TIM4_CH2 / PD13
VCCSERVO
GND
```

因此至少可以确认 TIM4 的部分通道与舵机/PWM 接口有关。

### 推荐 BSP 抽象

``` c
typedef enum {
    BSP_SERVO_1,
    BSP_SERVO_2
} bsp_servo_t;

void bsp_servo_set_pulse_us(bsp_servo_t servo, uint16_t us);
```

不要在 BSP 中直接使用"角度"，因为：

``` text
Timer PWM → pulse width
Servo Driver → angle mapping
```

角度转换属于设备/应用层。

------------------------------------------------------------------------

## 15. 减速电机与编码器接口

原理图明确存在：

> 减速电机信号和编码器接口

相关 Timer 网络包括 TIM1/TIM2/TIM3/TIM4。

推荐软件层设计：

``` c
void bsp_motor_pwm_set(...);
int32_t bsp_encoder_get_count(...);
void bsp_encoder_reset(...);
```

但是具体：

``` text
Motor1 → TIM?
Motor2 → TIM?
Encoder1 → TIM?
Encoder2 → TIM?
```

必须进一步沿 CN3/CN4/CN5/CN6 等连接器网络确认。

在确认之前不得自行绑定。

------------------------------------------------------------------------

## 16. 电源系统

### 输入

板上存在：

-   DC 输入接口 `DC1`
-   XT30 接口
-   电源开关 `SW5`
-   VIN / 12V 网络

### VCCSERVO

使用：

``` text
SCT2450CQSTER
L1 = 4.7 uH
```

生成独立的：

``` text
VCCSERVO
```

主要供舵机等大电流执行器。

### 5V

使用：

``` text
MP2315
L2 = 4.7 uH
```

形成 5V 电源轨。

### 3.3V

使用：

``` text
RT9013-33GB
```

由 5V 产生：

``` text
3V3
```

因此整体电源树应理解为：

``` text
VIN / 12V
│
├── Buck ──> VCCSERVO
│
└── MP2315 ──> 5V
                 │
                 └── RT9013-33 ──> 3V3
```

BSP 不应控制这些固定电源，除非后续确认某个转换器 EN 脚连接到了 MCU。

------------------------------------------------------------------------

## 17. 推荐 CubeMX `.ioc` 修改策略

AI 修改现有 `.ioc` 时应遵循以下顺序。

### Step 1：保持 MCU 型号和时钟树

除非用户明确要求，不修改：

-   MCU 型号
-   Package
-   SYS Debug
-   RCC oscillator
-   PLL
-   SYSCLK
-   APB/AHB prescaler

### Step 2：先配置固定 PCB Pin Mapping

优先加入：

``` text
PA0  LED
PA1  BUZZER

PA4  KEY1
PA5  KEY2
PA6  KEY3
PA7  KEY4

PC4  USART1_TX
PC5  USART1_RX

PA2  USART2_TX
PA3  USART2_RX

PB10 USART3_TX
PB11 USART3_RX

PB3 SPI1_SCK
PB4 SPI1_MISO
PB5 SPI1_MOSI

PB12 FDCAN2_RX
PB13 FDCAN2_TX

PA8 I2C2_SDA
PA9 I2C2_SCL

PC6 I2C4_SCL
PC7 I2C4_SDA
```

### Step 3：根据实际应用启用 Timer

不要无条件启用 TIM1\~TIM4 全部通道。

### Step 4：根据实际应用启用 ADC

只启用真实使用的 ADC4 channels。

### Step 5：DMA

UART / ADC 等是否启用 DMA 应根据已有代码和应用需求确定，不从 PCB
原理图推断。

### Step 6：NVIC

IRQ 优先级必须结合整个实时系统设计。

不要由 AI 随机分配优先级。

------------------------------------------------------------------------

## 18. 推荐 CubeMX User Label

建议在 `.ioc` 中为 GPIO 设置 User Label：

``` text
PA0  -> LED
PA1  -> BUZZER

PA4  -> KEY1
PA5  -> KEY2
PA6  -> KEY3
PA7  -> KEY4
```

对于 Alternate Function GPIO，不建议重复制造与 HAL handle 冲突的宏名。

------------------------------------------------------------------------

## 19. BSP 推荐目录

建议工程：

``` text
Core/
├── Inc/
├── Src/
└── ...

BSP/
├── Inc/
│   ├── bsp.h
│   ├── bsp_led.h
│   ├── bsp_key.h
│   ├── bsp_buzzer.h
│   ├── bsp_uart.h
│   ├── bsp_i2c.h
│   ├── bsp_spi.h
│   ├── bsp_can.h
│   ├── bsp_adc.h
│   ├── bsp_pwm.h
│   └── bsp_encoder.h
│
└── Src/
    ├── bsp_led.c
    ├── bsp_key.c
    ├── bsp_buzzer.c
    ├── bsp_uart.c
    ├── bsp_i2c.c
    ├── bsp_spi.c
    ├── bsp_can.c
    ├── bsp_adc.c
    ├── bsp_pwm.c
    └── bsp_encoder.c

Drivers/
├── NRF24L01/
├── IMU660RA/
└── ...

App/
└── ...
```

职责：

``` text
HAL / LL
   ↓
BSP
   ↓
Device Driver
   ↓
Application
```

------------------------------------------------------------------------

## 20. BSP 设计规则

### BSP 层负责

-   GPIO 操作
-   HAL Handle 映射
-   SPI/I2C/UART 收发封装
-   Timer/PWM 底层控制
-   ADC 底层采样
-   Encoder Counter
-   CAN 底层收发
-   板级初始化

### Device Driver 负责

例如：

``` text
NRF24L01
IMU660RA
Servo
Motor
Camera communication protocol
```

### Application 不应该出现

``` c
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, ...);
HAL_UART_Transmit(&huart2, ...);
__HAL_TIM_SET_COMPARE(...);
```

而应该：

``` c
bsp_led_on();
bsp_uart_send(...);
bsp_pwm_set(...);
```

这样以后更换 MCU / HAL / RTOS 时，上层代码不需要大规模修改。

------------------------------------------------------------------------

## 21. 建议统一初始化入口

``` c
void BSP_Init(void)
{
    BSP_LED_Init();
    BSP_Key_Init();
    BSP_Buzzer_Init();

    BSP_UART_Init();
    BSP_I2C_Init();
    BSP_SPI_Init();
    BSP_CAN_Init();

    BSP_ADC_Init();
    BSP_PWM_Init();
    BSP_Encoder_Init();
}
```

注意：

CubeMX 已经生成 `MX_GPIO_Init()`、`MX_USARTx_UART_Init()` 等情况下，BSP
不应重复初始化硬件。

更推荐：

``` text
HAL_Init()
SystemClock_Config()

MX_GPIO_Init()
MX_DMA_Init()
MX_ADC4_Init()
MX_FDCAN2_Init()
MX_I2C2_Init()
MX_I2C4_Init()
MX_SPI1_Init()
MX_TIMx_Init()
MX_USARTx_UART_Init()

BSP_Init()
```

此时 `BSP_Init()` 负责板级软件状态，而不是重新初始化 HAL Peripheral。

------------------------------------------------------------------------

## 22. AI 修改 `.ioc` 的硬性约束

后续 AI 在读取本文后修改 `.ioc` 时必须遵守：

1.  **原理图 GPIO 映射优先级最高。**
2.  不得自行交换 TX/RX。
3.  不得自行交换 SDA/SCL。
4.  不得因为 CubeMX 冲突就换 GPIO。
5.  不得猜测 NRF24L01 CSN/CE/IRQ。
6.  不得猜测 IMU660RA 总线连接。
7.  不得猜测电机/编码器具体 Timer 分配。
8.  不得猜测 ADC 采样时间。
9.  不得猜测 UART 波特率。
10. 不得猜测 CAN bitrate。
11. 不得猜测 NVIC priority。
12. 不得修改现有系统时钟，除非有明确需求。
13. 不得破坏现有 `USER CODE BEGIN/END` 区域。
14. 修改 `.ioc` 后必须重新检查 Pinout conflict。
15. CubeMX 重新生成代码后必须检查用户 BSP 是否仍可编译。
16. 对本文标记 `TODO`
    的内容，应返回问题或进一步检查原理图，而不是自行补全。

------------------------------------------------------------------------

## 23. 当前已确认与待确认信息

### 已确认

-   USART1：PC4 / PC5
-   USART2：PA2 / PA3
-   USART3：PB10 / PB11
-   SPI1：PB3 / PB4 / PB5
-   I2C2：PA8 / PA9
-   I2C4：PC6 / PC7
-   FDCAN2：PB12 / PB13
-   TIM1：PC0\~PC3
-   TIM2：PD3 / PD4 / PD7 / PD6
-   TIM3：PE2\~PE5
-   TIM4：PD12\~PD15
-   ADC4：PB14 / PB15 / PD8 / PD9 / PD10 / PD11
-   LED：PA0
-   BUZZER：PA1
-   KEY1~4：PA4~PA7
-   CAN PHY：TJA1044GT/3Z
-   VCCSERVO Buck：SCT2450CQSTER
-   5V Buck：MP2315
-   3V3 LDO：RT9013-33GB
-   电压采样运放：OPA2188AIDR
-   存在 NRF24L01、IMU660RA、PWM/舵机、编码器、ADC、I2C 等接口

### 待进一步确认

-   NRF24L01 CSN GPIO
-   NRF24L01 CE GPIO
-   NRF24L01 IRQ GPIO
-   IMU660RA 完整 Pin Mapping
-   各舵机接口与 Timer Channel 的一一对应
-   各电机 PWM 与 Timer Channel 的一一对应
-   各编码器与 Timer Channel 的一一对应
-   电源电压检测最终进入哪个 ADC channel
-   LED active level
-   KEY active level
-   BUZZER 实际控制方式
-   各连接器 Pin1/Pin2/... 的完整定义
-   UART baud rate
-   I2C frequency
-   SPI mode/frequency
-   CAN bitrate / CAN FD 参数
-   ADC sampling parameters
-   DMA 使用方案
-   NVIC priority

------------------------------------------------------------------------

## 24. 给后续 AI 的任务提示

如果本文件作为 AI coding agent 的上下文，可使用以下原则：

> 你正在维护一块 STM32G474 智能车/机器人扩展板。本文中的 GPIO/Peripheral
> 映射来自 PCB 原理图，应视为硬件约束。修改 CubeMX `.ioc`、HAL
> 初始化代码和 BSP 时不得擅自改变这些固定映射。对于标记
> TODO/待确认的信息，不得根据经验猜测；应检查原始原理图、现有 `.ioc`
> 或现有驱动代码后再决定。BSP 应隔离 STM32 HAL 与上层
> Device/Application，尽量避免设备驱动直接访问 `huartX`、`htimX`、GPIO
> Port/Pin 等 HAL 实现细节。

------------------------------------------------------------------------

## 25. 原理图来源

本文依据：

`SCH_G474拓展板_2026-09-09.pdf`

共 3 页：

-   Page 1：主要 MCU GPIO / Peripheral 网络映射
-   Page 2：外设接口、核心板排母、CAN、ADC、按键、LED、蜂鸣器、IMU 等
-   Page 3：12V、VCCSERVO、5V、3V3 电源系统

在本文与实际 PCB/最新版原理图发生冲突时，以**最新版原理图和实际 PCB
网络连接**为最终依据。
