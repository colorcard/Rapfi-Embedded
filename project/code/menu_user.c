#include "menu_user.h"

#include <math.h>

#include "menu_view.h"
#include "rp_common_debug.h"
#include "rp_device_buzzer.h"
#include "rp_device_imu660ra.h"
#include "rp_device_power.h"
#include "rp_driver_adc.h"
#include "rp_driver_rtc.h"

static void menu_user_key_remap_test(menu_action_enum action);
static void menu_user_placeholder(menu_action_enum action);
static void menu_user_param_view(menu_action_enum action);
static void menu_user_param_view_poll(void);
static void menu_user_timer(menu_action_enum action);
static void menu_user_timer_poll(void);
static void menu_user_imu_angle(menu_action_enum action);
static void menu_user_imu_angle_poll(void);
static void menu_user_buzzer(menu_action_enum action);
static void menu_user_melody(menu_action_enum action);
static void menu_user_melody_poll(void);

/** @brief 按键映射测试页面显示的可增减测试值。 */
static int32_t key_test_value;
/** @brief 参数观察页上一次刷新时的 HAL 毫秒时基，用于限频。 */
static uint32_t param_view_last_tick;
/** @brief 计时器是否正在计时。 */
static bool timer_running;
/** @brief 暂停前已累计的秒数。 */
static uint32_t timer_accumulated;
/** @brief 本轮开始计时时的 RTC 秒计数。 */
static uint32_t timer_base;
/** @brief 计时器页最近一次显示的秒数，用于避免无变化重绘。 */
static uint32_t timer_last_value;
/** @brief true 表示计时器页已进入（区分进入时的 OK 与后续按键）。 */
static bool timer_page_entered;
/** @brief IMU 页面显示数据。 */
static menu_imu_data_t imu_data;
/** @brief IMU 页面最近一次采样的 HAL 毫秒时基。 */
static uint32_t imu_last_tick;
/** @brief IMU 调试打印最近一次输出的 HAL 毫秒时基。 */
static uint32_t imu_debug_tick;
/** @brief 蜂鸣器页当前音名索引。 */
static uint32_t buzzer_note_index;
/** @brief 蜂鸣器是否正在鸣叫。 */
static bool buzzer_playing;
/** @brief true 表示蜂鸣器页已进入。 */
static bool buzzer_page_entered;

/** @brief 音名与频率（十二平均律，A4=440Hz），范围 C4~B6。 */
typedef struct {
  const char *name;
  uint32_t freq_hz;
} buzzer_note_t;

static const buzzer_note_t buzzer_notes[] = {
  {"C4", 262},  {"C#4", 277}, {"D4", 294},  {"D#4", 311}, {"E4", 330},
  {"F4", 349},  {"F#4", 370}, {"G4", 392},  {"G#4", 415}, {"A4", 440},
  {"A#4", 466}, {"B4", 494},  {"C5", 523},  {"C#5", 554}, {"D5", 587},
  {"D#5", 622}, {"E5", 659},  {"F5", 698},  {"F#5", 740}, {"G5", 784},
  {"G#5", 831}, {"A5", 880},  {"A#5", 932}, {"B5", 988},  {"C6", 1047},
  {"C#6", 1109}, {"D6", 1175}, {"D#6", 1245}, {"E6", 1319}, {"F6", 1397},
  {"F#6", 1480}, {"G6", 1568}, {"G#6", 1661}, {"A6", 1760}, {"A#6", 1865},
  {"B6", 1976},
};

#define BUZZER_NOTE_COUNT ((uint32_t)(sizeof(buzzer_notes) / sizeof(buzzer_notes[0])))
#define BUZZER_NOTE_DEFAULT_INDEX 9U /* A4 */

/**
 * @brief 按频率查音名。
 * @param freq 频率，单位 Hz。
 * @return 音名字符串；未匹配时返回 "--"。
 */
static const char *note_name_from_freq(uint32_t freq)
{
  uint32_t i;

  for (i = 0U; i < BUZZER_NOTE_COUNT; ++i) {
    if (buzzer_notes[i].freq_hz == freq) {
      return buzzer_notes[i].name;
    }
  }
  return "--";
}

/** @brief 旋律音符：频率 + 时值（1=全音符，2=二分，4=四分，8=八分）。 */
typedef struct {
  uint16_t freq_hz;
  uint16_t divider;
} melody_note_t;

/** @brief 测试曲《小星星》（4/4，每拍四分音符）。 */
static const melody_note_t melody_star[] = {
  {262, 4}, {262, 4}, {392, 4}, {392, 4}, {440, 4}, {440, 4}, {392, 2},
  {349, 4}, {349, 4}, {330, 4}, {330, 4}, {294, 4}, {294, 4}, {262, 2},
  {392, 4}, {392, 4}, {349, 4}, {349, 4}, {330, 4}, {330, 4}, {294, 2},
  {392, 4}, {392, 4}, {349, 4}, {349, 4}, {330, 4}, {330, 4}, {294, 2},
  {262, 4}, {262, 4}, {392, 4}, {392, 4}, {440, 4}, {440, 4}, {392, 2},
  {349, 4}, {349, 4}, {330, 4}, {330, 4}, {294, 4}, {294, 4}, {262, 2},
};

#define MELODY_STAR_COUNT ((uint32_t)(sizeof(melody_star) / sizeof(melody_star[0])))
/** @brief 速度（BPM）。 */
#define MELODY_TEMPO     150U
/** @brief 全音符时长，单位 ms。 */
#define MELODY_WHOLE_MS  ((60000U * 4U) / MELODY_TEMPO)

/** @brief 旋律页状态。 */
static bool melody_playing;
static bool melody_in_gap;
static uint32_t melody_index;
static uint32_t melody_note_start;
static bool melody_page_entered;

/*
 * 菜单层级规划：
 *   顶层（parent_id = -1）直接列出所有功能页，按 OK 进入、BACK 返回，
 *   不再设置 "Main Menu"/"modeX" 之类的中间容器，避免层级过深。
 *
 * 新增页面：在数组末尾追加 {唯一ID, -1, "清晰标题", 按键回调, 周期回调或NULL}。
 * 命名建议：标题为简短英文（LCD 仅支持 ASCII），一眼能看出功能。
 */
static menu_item_t user_menu_items[] = {
    {11, -1, "Param View", menu_user_param_view, menu_user_param_view_poll},
    {12, -1, "Timer", menu_user_timer, menu_user_timer_poll},
    {8,  -1, "Key Test",   menu_user_key_remap_test, NULL},
    {9,  -1, "IMU Angle",  menu_user_imu_angle, menu_user_imu_angle_poll},
    {13, -1, "Buzzer",     menu_user_buzzer, NULL},
    {14, -1, "Melody",     menu_user_melody, menu_user_melody_poll},
    {10, -1, "Motor Cal",  menu_user_placeholder, NULL},
};

/**
 * @brief 初始化用户功能页面中的可变业务数据。
 * @return 无。
 */
void menu_user_init(void)
{
  key_test_value = 0;
  param_view_last_tick = 0U;
  timer_running = false;
  timer_accumulated = 0U;
  timer_base = 0U;
  timer_last_value = 0U;
  timer_page_entered = false;
  imu_last_tick = 0U;
  imu_debug_tick = 0U;
  buzzer_note_index = BUZZER_NOTE_DEFAULT_INDEX;
  buzzer_playing = false;
  buzzer_page_entered = false;
  melody_playing = false;
  melody_in_gap = false;
  melody_index = 0U;
  melody_note_start = 0U;
  melody_page_entered = false;
}

/**
 * @brief 返回用户定义的菜单配置表及其元素数量。
 * @param item_count 用于接收菜单项数量的有效指针。
 * @return user_menu_items 数组首地址。
 */
menu_item_t *menu_user_get_items(size_t *item_count)
{
  if (item_count != NULL) {
    *item_count = sizeof(user_menu_items) / sizeof(user_menu_items[0]);
  }
  return user_menu_items;
}

/**
 * @brief 处理按键映射测试功能并重绘该功能页面。
 * @param action 本次需要处理的菜单按键动作。
 * @return 无。
 */
static void menu_user_key_remap_test(menu_action_enum action)
{
  switch (action) {
    case MENU_ACTION_UP:
    case MENU_ACTION_UP_LONG:
      ++key_test_value;
      break;
    case MENU_ACTION_DOWN:
    case MENU_ACTION_DOWN_LONG:
      --key_test_value;
      break;
    case MENU_ACTION_OK:
    case MENU_ACTION_OK_LONG:
      key_test_value = 0;
      break;
    case MENU_ACTION_BACK:
    case MENU_ACTION_BACK_LONG:
      menu_exit_function();
      return;
    default:
      break;
  }

  menu_view_user_key_remap_test_8(key_test_value);
  menu_request_refresh();
}

/**
 * @brief 显示尚未接入具体业务逻辑的用户占位页面。
 * @param action 本次按键动作；返回动作退出当前功能页。可在该函数中自由映射定义功能
 * @return 无。
 */
static void menu_user_placeholder(menu_action_enum action)
{
  //默认保留的返回键位，每个回调函数中必须有该功能，可以只启用一个
  if ((action == MENU_ACTION_BACK) || (action == MENU_ACTION_BACK_LONG)) {
    menu_exit_function();
    return;
  }

  //需要自定义页面绘制函数可在view文件中定义随后在此调用
  menu_view_user_function_page_common(menu_get_current_item()->name,
                                    "Callback not installed",
                                    "Edit menu_user.c");
  menu_request_refresh();//申请刷屏，为了便于维护建议不要在view的页面函数中随意调用该函数
}

/**
 * @brief 采集参数并绘制参数观察页。
 * @param full true 表示整页重绘，false 表示只刷新数值区。
 * @return 无。
 */
static void menu_user_param_draw(bool full)
{
  uint32_t voltage_mv = 0U;
  uint16_t adc_raw = 0U;

  if (adc_convert(ADC4_IN4, &adc_raw) != RP_OK) {
    adc_raw = 0U;
  }
  if (power_read_voltage_mv(&voltage_mv) != RP_OK) {
    voltage_mv = 0U;
  }

  if (full) {
    menu_view_user_param_view_11(voltage_mv, adc_raw, HAL_GetTick() / 1000U);
  } else {
    menu_view_user_param_view_11_refresh(voltage_mv, adc_raw,
                                         HAL_GetTick() / 1000U);
  }
}

/**
 * @brief 参数观察页的按键处理函数。
 * @param action 本次按键动作。
 * @return 无。
 * @note 进入页面时整页绘制一次；BACK 退出返回上级菜单。
 */
static void menu_user_param_view(menu_action_enum action)
{
  if ((action == MENU_ACTION_BACK) || (action == MENU_ACTION_BACK_LONG)) {
    menu_exit_function();
    return;
  }

  param_view_last_tick = HAL_GetTick();
  menu_user_param_draw(true);
  menu_request_refresh();
}

/**
 * @brief 参数观察页的周期刷新回调。
 * @return 无。
 * @note 限频到约 4 Hz，只刷新数值区，避免整屏刷新拖慢菜单。
 */
static void menu_user_param_view_poll(void)
{
  uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - param_view_last_tick) < 250U) {
    return;
  }
  param_view_last_tick = now;
  menu_user_param_draw(false);
  menu_request_refresh();
}

/**
 * @brief 计算计时器当前已计时的秒数。
 * @return 已计时秒数。
 */
static uint32_t menu_user_timer_elapsed(void)
{
  if (timer_running) {
    return timer_accumulated + (rtc_get_seconds() - timer_base);
  }
  return timer_accumulated;
}

/**
 * @brief 计时器页的按键处理函数。
 * @param action 本次按键动作。
 * @return 无。
 * @note 进入后 OK：开始/暂停；UP：清零；BACK：退出。
 */
static void menu_user_timer(menu_action_enum action)
{
  uint32_t now = rtc_get_seconds();

  if (!timer_page_entered) {
    /* 首次进入只绘制，保持原有计时状态，避免进入动作误触发。 */
    timer_page_entered = true;
    timer_last_value = menu_user_timer_elapsed();
    menu_view_user_timer_12(timer_last_value, timer_running);
    menu_request_refresh();
    return;
  }

  switch (action) {
    case MENU_ACTION_OK:
    case MENU_ACTION_OK_LONG:
      if (timer_running) {
        timer_accumulated += now - timer_base;
        timer_running = false;
      } else {
        timer_base = now;
        timer_running = true;
      }
      break;

    case MENU_ACTION_UP:
    case MENU_ACTION_UP_LONG:
      timer_accumulated = 0U;
      timer_base = now;
      timer_running = false;
      break;

    case MENU_ACTION_BACK:
    case MENU_ACTION_BACK_LONG:
      timer_page_entered = false;
      menu_exit_function();
      return;

    default:
      break;
  }

  timer_last_value = menu_user_timer_elapsed();
  menu_view_user_timer_12(timer_last_value, timer_running);
  menu_request_refresh();
}

/**
 * @brief 读取一次 IMU660RA 并换算姿态角与温度。
 * @return 无。
 * @note 读写失败时把 valid 置 false，由页面提示读取错误。
 */
static void menu_user_imu_update(void)
{
  int16_t acc[3];
  int16_t gyro[3];
  int16_t temperature = 0;

  if ((imu660ra_read_accel(acc) != RP_OK) ||
      (imu660ra_read_gyro(gyro) != RP_OK)) {
    imu_data.valid = false;
    return;
  }

  for (uint8_t i = 0U; i < 3U; ++i) {
    imu_data.acc[i] = acc[i];
    imu_data.gyro[i] = gyro[i];
  }

  /* 由重力分量解算俯仰/横滚角，系数 57.29578*10 直接得到 0.1°。 */
  {
    float ax = (float)acc[0];
    float ay = (float)acc[1];
    float az = (float)acc[2];

    imu_data.pitch_d10 = (int16_t)lroundf(
        atan2f(-ax, sqrtf(ay * ay + az * az)) * 572.9578f);
    imu_data.roll_d10 = (int16_t)lroundf(atan2f(ay, az) * 572.9578f);
  }

  if (imu660ra_read_temperature(&temperature) == RP_OK) {
    /* 摄氏度 = 23 + raw/512，转 0.1℃：230 + raw*10/512。 */
    imu_data.temp_d10 = (int16_t)(230 + ((int32_t)temperature * 10) / 512);
  } else {
    imu_data.temp_d10 = 0;
  }

  imu_data.valid = true;
}

/**
 * @brief IMU 姿态页的按键处理函数。
 * @param action 本次按键动作。
 * @return 无。
 */
static void menu_user_imu_angle(menu_action_enum action)
{
  if ((action == MENU_ACTION_BACK) || (action == MENU_ACTION_BACK_LONG)) {
    menu_exit_function();
    return;
  }

  imu_last_tick = HAL_GetTick();
  imu_debug_tick = imu_last_tick;
  menu_user_imu_update();
  menu_view_user_imu_angle_9(&imu_data);
  menu_request_refresh();
}

/**
 * @brief IMU 姿态页的周期刷新回调。
 * @return 无。
 * @note 采样限频到 10 Hz；每秒经调试串口输出一次原始值，便于无屏核对。
 */
static void menu_user_imu_angle_poll(void)
{
  uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - imu_last_tick) < 100U) {
    return;
  }
  imu_last_tick = now;
  menu_user_imu_update();

  if ((uint32_t)(now - imu_debug_tick) >= 1000U) {
    imu_debug_tick = now;
    debug_printf("IMU A=%d,%d,%d G=%d,%d,%d P=%d R=%d T=%d valid=%d\r\n",
                 (int)imu_data.acc[0], (int)imu_data.acc[1],
                 (int)imu_data.acc[2], (int)imu_data.gyro[0],
                 (int)imu_data.gyro[1], (int)imu_data.gyro[2],
                 (int)imu_data.pitch_d10, (int)imu_data.roll_d10,
                 (int)imu_data.temp_d10, (int)imu_data.valid);
  }

  menu_view_user_imu_angle_9_refresh(&imu_data);
  menu_request_refresh();
}

/**
 * @brief 计时器页的周期刷新回调。
 * @return 无。
 * @note 仅当秒数变化时重绘；开始/暂停由按键处理即时刷新。
 */
static void menu_user_timer_poll(void)
{
  uint32_t elapsed = menu_user_timer_elapsed();

  if (elapsed == timer_last_value) {
    return;
  }
  timer_last_value = elapsed;
  menu_view_user_timer_12_refresh(elapsed, timer_running);
  menu_request_refresh();
}

/**
 * @brief 蜂鸣器测试页的按键处理函数。
 * @param action 本次按键动作。
 * @return 无。
 * @note 进入即初始化 PWM；UP/DOWN 切换音符，OK 开/关，BACK 退出并静音。
 */
static void menu_user_buzzer(menu_action_enum action)
{
  if (!buzzer_page_entered) {
    /* 首次进入：初始化 PWM 并绘制，保持静音。 */
    buzzer_page_entered = true;
    buzzer_init();
    buzzer_note_index = BUZZER_NOTE_DEFAULT_INDEX;
    buzzer_playing = false;
    buzzer_off();
    menu_view_user_buzzer_13(buzzer_notes[buzzer_note_index].name,
                             buzzer_notes[buzzer_note_index].freq_hz,
                             buzzer_playing);
    menu_request_refresh();
    return;
  }

  switch (action) {
    case MENU_ACTION_UP:
    case MENU_ACTION_UP_LONG:
      buzzer_note_index = (buzzer_note_index + 1U) % BUZZER_NOTE_COUNT;
      if (buzzer_playing) {
        buzzer_tone(buzzer_notes[buzzer_note_index].freq_hz);
      }
      break;

    case MENU_ACTION_DOWN:
    case MENU_ACTION_DOWN_LONG:
      buzzer_note_index =
          (buzzer_note_index + BUZZER_NOTE_COUNT - 1U) % BUZZER_NOTE_COUNT;
      if (buzzer_playing) {
        buzzer_tone(buzzer_notes[buzzer_note_index].freq_hz);
      }
      break;

    case MENU_ACTION_OK:
    case MENU_ACTION_OK_LONG:
      buzzer_playing = !buzzer_playing;
      if (buzzer_playing) {
        buzzer_tone(buzzer_notes[buzzer_note_index].freq_hz);
      } else {
        buzzer_off();
      }
      break;

    case MENU_ACTION_BACK:
    case MENU_ACTION_BACK_LONG:
      buzzer_off();
      buzzer_playing = false;
      buzzer_page_entered = false;
      menu_exit_function();
      return;

    default:
      break;
  }

  menu_view_user_buzzer_13(buzzer_notes[buzzer_note_index].name,
                           buzzer_notes[buzzer_note_index].freq_hz,
                           buzzer_playing);
  menu_request_refresh();
}

/**
 * @brief 从头开始播放测试曲。
 * @return 无。
 */
static void menu_user_melody_start(void)
{
  melody_index = 0U;
  melody_note_start = HAL_GetTick();
  melody_playing = true;
  melody_in_gap = false;
  buzzer_tone(melody_star[0].freq_hz);
  menu_view_user_melody_14(note_name_from_freq(melody_star[0].freq_hz), 1U,
                           MELODY_STAR_COUNT, true);
  menu_request_refresh();
}

/**
 * @brief 旋律页的按键处理函数。
 * @param action 本次按键动作。
 * @return 无。
 * @note 进入即开始播放；OK 重播，BACK 停止并退出。
 */
static void menu_user_melody(menu_action_enum action)
{
  if (!melody_page_entered) {
    melody_page_entered = true;
    buzzer_init();
    menu_user_melody_start();
    return;
  }

  switch (action) {
    case MENU_ACTION_OK:
    case MENU_ACTION_OK_LONG:
      menu_user_melody_start();
      break;

    case MENU_ACTION_BACK:
    case MENU_ACTION_BACK_LONG:
      buzzer_off();
      melody_playing = false;
      melody_page_entered = false;
      menu_exit_function();
      return;

    default:
      break;
  }
}

/**
 * @brief 旋律页周期回调：按每个音符的时长推进播放。
 * @return 无。
 */
static void menu_user_melody_poll(void)
{
  uint32_t now;
  uint32_t duration;
  uint32_t elapsed;

  if (!melody_playing) {
    return;
  }
  now = HAL_GetTick();
  duration = MELODY_WHOLE_MS / (uint32_t)melody_star[melody_index].divider;
  elapsed = (uint32_t)(now - melody_note_start);

  if (elapsed < duration) {
    /* 每个音符只发 90% 时长，留 10% 静音，避免连音发糊。 */
    if ((!melody_in_gap) && (elapsed >= (duration * 9U / 10U))) {
      buzzer_off();
      melody_in_gap = true;
    }
    return;
  }

  ++melody_index;
  if (melody_index >= MELODY_STAR_COUNT) {
    melody_playing = false;
    buzzer_off();
    menu_view_user_melody_14_refresh(
        note_name_from_freq(melody_star[MELODY_STAR_COUNT - 1U].freq_hz),
        MELODY_STAR_COUNT, MELODY_STAR_COUNT, false);
    menu_request_refresh();
    return;
  }

  melody_note_start += duration;
  melody_in_gap = false;
  buzzer_tone(melody_star[melody_index].freq_hz);
  menu_view_user_melody_14_refresh(
      note_name_from_freq(melody_star[melody_index].freq_hz),
      melody_index + 1U, MELODY_STAR_COUNT, true);
  menu_request_refresh();
}
