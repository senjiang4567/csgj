#include "sensor_group.h"

#define SENSOR_ID_SAMPLE_COUNT (32u)
#define ADC_FULL_SCALE_12BIT   (4095.0f)

/*
 * 软延时循环系数：与主频相关。
 * 如延时不准，可在项目中重定义该宏进行修正。
 */
#ifndef SENSOR_DELAY_LOOP_PER_MS
#define SENSOR_DELAY_LOOP_PER_MS (4800u)
#endif

static ADC_HandleTypeDef *s_hadc = NULL;

static SensorGroup_Calib_t s_calib = {
    .vref_voltage_v = 3.3f,
    .k_zero_offset_v = 0.0f,
    .k_slope_v_per_c = 0.005f,
    .pt1000_excitation_a = 250e-6f,
    .pt1000_r0_ohm = 1000.0f,
    .pt1000_alpha = 0.00385f,
    .pt1000_lead_res_ohm = 0.0f,
    .id_threshold = 2048u,
};

/* ============================== GPIO简易封装 ============================== */
static void sensor_id_en_set(uint8_t high)
{
    HAL_GPIO_WritePin(SENSOR_ID_EN_GPIO_PORT,
                      SENSOR_ID_EN_PIN,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void sensor_sel_set(uint8_t high)
{
    HAL_GPIO_WritePin(SENSOR_SEL_GPIO_PORT,
                      SENSOR_SEL_PIN,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================== 非系统延时实现 ============================== */
static void sensor_delay_ms(uint32_t ms)
{
    volatile uint32_t i = 0;
    volatile uint32_t total = ms * SENSOR_DELAY_LOOP_PER_MS;

    for (i = 0; i < total; i++) {
        __NOP();
    }
}

static HAL_StatusTypeDef adc_read_single_channel(uint32_t channel, uint16_t *raw)
{
    ADC_ChannelConfTypeDef cfg = {0};

    if ((s_hadc == NULL) || (raw == NULL)) {
        return HAL_ERROR;
    }

    cfg.Channel = channel;
    cfg.Rank = ADC_RANK_CHANNEL_NUMBER;
    cfg.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;

    if (HAL_ADC_ConfigChannel(s_hadc, &cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    if (HAL_ADC_Start(s_hadc) != HAL_OK) {
        return HAL_ERROR;
    }

    if (HAL_ADC_PollForConversion(s_hadc, 10) != HAL_OK) {
        (void)HAL_ADC_Stop(s_hadc);
        return HAL_ERROR;
    }

    *raw = (uint16_t)HAL_ADC_GetValue(s_hadc);

    if (HAL_ADC_Stop(s_hadc) != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef read_adc_id_average(uint16_t *adc_id_avg)
{
    uint32_t i = 0;
    uint32_t sum = 0;
    uint16_t sample = 0;

    if (adc_id_avg == NULL) {
        return HAL_ERROR;
    }

    /* 识别使能：ID_EN = 1，延时5ms */
    sensor_id_en_set(1u);
    sensor_delay_ms(5u);

    for (i = 0; i < SENSOR_ID_SAMPLE_COUNT; i++) {
        if (adc_read_single_channel(SENSOR_ADC_ID_CHANNEL, &sample) != HAL_OK) {
            sensor_id_en_set(0u);
            return HAL_ERROR;
        }
        sum += sample;
    }

    /* 关闭识别使能：ID_EN = 0 */
    sensor_id_en_set(0u);

    *adc_id_avg = (uint16_t)(sum / SENSOR_ID_SAMPLE_COUNT);
    return HAL_OK;
}

HAL_StatusTypeDef SensorGroup_Init(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL) {
        return HAL_ERROR;
    }

    s_hadc = hadc;

    /* 默认关闭识别使能 */
    sensor_id_en_set(0u);

    return HAL_OK;
}

void SensorGroup_SetCalib(const SensorGroup_Calib_t *calib)
{
    if (calib != NULL) {
        s_calib = *calib;
    }
}

void SensorGroup_GetCalib(SensorGroup_Calib_t *calib_out)
{
    if (calib_out != NULL) {
        *calib_out = s_calib;
    }
}

HAL_StatusTypeDef SensorGroup_DetectAndRead(SensorGroup_Result_t *out)
{
    uint16_t adc_id_avg = 0;
    uint16_t raw = 0;

    if (out == NULL) {
        return HAL_ERROR;
    }

    out->probe_type = SENSOR_PROBE_UNKNOWN;
    out->adc_id_avg = 0;
    out->adc_rtd_raw = 0;
    out->adc_tc_raw = 0;
    out->pt1000_voltage_v = 0.0f;
    out->pt1000_resistance_ohm = 0.0f;
    out->pt1000_temp_c = 0.0f;
    out->k_voltage_v = 0.0f;
    out->k_temp_c = 0.0f;

    if (read_adc_id_average(&adc_id_avg) != HAL_OK) {
        return HAL_ERROR;
    }

    out->adc_id_avg = adc_id_avg;

    /* 判断探头类型 */
    if (adc_id_avg > s_calib.id_threshold) {
        /* 判定PT1000：SEL_SENSOR = 0，延时20ms，读取ADC_RTD */
        out->probe_type = SENSOR_PROBE_PT1000;
        sensor_sel_set(0u);
        sensor_delay_ms(20u);

        if (adc_read_single_channel(SENSOR_ADC_RTD_CHANNEL, &raw) != HAL_OK) {
            return HAL_ERROR;
        }

        out->adc_rtd_raw = raw;
        out->pt1000_voltage_v = ((float)raw / ADC_FULL_SCALE_12BIT) * s_calib.vref_voltage_v;

        if (s_calib.pt1000_excitation_a > 0.0f) {
            out->pt1000_resistance_ohm = out->pt1000_voltage_v / s_calib.pt1000_excitation_a;
            out->pt1000_resistance_ohm -= s_calib.pt1000_lead_res_ohm;
        }

        if ((s_calib.pt1000_alpha > 0.0f) && (s_calib.pt1000_r0_ohm > 0.0f)) {
            out->pt1000_temp_c = (out->pt1000_resistance_ohm - s_calib.pt1000_r0_ohm) /
                                 (s_calib.pt1000_alpha * s_calib.pt1000_r0_ohm);
        }
    } else {
        /* 判定K热电偶：SEL_SENSOR = 1，延时50ms，读取ADC_TC */
        out->probe_type = SENSOR_PROBE_K_TC;
        sensor_sel_set(1u);
        sensor_delay_ms(50u);

        if (adc_read_single_channel(SENSOR_ADC_TC_CHANNEL, &raw) != HAL_OK) {
            return HAL_ERROR;
        }

        out->adc_tc_raw = raw;
        out->k_voltage_v = ((float)raw / ADC_FULL_SCALE_12BIT) * s_calib.vref_voltage_v;

        if (s_calib.k_slope_v_per_c > 0.0f) {
            out->k_temp_c = (out->k_voltage_v - s_calib.k_zero_offset_v) / s_calib.k_slope_v_per_c;
        }
    }

    return HAL_OK;
}
