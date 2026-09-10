#include "zf_driver_rtc.h"

/** @brief RTC 句柄，由本模块统一持有。 */
static RTC_HandleTypeDef s_rtc;

/**
 * @brief 把日期换算为自 2000-01-01 起的天数。
 * @param year 年份偏移（0 表示 2000 年）。
 * @param month 月份，1~12。
 * @param date 日，1~31。
 * @return 天数。
 */
static uint32_t rtc_day_count(uint8_t year, uint8_t month, uint8_t date)
{
  static const uint16_t month_cumulative[12] = {
    0U, 31U, 59U, 90U, 120U, 151U, 181U, 212U, 243U, 273U, 304U, 334U
  };
  uint32_t full_year = 2000U + year;
  uint32_t days;

  if ((month == 0U) || (month > 12U)) {
    return 0U;
  }
  days = (full_year - 2000U) * 365U + month_cumulative[month - 1U];
  if ((month > 2U) &&
      (((full_year % 4U) == 0U) || ((full_year % 400U) == 0U))) {
    days += 1U;
  }
  return days + (uint32_t)(date - 1U);
}

zf_status_t rtc_init(void)
{
  RCC_OscInitTypeDef osc_init = {0};
  RCC_PeriphCLKInitTypeDef periph_init = {0};
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  uint32_t async_prediv;
  uint32_t sync_prediv;

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  /* 优先 LSE：32768 / 128 / 256 = 1 Hz；失败回退 LSI：32000 / 128 / 250 = 1 Hz。 */
  osc_init.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  osc_init.LSEState = RCC_LSE_ON;
  osc_init.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&osc_init) == HAL_OK) {
    periph_init.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    async_prediv = 127U;
    sync_prediv = 255U;
  } else {
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    osc_init.LSIState = RCC_LSI_ON;
    osc_init.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc_init) != HAL_OK) {
      return ZF_ERROR;
    }
    periph_init.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    async_prediv = 127U;
    sync_prediv = 249U;
  }

  periph_init.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  if (HAL_RCCEx_PeriphCLKConfig(&periph_init) != HAL_OK) {
    return ZF_ERROR;
  }

  /* HAL_RTC_Init 不会打开 RTC 总线时钟，必须显式使能，否则 RTC 不计数。 */
  __HAL_RCC_RTC_ENABLE();

  s_rtc.Instance = RTC;
  s_rtc.Init.HourFormat = RTC_HOURFORMAT_24;
  s_rtc.Init.AsynchPrediv = async_prediv;
  s_rtc.Init.SynchPrediv = sync_prediv;
  s_rtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  s_rtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  s_rtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&s_rtc) != HAL_OK) {
    return ZF_ERROR;
  }

  /* 固定起始基准 2000-01-01 00:00:00。 */
  date.Year = 0U;
  date.Month = RTC_MONTH_JANUARY;
  date.Date = 1U;
  date.WeekDay = RTC_WEEKDAY_SATURDAY;
  if (HAL_RTC_SetDate(&s_rtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
    return ZF_ERROR;
  }
  time.Hours = 0U;
  time.Minutes = 0U;
  time.Seconds = 0U;
  time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  time.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&s_rtc, &time, RTC_FORMAT_BIN) != HAL_OK) {
    return ZF_ERROR;
  }
  return ZF_OK;
}

zf_status_t rtc_get_time(rtc_time_t *time)
{
  RTC_TimeTypeDef rtc_time = {0};
  RTC_DateTypeDef rtc_date = {0};

  if (time == NULL) {
    return ZF_INVALID_PARAM;
  }
  /* 必须先读时间再读日期，HAL 借此解锁影子寄存器。 */
  if (HAL_RTC_GetTime(&s_rtc, &rtc_time, RTC_FORMAT_BIN) != HAL_OK) {
    return ZF_ERROR;
  }
  if (HAL_RTC_GetDate(&s_rtc, &rtc_date, RTC_FORMAT_BIN) != HAL_OK) {
    return ZF_ERROR;
  }
  time->hour = (uint8_t)rtc_time.Hours;
  time->minute = (uint8_t)rtc_time.Minutes;
  time->second = (uint8_t)rtc_time.Seconds;
  return ZF_OK;
}

uint32_t rtc_get_seconds(void)
{
  RTC_TimeTypeDef rtc_time = {0};
  RTC_DateTypeDef rtc_date = {0};

  if (HAL_RTC_GetTime(&s_rtc, &rtc_time, RTC_FORMAT_BIN) != HAL_OK) {
    return 0U;
  }
  if (HAL_RTC_GetDate(&s_rtc, &rtc_date, RTC_FORMAT_BIN) != HAL_OK) {
    return 0U;
  }
  return rtc_day_count((uint8_t)rtc_date.Year, (uint8_t)rtc_date.Month,
                       (uint8_t)rtc_date.Date) *
             86400U +
         (uint32_t)rtc_time.Hours * 3600U +
         (uint32_t)rtc_time.Minutes * 60U + (uint32_t)rtc_time.Seconds;
}
