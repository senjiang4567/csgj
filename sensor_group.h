#ifndef SENSOR_GROUP_H
#define SENSOR_GROUP_H

#include "stm32f0xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ========================= 用户需按项目实际修改的宏定义 =========================
 * 说明：
 * 1) 为减少指针用量，GPIO 与 ADC 通道均使用编译期宏，不再通过结构体传入。
 * 2) 若你的工程已有 board.h / main.h，可将这些宏迁移到公共配置头文件。
 */
#ifndef SENSOR_ID_EN_GPIO_PORT
#define SENSOR_ID_EN_GPIO_PORT GPIOA
#endif

#ifndef SENSOR_ID_EN_PIN
#define SENSOR_ID_EN_PIN GPIO_PIN_0
#endif

#ifndef SENSOR_SEL_GPIO_PORT
#define SENSOR_SEL_GPIO_PORT GPIOA
#endif

#ifndef SENSOR_SEL_PIN
#define SENSOR_SEL_PIN GPIO_PIN_1
#endif

#ifndef SENSOR_ADC_ID_CHANNEL
#define SENSOR_ADC_ID_CHANNEL ADC_CHANNEL_0
#endif

#ifndef SENSOR_ADC_RTD_CHANNEL
#define SENSOR_ADC_RTD_CHANNEL ADC_CHANNEL_1
#endif

#ifndef SENSOR_ADC_TC_CHANNEL
#define SENSOR_ADC_TC_CHANNEL ADC_CHANNEL_2
#endif

typedef enum
{
    SENSOR_PROBE_UNKNOWN = 0,
    SENSOR_PROBE_PT1000,
    SENSOR_PROBE_K_TC
} SensorProbeType_t;

/* 检测组输出结果 */
typedef struct
{
    SensorProbeType_t probe_type;

    uint16_t adc_id_avg;
    uint16_t adc_rtd_raw;
    uint16_t adc_tc_raw;

    float pt1000_voltage_v;
    float pt1000_resistance_ohm;
    float pt1000_temp_c;

    float k_voltage_v;
    float k_temp_c;
} SensorGroup_Result_t;

/* 运行时校准参数 */
typedef struct
{
    float vref_voltage_v;

    float k_zero_offset_v;
    float k_slope_v_per_c;

    float pt1000_excitation_a;
    float pt1000_r0_ohm;
    float pt1000_alpha;
    float pt1000_lead_res_ohm;   /* 预留线阻补偿 */

    uint16_t id_threshold;
} SensorGroup_Calib_t;

/*
 * @brief 初始化检测组（仅需传入 ADC 句柄）
 */
HAL_StatusTypeDef SensorGroup_Init(ADC_HandleTypeDef *hadc);

/*
 * @brief 执行一次完整“识别 + 读取”流程
 */
HAL_StatusTypeDef SensorGroup_DetectAndRead(SensorGroup_Result_t *out);

void SensorGroup_SetCalib(const SensorGroup_Calib_t *calib);
void SensorGroup_GetCalib(SensorGroup_Calib_t *calib_out);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_GROUP_H */
