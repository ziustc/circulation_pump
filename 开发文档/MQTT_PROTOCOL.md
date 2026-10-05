# 循环泵 MQTT 通信协议

> 依据 [`include/mqtt2.h`](include/mqtt2.h)、[`src/mqtt2.cpp`](src/mqtt2.cpp) 整理，
> 相关常量见 [`include/common.h`](include/common.h)、[`include/ssid.h`](include/ssid.h)。

---

## 1. 总体结构

代码分两层：

| 层 | 类 | 职责 |
| --- | --- | --- |
| 传输层 | `MqttCore` | 封装 PubSubClient：连接/重连、订阅、发布、HA 自动发现报文组装、接收分发与自回环过滤 |
| 业务层 | `MqttManager` | 定义 topic 与实体 ID、生成设置/状态报文、解析指令、通过回调把数据交给 PCU |

数据流：

```
Home Assistant ──cmd──▶ hass/circ_pump/cmd
                              │
                              ▼
                    MqttCore::onReceive   (反序列化 + sender 自回环过滤)
                              │
                              ▼
                    MqttManager::AnalyzeMsg
                        ├── handleCmd   ──▶ onCmd_cb / onSwitch_cb ──▶ PumpCtrlUnit
                        └── handleState ──▶ onState_cb             ──▶ PumpCtrlUnit

PumpCtrlUnit ──sendSettings/sendState──▶ MqttManager ──▶ MqttCore::sendState
                                                              │
                                                              ▼
                                                    hass/circ_pump/state ──▶ HA

设备 ──retained──▶ homeassistant/{type}/{entityId}/config ──▶ HA 自动创建实体
```

---

## 2. 连接参数

| 项目 | 值 | 出处 |
| --- | --- | --- |
| Client ID | `circulation_pump` | `HOSTNAME`（`ssid.h`） |
| Broker | `192.168.0.20` | `MQTT_SERVER` |
| 端口 | `1883` | `MQTT_PORT` |
| 报文缓冲 | 1024 字节 | `MqttInitParams_t::bufSize`，`setBufferSize()` |
| 订阅 | `hass/circ_pump/#` | `reconnect()` 中按 `id_abbr` 拼接 |
| QoS | 0（PubSubClient 默认） | — |
| 遗嘱 / LWT | 未设置 | — |
| Keep Alive | 15 s（库默认） | — |

**重连策略**：`init()` 中连接一次；`loop()` 每轮检查 `_client.connected()`，未连接即调用 `reconnect()`。
失败时仅打印 `connect failed, rc=%d`，**没有退避与超时控制**，因此掉线期间每个主循环都会尝试重连一次。

> ⚠️ 设备**不订阅** `homeassistant/#`，只订阅 `hass/circ_pump/#`。发现报文是单向发往 HA 的。

---

## 3. Topic 一览

| Topic | 方向 | retained | 说明 |
| --- | --- | --- | --- |
| `hass/circ_pump/state` | 设备 → HA（上报）<br>对端 → 设备（接收） | 否 | 设置与状态共用同一 topic；双向复用 |
| `hass/circ_pump/cmd` | HA → 设备 | 否 | 全部控制指令入口 |
| `homeassistant/number/{entityId}/config` | 设备 → HA | **是** | 数值型实体自动发现 |
| `homeassistant/switch/{entityId}/config` | 设备 → HA | **是** | 开关型实体自动发现 |
| `homeassistant/time/{entityId}/config` | 设备 → HA | **是** | 时间型实体自动发现 |
| `homeassistant/sensor/{entityId}/config` | 设备 → HA | **是** | 传感器实体自动发现 |

其中 `circ_pump` 来自 `DeviceInfo_t::id_abbr`，是设备统一的 topic 前缀。

---

## 4. 报文通用规则

1. **格式**：所有 payload 均为 JSON 对象，缓冲区 1024 字节。
2. **发送方标识**：`MqttCore::sendState()` 发布前会自动注入 `"sender": "<自己的 clientId>"`。
3. **自回环过滤**：`MqttCore::onReceive()` 中，若 `sender` 字段存在且等于自身 clientId，**直接丢弃该报文**。
   这是必要的——设备订阅了 `hass/circ_pump/#`，会收到自己刚发布的 `state`。
4. **解析失败**：`deserializeJson` 出错时打印日志并丢弃，不回调上层。
5. **空字段容忍**：`icon`、`unit` 等为空字符串时不会写入发现报文。

---

## 5. 指令处理规则（`hass/circ_pump/cmd`）

### 5.1 核心约束：**一条报文只处理一个字段**

`handleCmd()` 按固定优先级自上而下匹配，**命中第一个可用字段后立即 `return`**，
同一条报文中的其余字段会被忽略。因此 HA 侧每次下发都必须只包含一个待修改的字段。

### 5.2 优先级与解析规则

| 序 | 字段 | JSON 类型 | 取值格式 | 解析方式 | 回调 | `SettingsRev_t` |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `pump_s_on` | string | `"ON"` / `"OFF"`（其它字符串按 `OFF` 处理） | 字符串比较 | `onSwitch_cb(bool)` | — |
| 2 | `pump_st_1`…`pump_st_3` | string | `"HH:MM:SS"` | `sscanf("%d:%d:%d")`，**必须 3 段** | `onCmd_cb` | `START_TIME_1`+i |
| 3 | `pump_et_1`…`pump_et_3` | string | `"HH:MM:SS"` | 同上 | `onCmd_cb` | `END_TIME_1`+i |
| 4 | `pump_keept` | string | `"ON"` / `"OFF"` | 字符串比较 | `onCmd_cb` | `HEAT_KEEP_ENABLED` |
| 5 | `pump_wmin` | int | 整数 | `.as<int>()` | `onCmd_cb` | `WATER_MIN_SEC` |
| 6 | `pump_wmax` | int | 整数 | `.as<int>()` | `onCmd_cb` | `WATER_MAX_SEC` |
| 7 | `pump_dur` | int | 整数 | `.as<int>()` | `onCmd_cb` | `PUMP_ON_DURATION` |
| 8 | `pump_demandt` | int | 整数 | `.as<int>()` | `onCmd_cb` | `DEMAND_TEMP` |

注意三个 time 字段在循环中**逐组检查**（先 `st_i` 再 `et_i`），因此 `pump_st_1` 优先级高于 `pump_et_1`，而 `pump_et_1` 高于 `pump_st_2`。

### 5.3 回调语义

```cpp
typedef void (*OnCmdCB_t)(Settings_t set, SettingsRev_t revisedField);
```

- `set` 是**默认构造**的 `Settings_t`（时间字段为 `-1`，`heatKeepEnabled = true`），**只有 `revisedField` 指明的字段是有效值**。
- PCU 侧 `onMqttUpdate()` 用 `switch(revisedField)` 只把对应字段并入 `_settings`，其余一律忽略。
- 处理完毕后 PCU 会 `importSettings → exportSettings`（越界值会在屏幕层被夹紧），随后**回传一次 `sendSettings()` 作为确认**并写入 NVS。
- 开关泵走独立回调 `OnSwitchCB_t(bool)`，不经过 `Settings_t`。

---

## 6. 实体清单

`fullDevInfo` 列表示该条发现报文是否携带完整 `device` 信息（见 §7.3）。

### 6.1 可下发设置（cmd + state）

| entity_id | 名称 | 类型 | 取值 | 单位/范围 | 图标 | fullDevInfo |
| --- | --- | --- | --- | --- | --- | --- |
| `pump_s_on` | 启动 | switch | `ON` / `OFF` | — | `mdi:power-cycle` | ✅ 首个 |
| `pump_wmin` | 水控最短时间 | number | 整数 | 秒，1–9，步长 1 | `mdi:clock-start` | ❌ |
| `pump_wmax` | 水控最长时间 | number | 整数 | 秒，1–9，步长 1 | `mdi:clock-end` | ❌ |
| `pump_dur` | 单次开泵时长 | number | 整数 | 分，1–9，步长 1 | `mdi:timer-play` | ❌ |
| `pump_demandt` | 保温温度 | number | 整数 | °C，20–45，步长 1 | `mdi:thermometer` | ❌ |
| `pump_st_1..3` | 保温N起始时间 | time | `HH:MM` | `has_date=false`,`has_time=true` | `mdi:calendar-clock` | ❌ |
| `pump_et_1..3` | 保温N结束时间 | time | `HH:MM` | 同上 | `mdi:calendar-clock` | ❌ |
| `pump_keept` | 保温 | switch | `ON` / `OFF` | — | `mdi:sun-thermometer` | ❌ |

### 6.2 只上报传感器（state only）

| entity_id | 名称 | 类型 | 单位 | state_class | 图标 |
| --- | --- | --- | --- | --- | --- |
| `pump_s_wt` | 当前水温-泵体 | sensor | °C | `measurement` | `mdi:water-thermometer` |
| `pump_s_flow` | 当前流量 | sensor | L/min | `measurement` | `mdi:waves-arrow-right` |
| `pump_s_wifi` | WiFi强度-泵体 | sensor | dBm | `measurement` | `mdi:wifi` |

### 6.3 只接收字段（无发现报文）

| 字段 | 含义 | 来源 |
| --- | --- | --- |
| `pump_s_ewt` | 用水端水温 | 由**对端设备**发布到 `hass/circ_pump/state`，本设备 `handleState()` 接收 |

> 📌 `pump_s_ewt` **没有对应的 Discovery 报文**，因此在本设备的 HA 设备卡片上不会出现这个实体；
> 它只是本设备从 state topic 上"蹭"读的对端数据，最终仅用于刷新屏幕与温控判断。

---

## 7. HA 自动发现（Discovery）

### 7.1 公共字段（`sendDiscoveryBase`）

每条发现报文都会写入：

| Key | 值 |
| --- | --- |
| `name` | 实体中文名 |
| `unique_id` | `entityId` |
| `icon` | 图标（非空时） |
| `stat_t` | `hass/circ_pump/state` |
| `cmd_t` | `hass/circ_pump/cmd` |
| `val_tpl` | `{{ value_json.<entityId> }}` |
| `cmd_tpl` | `{"<entityId>": {{ value }} }` |

随后叠加各类型的专用属性（`attr`），**attr 中的同名 key 会覆盖公共字段**——switch 与 time 正是靠这一点改写 `cmd_tpl` / `val_tpl`。

### 7.2 各类型专用属性

| 类型 | 追加/覆盖的属性 |
| --- | --- |
| `number` | `unit`、`min`、`max`、`step` |
| `switch` | `pl_on="ON"`、`pl_off="OFF"`、`cmd_tpl={"<id>": "{{ value }}"}` |
| `time` | `has_date`、`has_time`、`val_tpl={{ value_json.<id> }}:00`、`cmd_tpl={"<id>": "{{ value }}"}` |
| `sensor` | `stat_cla`、`unit_of_meas` |

**value/cmd 模板的差异是协议的关键一环：**

- number：`cmd_tpl` 输出**数字** → `{"pump_dur": 3}`
- switch / time：`cmd_tpl` 输出**带引号的字符串** → `{"pump_keept": "ON"}`、`{"pump_et_1": "08:30:00"}`
- time：`val_tpl` 在设备上报的 `HH:MM` 后**补 `:00`**，因为 HA 时间实体需要 `HH:MM:SS`

### 7.3 设备信息节流（`fullDevInfo`）

- 第一条（`pump_s_on`）携带完整 `device` 块：`identifiers`、`name`、`manufacturer`、`model`、`sw_version`、`hw_version`、`via_device`。
- 其余报文只发 `"dev": {"identifiers": ["circulation_pump"]}`，用于节省流量（HA 的缩写键 `dev` 即 `device`）。

设备信息取值：

| 字段 | 值 |
| --- | --- |
| `identifiers[0]` | `circulation_pump` |
| `name` | `循环泵` |
| `manufacturer` | `ZiUtility` |
| `model` | `ESP32-S3-Pump` |
| `sw_version` | `SW_VERSION`（`version.h`，编译时自增） |
| `hw_version` | `1.0.0` |
| `via_device` | `""`（空字符串） |

---

## 8. 上报报文（`hass/circ_pump/state`）

设置与状态**共用同一个 topic**，两者的字段集合互不重叠，HA 侧靠 `value_json.<id>` 各取所需。

### 8.1 设置上报（`sendSettings`）

| 字段 | 格式 | 说明 |
| --- | --- | --- |
| `pump_st_1..3` / `pump_et_1..3` | `"HH:MM"` | `snprintf("%02d:%02d")` |
| `pump_keept` | `"ON"` / `"OFF"` | 布尔转字符串 |
| `pump_wmin` / `pump_wmax` / `pump_dur` / `pump_demandt` | 整数 | 直接写入 |

### 8.2 状态上报（`sendState`）

| 字段 | 处理 | 精度 |
| --- | --- | --- |
| `pump_s_wt` | `(int)(tempC * 10) / 10.0f` | 1 位小数 |
| `pump_s_flow` | `(int)(flow * 100) / 100.0f` | 2 位小数 |
| `pump_s_wifi` | `WiFi.RSSI()` | 整数（负值，dBm） |
| `pump_s_on` | 布尔转 `"ON"` / `"OFF"` | — |

> 小数"圆整"是用 `(int)` **截断**实现的，不是四舍五入。

---

## 9. 接收规则（`AnalyzeMsg`）

收到报文后按 topic 精确匹配（`strcmp`）：

| Topic | 处理函数 | 行为 |
| --- | --- | --- |
| `hass/circ_pump/cmd` | `handleCmd` | 解析指令 → `onCmd_cb` / `onSwitch_cb` |
| `hass/circ_pump/state` | `handleState` | 只读取 `pump_s_ewt`（`is<float>()`）→ `onState_cb` |

`handleState()` 目前**仅处理 `pump_s_ewt` 一个字段**，且 `State_t::tempC2` 声明为 `int`，浮点值会被截断取整。

---

## 10. 时序

| 时机 | 动作 | 触发点 |
| --- | --- | --- |
| 上电初始化 | `sendDiscoveries()` + `sendSettings()` 各一次 | `PumpCtrlUnit::init()` |
| 周期 | 每 **10 秒**（`MQTT_STATE_FREQ`）`sendSettings()` + `sendState()` | `PumpCtrlUnit::loop()` |
| 事件 | 泵开关状态变化 → `sendState()` | `switchPump()` |
| 事件 | 屏幕改参数 / 收到 MQTT 指令 → `sendSettings()` | `onScreenUpdate()` / `onMqttUpdate()` |

> ⚠️ `MQTT_SETTING_FREQ`（10 分钟重发 Discovery）在 `common.h` 中有定义，但**当前代码中没有任何调用点**，
> Discovery 实际只在开机时发送一次。由于发现报文是 retained 的，HA 重启后仍能从 broker 取回配置；
> 但若 broker 未开启持久化并重启，实体将丢失，直到设备重新上电。

---

## 11. 完整报文示例

### 11.1 首条发现报文（switch，带完整设备信息，retained）

Topic：`homeassistant/switch/pump_s_on/config`

```json
{
  "name": "启动",
  "unique_id": "pump_s_on",
  "icon": "mdi:power-cycle",
  "stat_t": "hass/circ_pump/state",
  "cmd_t": "hass/circ_pump/cmd",
  "val_tpl": "{{ value_json.pump_s_on }}",
  "cmd_tpl": "{\"pump_s_on\": \"{{ value }}\"}",
  "pl_on": "ON",
  "pl_off": "OFF",
  "device": {
    "identifiers": ["circulation_pump"],
    "name": "循环泵",
    "manufacturer": "ZiUtility",
    "model": "ESP32-S3-Pump",
    "sw_version": "v0.8.040",
    "hw_version": "1.0.0",
    "via_device": ""
  }
}
```

### 11.2 后续发现报文（number / time / sensor，仅带 identifiers）

Topic：`homeassistant/number/pump_dur/config`

```json
{"name":"单次开泵时长","unique_id":"pump_dur","icon":"mdi:timer-play",
 "stat_t":"hass/circ_pump/state","cmd_t":"hass/circ_pump/cmd",
 "val_tpl":"{{ value_json.pump_dur }}","cmd_tpl":"{\"pump_dur\": {{ value }} }",
 "unit":"分","min":1,"max":9,"step":1,
 "dev":{"identifiers":["circulation_pump"]}}
```

Topic：`homeassistant/time/pump_et_1/config`

```json
{"name":"保温1结束时间","unique_id":"pump_et_1","icon":"mdi:calendar-clock",
 "stat_t":"hass/circ_pump/state","cmd_t":"hass/circ_pump/cmd",
 "val_tpl":"{{ value_json.pump_et_1 }}:00","cmd_tpl":"{\"pump_et_1\": \"{{ value }}\"}",
 "has_date":false,"has_time":true,
 "dev":{"identifiers":["circulation_pump"]}}
```

Topic：`homeassistant/sensor/pump_s_flow/config`

```json
{"name":"当前流量","unique_id":"pump_s_flow","icon":"mdi:waves-arrow-right",
 "stat_t":"hass/circ_pump/state","cmd_t":"hass/circ_pump/cmd",
 "val_tpl":"{{ value_json.pump_s_flow }}","cmd_tpl":"{\"pump_s_flow\": {{ value }} }",
 "stat_cla":"measurement","unit_of_meas":"L/min",
 "dev":{"identifiers":["circulation_pump"]}}
```

### 11.3 上报设置

Topic：`hass/circ_pump/state`（retained = false）

```json
{"pump_st_1":"08:00","pump_et_1":"08:30",
 "pump_st_2":"12:00","pump_et_2":"12:30",
 "pump_st_3":"00:00","pump_et_3":"00:00",
 "pump_keept":"ON","pump_wmin":2,"pump_wmax":6,"pump_dur":4,"pump_demandt":35,
 "sender":"circulation_pump"}
```

### 11.4 上报状态

```json
{"pump_s_wt":25.3,"pump_s_flow":4.12,"pump_s_wifi":-58,"pump_s_on":"OFF",
 "sender":"circulation_pump"}
```

### 11.5 下发指令（每条只含一个字段）

```json
{"pump_s_on": "ON"}
{"pump_keept": "OFF"}
{"pump_dur": 3}
{"pump_demandt": 35}
{"pump_wmin": 2}
{"pump_et_1": "08:30:00"}
```

### 11.6 对端上报用水端水温

```json
{"pump_s_ewt": 26.5, "sender": "peer_device"}
```

---

## 12. 注意事项与已知约束

### 12.1 协议侧

1. **一条 cmd 只能改一个字段**：`handleCmd` 命中即 `return`，多字段报文中除最高优先级字段外全部丢弃。
2. **时间指令必须是 `HH:MM:SS`**：`sscanf("%d:%d:%d")` 要求返回 3，`"08:30"` 会被静默丢弃。
   设备自身上报的是 `HH:MM`（配合 HA 模板补 `:00`），二者格式**不对称**。
3. **数值指令必须是 JSON 整数**：判定条件是 `is<int>()`。若 HA 送出 `5.0` 这类浮点 JSON（ArduinoJson 会解析为 float），`is<int>()` 为 false，整条指令被丢弃。
4. **`pump_s_on` 优先级最高**：与其它字段同报文时，只执行开关泵。
5. **非 `"ON"` 字符串一律按 `OFF` 处理**（`pump_s_on`、`pump_keept` 皆然）。
6. **`pump_s_ewt` 无实体**：接收正常但不会在 HA 中显示（无 Discovery 报文），且 `tempC2` 为 `int`，小数部分被截断。
7. **state topic 双向复用**：靠 `sender` 字段区分自产报文。若对端发布时**不带 `sender`**，本设备会把它当作有效数据；反之若对端 clientId 与本机相同，其报文会被误判为自回环而丢弃。

### 12.2 实现侧

1. **`unit` 不是 HA 的合法缩写键**：number 实体的单位按 HA 规范应写作 `unit_of_meas` 或 `unit_of_measurement`
   （`sensor` 分支用的就是 `unit_of_meas`）。当前 number 分支写入的是 `unit`，HA 大概率会忽略该键，
   导致 4 个 number 实体（`pump_wmin`/`pump_wmax`/`pump_dur`/`pump_demandt`）**不显示单位**。
2. **`via_device` 为空字符串**：设备块中仍会写出 `"via_device": ""`，属于无效引用，建议在为空时省略该键。
3. **sensor 也带了 `cmd_t` / `cmd_tpl`**：公共字段无差别写入，对只读传感器是冗余的（无害）。
4. **重连无退避**：断线时每个主循环都会重连一次，可能造成日志刷屏与无效连接尝试。
5. **缓冲区固定 1024**：`sendDiscoveryBase` 与 `sendState` 的本地 `payload[1024]` 与 MQTT 缓冲一致；
   首条带完整设备信息的发现报文约 400 字节，余量充足，但新增实体属性时需留意。
6. **`MQTT_SETTING_FREQ` 未被使用**（见 §10），Discovery 仅开机发送一次。
7. **单例指针**：`MqttCore::coreInstance` / `MqttManager::managerInstance` 在构造函数中赋值，
   静态回调依赖它们，因此**只允许各存在一个实例**。
