#ifndef _zf_device_imu660ra_h_
#define _zf_device_imu660ra_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------- IMU660RA 器件地址 ---------------------- */
#define IMU660RA_DEV_ADDR             0x69U /**< 7 位 I2C 地址：SA0 上拉（模块默认）为 0x69，SA0 接地为 0x68。 */

/* ---------------------- IMU660RA 寄存器地址 ---------------------- */
#define IMU660RA_CHIP_ID              0x00U /**< 芯片 ID，IMU660RA 固定为 0x24。 */
#define IMU660RA_INT_STA              0x21U /**< 内部状态寄存器，配置加载完成时为 0x01。 */
#define IMU660RA_TEMP_ADDRESS         0x22U /**< 温度数据起始地址（LSB=0x22、MSB=0x23）。 */
#define IMU660RA_PWR_CONF             0x7CU /**< 电源配置寄存器。 */
#define IMU660RA_PWR_CTRL             0x7DU /**< 电源控制寄存器，使能加速度/陀螺仪/温度。 */
#define IMU660RA_INIT_CTRL            0x59U /**< 配置加载控制寄存器。 */
#define IMU660RA_INIT_DATA            0x5EU /**< 配置数据写入窗口。 */
#define IMU660RA_ACC_ADDRESS          0x0CU /**< 加速度计数据起始地址（6 字节，X/Y/Z 小端）。 */
#define IMU660RA_GYRO_ADDRESS         0x12U /**< 陀螺仪数据起始地址（6 字节，X/Y/Z 小端）。 */
#define IMU660RA_ACC_CONF             0x40U /**< 加速度计采集配置寄存器。 */
#define IMU660RA_ACC_RANGE            0x41U /**< 加速度计量程寄存器。 */
#define IMU660RA_GYR_CONF             0x42U /**< 陀螺仪采集配置寄存器。 */
#define IMU660RA_GYR_RANGE            0x43U /**< 陀螺仪量程寄存器。 */

/** @brief 参考库内置配置文件的字节数（Bosch BMI270 配置流）。 */
#define IMU660RA_CONFIG_FILE_SIZE     8192U
/** @brief 自检轮询次数上限（每次间隔 1 ms）。 */
#define IMU660RA_TIMEOUT_COUNT        0x00FFU
/** @brief 单次 I2C 传输超时，单位毫秒。 */
#define IMU660RA_I2C_TIMEOUT_MS       1000U

/** @brief 加速度计默认量程配置。 */
typedef enum {
  IMU660RA_ACC_SAMPLE_SGN_2G = 0, /**< ±2 g，换算系数 16384 LSB/g。 */
  IMU660RA_ACC_SAMPLE_SGN_4G,     /**< ±4 g，换算系数 8192 LSB/g。 */
  IMU660RA_ACC_SAMPLE_SGN_8G,     /**< ±8 g，换算系数 4096 LSB/g。 */
  IMU660RA_ACC_SAMPLE_SGN_16G     /**< ±16 g，换算系数 2048 LSB/g。 */
} imu660ra_acc_sample_config;

/** @brief 陀螺仪默认量程配置。 */
typedef enum {
  IMU660RA_GYRO_SAMPLE_SGN_125DPS = 0, /**< ±125 dps，换算系数 262.4 LSB/(°/s)。 */
  IMU660RA_GYRO_SAMPLE_SGN_250DPS,     /**< ±250 dps，换算系数 131.2 LSB/(°/s)。 */
  IMU660RA_GYRO_SAMPLE_SGN_500DPS,     /**< ±500 dps，换算系数 65.6 LSB/(°/s)。 */
  IMU660RA_GYRO_SAMPLE_SGN_1000DPS,    /**< ±1000 dps，换算系数 32.8 LSB/(°/s)。 */
  IMU660RA_GYRO_SAMPLE_SGN_2000DPS     /**< ±2000 dps，换算系数 16.4 LSB/(°/s)。 */
} imu660ra_gyro_sample_config;

/** @brief 加速度计初始化量程，与参考库默认值一致（±8 g）。 */
#define IMU660RA_ACC_SAMPLE_DEFAULT   IMU660RA_ACC_SAMPLE_SGN_8G
/** @brief 陀螺仪初始化量程，与参考库默认值一致（±2000 dps）。 */
#define IMU660RA_GYRO_SAMPLE_DEFAULT  IMU660RA_GYRO_SAMPLE_SGN_2000DPS

/** @brief 换算系数：[0] 加速度 LSB/g，[1] 陀螺仪 LSB/(°/s)。 */
extern float imu660ra_transition_factor[2];

/**
 * @brief 将加速度计原始值换算为重力加速度（单位 g）。
 * @param acc_value 任意轴加速度原始值。
 * @return 以 g 为单位的浮点加速度。
 */
#define imu660ra_acc_transition(acc_value)  ((float)(acc_value) / imu660ra_transition_factor[0])

/**
 * @brief 将陀螺仪原始值换算为角速度（单位 °/s）。
 * @param gyro_value 任意轴陀螺仪原始值。
 * @return 以 °/s 为单位的浮点角速度。
 */
#define imu660ra_gyro_transition(gyro_value) ((float)(gyro_value) / imu660ra_transition_factor[1])

/**
 * @brief 将温度原始值换算为摄氏度。
 * @param temp_value 温度寄存器原始值。
 * @return 摄氏度，换算关系为 23 + raw / 512。
 */
#define imu660ra_temperature_transition(temp_value) (23.0f + (float)(temp_value) / 512.0f)

/**
 * @brief 初始化 IMU660RA（I2C2：PA8=SDA、PA9=SCL）。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 内部先调用 i2c_init(I2C_2, NULL)，再按参考库序列自检、加载
 *       BMI270 配置流并配置量程；调用前需确保 system_delay_init() 已执行。
 */
zf_status_t imu660ra_init(void);

/**
 * @brief 轮询芯片 ID 进行自检。
 * @return ZF_OK 表示检测到 IMU660RA（CHIP_ID=0x24），ZF_TIMEOUT 表示超时。
 * @note 需先完成 I2C 总线初始化；每次读取间隔 1 ms，最多 IMU660RA_TIMEOUT_COUNT 次。
 */
zf_status_t imu660ra_self_check(void);

/**
 * @brief 读取三轴加速度计原始数据。
 * @param accel 输出数组，顺序为 x、y、z，单位为 LSB。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 使用 imu660ra_acc_transition() 可换算为 g。
 */
zf_status_t imu660ra_read_accel(int16_t accel[3]);

/**
 * @brief 读取三轴陀螺仪原始数据。
 * @param gyro 输出数组，顺序为 x、y、z，单位为 LSB。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 使用 imu660ra_gyro_transition() 可换算为 °/s。
 */
zf_status_t imu660ra_read_gyro(int16_t gyro[3]);

/**
 * @brief 读取芯片内部温度原始值。
 * @param temperature 输出温度寄存器原始值（16 位有符号）。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 参考库未提供温度接口，本接口依据 Bosch BMI270 数据手册实现：
 *       温度寄存器 0x22/0x23 小端组合，0x0000 对应 23 ℃，
 *       每 LSB 为 1/512 ℃，即 摄氏度 = 23 + raw / 512。
 */
zf_status_t imu660ra_read_temperature(int16_t *temperature);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_imu660ra_h_ */
