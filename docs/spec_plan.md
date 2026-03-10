# 传感器识别与测量工程说明（Spec + Plan）

## 1. 项目目标

在 STM32F030C8T6 平台上实现一个统一检测组，完成以下能力：

- 对探头类型进行自动识别（PT1000 / K 型热电偶）。
- 按识别结果切换通道并采集 ADC。
- 输出可直接使用的电压、阻值、温度结果。
- 提供可配置校准参数（含 PT1000 线阻补偿预留）。

---

## 2. 需求规格（Specification）

### 2.1 功能需求

1. 识别流程（固定时序）：
   - `ID_EN = 1`
   - 延时 `5 ms`
   - 读取 `ADC_ID` 共 `32` 次并取平均
   - `ID_EN = 0`
2. 判型逻辑：
   - 若 `ADC_ID_avg > 阈值`，判定为 **PT1000**
   - 若 `ADC_ID_avg <= 阈值`，判定为 **K 型热电偶**
3. PT1000 分支：
   - `SEL_SENSOR = 0`
   - 延时 `20 ms`
   - 读取 `ADC_RTD`
4. K 热电偶分支：
   - `SEL_SENSOR = 1`
   - 延时 `50 ms`
   - 读取 `ADC_TC`

### 2.2 电气与换算需求

- PT1000 激励电流默认值：`250 uA`。
- PT1000 预留线阻补偿：`pt1000_lead_res_ohm`。
- ADC 量化基准：12-bit（`0~4095`）。
- 电压换算：

```c
voltage_v = (adc_raw / 4095.0f) * vref_voltage_v;
```

- PT1000 阻值换算：

```c
r_pt1000 = voltage_v / pt1000_excitation_a - pt1000_lead_res_ohm;
```

- PT1000 温度（线性近似）：

```c
t_pt1000 = (r_pt1000 - pt1000_r0_ohm) / (pt1000_alpha * pt1000_r0_ohm);
```

- K 热电偶温度（线性标定）：

```c
t_k = (k_voltage_v - k_zero_offset_v) / k_slope_v_per_c;
```

### 2.3 软件接口需求

- 仅使用 `.c/.h` 形式提供模块。
- 对外接口：
  - `SensorGroup_Init(...)`
  - `SensorGroup_DetectAndRead(...)`
  - `SensorGroup_SetCalib(...)`
  - `SensorGroup_GetCalib(...)`
- GPIO 控制采用简易封装，减少运行时指针传参。
- 不使用系统延时（如 `HAL_Delay`），使用本地忙等待延时实现。

### 2.4 配置需求

采用编译期宏完成硬件映射（可在项目中重定义）：

| 类别 | 宏名 | 默认值示例 |
|---|---|---|
| ID 使能引脚 | `SENSOR_ID_EN_GPIO_PORT` / `SENSOR_ID_EN_PIN` | `GPIOA` / `GPIO_PIN_0` |
| 传感器选择引脚 | `SENSOR_SEL_GPIO_PORT` / `SENSOR_SEL_PIN` | `GPIOA` / `GPIO_PIN_1` |
| ADC 通道 | `SENSOR_ADC_ID_CHANNEL` | `ADC_CHANNEL_0` |
| ADC 通道 | `SENSOR_ADC_RTD_CHANNEL` | `ADC_CHANNEL_1` |
| ADC 通道 | `SENSOR_ADC_TC_CHANNEL` | `ADC_CHANNEL_2` |
| 软延时系数 | `SENSOR_DELAY_LOOP_PER_MS` | `4800` |

---

## 3. 执行计划（Plan）

### 3.1 开发分解

- [x] 定义统一数据结构（识别结果 + 校准参数）。
- [x] 实现 GPIO 简易封装（`ID_EN`、`SEL_SENSOR`）。
- [x] 实现单通道 ADC 读取与 ID 多次平均。
- [x] 实现识别分支流程与固定时序。
- [x] 实现 PT1000 / K 热电偶换算逻辑。
- [x] 实现参数读写接口。
- [x] 添加中文注释并整理输出文档。

### 3.2 调用流程（建议放在主循环）

```c
SensorGroup_Result_t result;

while (1) {
    if (SensorGroup_DetectAndRead(&result) == HAL_OK) {
        // 根据 result.probe_type 使用对应结果
        // PT1000: result.pt1000_temp_c / result.pt1000_resistance_ohm
        // K热电偶: result.k_temp_c / result.k_voltage_v
    }

    // 其余业务逻辑...
}
```

### 3.3 风险与注意事项

1. 忙等待延时会占用 CPU；若主循环周期要求高，建议后续改为定时器节拍调度。
2. `SENSOR_DELAY_LOOP_PER_MS` 受系统主频与编译优化影响，需要实测校准。
3. K 热电偶当前为线性模型，仅适合简化场景；高精度建议引入分段多项式与冷端补偿。
4. PT1000 当前为线性近似，温区较宽时建议采用 Callendar–Van Dusen 模型。

### 3.4 验证计划

| 验证项 | 方法 | 通过标准 |
|---|---|---|
| ID 识别时序 | 示波器抓 `ID_EN` + ADC 触发 | 满足 5ms + 32 次采样 |
| PT1000 分支 | 设置 `ADC_ID_avg > 阈值` 条件 | `SEL_SENSOR=0` 且读取 RTD |
| K 热电偶分支 | 设置 `ADC_ID_avg <= 阈值` 条件 | `SEL_SENSOR=1` 且读取 TC |
| 线阻补偿 | 调整 `pt1000_lead_res_ohm` | 温度输出按预期偏移 |
| 标定参数生效 | 调整斜率/零点/阈值 | 输出变化与参数一致 |

---

## 4. PDF 导出友好建议

- 使用标准 Markdown 编辑器（Typora、VS Code Markdown PDF、Pandoc）可直接导出。
- 文档已使用：
  - 标题层级（`# ## ###`）
  - 列表（有序/无序）
  - 代码块（```c）
  - 表格（需求与验证矩阵）

