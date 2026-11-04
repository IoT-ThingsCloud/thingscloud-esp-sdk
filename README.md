# ThingsCloud ESP SDK

ThingsCloud IoT Platform WiFi and MQTT client library for ESP8266/ESP32 based boards using arduino platform.

---

## 📖 简介

ThingsCloud 推出了基于 ESP32/ESP8266 Arduino 框架的 SDK，帮助物联网硬件厂商和开发者轻松将设备接入 ThingsCloud 物联网平台。

### 🎯 为什么选择 ThingsCloud？

| 优势 | 说明 |
|------|------|
| 🚀 **快速接入** | 几行代码即可完成设备与云平台的双向通信，无需搭建服务器 |
| 💰 **免费使用** | 提供免费版项目，个人开发者和小型项目可零成本启动 |
| 🔧 **低代码平台** | 无需云端开发，通过可视化配置即可生成应用 SaaS 和用户 App |
| 📱 **开箱即用** | 内置设备管理、数据可视化、告警规则、用户权限等完整功能 |
| 🌐 **协议丰富** | 支持 MQTT、HTTP、TCP、Modbus、LoRa、Zigbee 等多种协议 |
| 🏭 **适合量产** | 一型一密、WiFi 配网、OTA 升级等功能完美支持设备量产 |

### 💡 适用于哪些场景？

- 🏠 **智能家居** - 灯光控制、环境监测、安防报警
- 🌾 **智慧农业** - 温室监控、灌溉控制、气象站
- 🏭 **工业物联** - 设备监控、数据采集、远程运维
- 🏥 **健康监护** - 远程医疗、养老监护、健康检测
- 🔋 **能源管理** - 电量监测、节能控制、能耗分析

---

## ✨ SDK 支持特性

| 特性 | 说明 |
|------|------|
| 📶 WiFi 基本连接 | 指定 WiFi SSID/Password，连接到固定 AP |
| 📱 WiFi 手机配网 | 使用 [ThingsX](https://www.thingscloud.xyz/docs/guide/customer-app/common-app.html) iOS/Android App 配网，无需预先写入 WiFi 信息，适合设备量产。支持 SoftAP（WiFi 热点）和 BLE（蓝牙）两种配置方式，均提供项目模式与厂商模式的示例代码。 |
| 🔐 MQTT 一机一密 | 为每个模组单独烧录 MQTT 证书 |
| 🔑 MQTT 一型一密 | 所有模组烧录相同固件，自动获取 MQTT 证书，适合设备量产 |
| 📡 完整协议支持 | 全面支持 [ThingsCloud MQTT 接入协议](https://www.thingscloud.xyz/docs/guide/connect-device/mqtt.html)，实现双向数据实时传输 |
| 🔗 自动重连 | 支持 MQTT 断线后自动重连 |
| 🔄 OTA 升级 | 支持固件 OTA 升级，结合 ThingsCloud 版本管理功能 |

## 🎯 支持模组型号

### ESP32 系列

| 型号 | 说明 |
|------|------|
| ESP32 | 双核、WiFi + BLE |
| ESP32-S2 | 低功耗、USB OTG |
| ESP32-S3 | AI 加速、USB OTG |
| ESP32-C3 | RISC-V、低成本 |
| ESP32-C5 | WiFi / BLE 配网，编译验证范围见下方配网模式参考 |
| ESP32-C6 | RISC-V、Zigbee/Thread（需安装 ESP32 开发板 v3.x.x） |

### ESP8266 系列

| 型号 | 说明 |
|------|------|
| ESP8266 | WiFi SoC |
| ESP8285 | 内置 1MB Flash |

---

## 📦 安装开发板依赖

SDK 基于 Arduino 框架，因此您需要先完成 Arduino 框架中 ESP32/ESP8266 的开发板依赖安装。

这里以 Arduino IDE 为例，进入 **文件 > 首选项**。

### ESP32 开发板

添加以下开发板仓库地址：
```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

### ESP8266 开发板

添加以下开发板仓库地址：
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

如下图：

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611101858_2180114cc1fea55dcdd37cce87e5ff3c.png)

然后在开发板管理器中，根据您的实际需要，搜索并安装 ESP32 或 ESP8266 开发板。

> 💡 原有 WiFi / MQTT 功能支持 ESP32 开发板 v2.x.x 和 v3.x.x。新增 BLE 配网还要求开发板包包含所需的配网协议与 Security1 组件，本次验证使用 Arduino-ESP32 3.3.8；不能将原有功能的版本支持范围直接视为 BLE 配网的支持范围。
> ![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611101202_80999e59ebfbc152360b35027a225247.png)

> 💡 ESP8266 开发板支持 v2.x.x 和 v3.x.x
> ![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611101336_032f553d8e401dabd72f729ba022f093.png)

---

## 💻 安装 SDK

更详细的安装方法可参考官网 [ThingsCloud ESP32/ESP8266 Arduino SDK](https://www.thingscloud.xyz/docs/tutorials/connect-device/esp32-arduino-sdk.html)

### 方法 1️⃣：Arduino 库管理器安装（推荐）

进入 Arduino 菜单的 `工具 > 管理库`，搜索 `ThingsCloud`，**选择最新版本**，点击安装。有版本更新时，需要在这里点击更新。

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611102233_59bc8eb4bed4c471c3d1203977d14d09.png)

### 方法 2️⃣：手动安装

下载代码仓库最新的 release 压缩包，解压缩后，将整个目录放置在 `Arduino\libraries` 中。

### 方法 3️⃣：PlatformIO

使用 VSCode + PlatformIO 开发方式，可以直接在 PlatformIO Library 中搜索并添加到项目中。

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2022/20230110235709_ae88b059b93a179e98945a207f6576f9.png)

---

## 📚 安装其它依赖库

SDK 正常运行需要以下依赖库：

| 依赖库 | 说明 | 链接 |
|--------|------|------|
| PubSubClient | MQTT 客户端 | [GitHub](https://github.com/knolleary/pubsubclient) |
| ArduinoJson | JSON 解析 | [GitHub](https://github.com/bblanchon/ArduinoJson) |

> ⚠️ 如使用 PlatformIO IDE，依赖库将自动安装。如使用 Arduino IDE，需要单独安装这些依赖库，方法同上。

---

## 🔧 ThingsCloud 准备工作

- 📖 [快速上手控制台](https://www.thingscloud.xyz/docs/guide/quickstart/signin-console.html)
- 🔑 [如何获得设备证书？](https://www.thingscloud.xyz/docs/guide/connect-device/device-certificate.html)
- 📡 [ThingsCloud MQTT 接入文档](https://www.thingscloud.xyz/docs/guide/connect-device/mqtt.html)

---

## 📂 示例代码

> 💡 如使用 Arduino IDE，可在 `文件 > 示例 > ThingsCloud_ESP_SDK` 中，直接打开以下示例代码。

### 🟢 01.WiFi_Basic - 基础 WiFi 连接

普通 WiFi 连接方式，指定 WiFi SSID 和密码。

| 示例名称 | 说明 | 适用场景 |
|----------|------|----------|
| [`mqtt_connect_with_access_token`](examples/01.WiFi_Basic/mqtt_connect_with_access_token/) | 每个设备使用自己的 AccessToken 连接 ThingsCloud | 开发调试、小批量设备 |
| [`mqtt_connect_with_device_key`](examples/01.WiFi_Basic/mqtt_connect_with_device_key/) | 每个设备使用 DeviceKey 请求 AccessToken，连接 ThingsCloud | 设备量产 |
| [`mqtt_connect_with_device_key_auto_create_device`](examples/01.WiFi_Basic/mqtt_connect_with_device_key_auto_create_device/) | 使用 DeviceKey 请求 AccessToken，如果设备不存在则自动创建 | 批量自动注册设备 |
| [`mqtt_connect_with_auto_device_key`](examples/01.WiFi_Basic/mqtt_connect_with_auto_device_key/) | 每个设备自动生成唯一的 DeviceKey，请求 AccessToken | 一型一密量产 |

---

### 📱 02.WiFi_Provisioning_AP_Mode - SoftAP 配网

设备创建 WiFi 热点，用户通过 ThingsX App 将目标 WiFi 信息发送给设备，无需在固件中预先写入路由器的 SSID 和密码。

| 示例名称 | 说明 | 功能特点 |
|----------|------|----------|
| [`wifi_provisioning_cliam_device`](examples/02.WiFi_Provisioning_AP_Mode/wifi_provisioning_cliam_device/) | SoftAP 项目模式，沿用已有示例名称 | 固件预设项目及设备类型，配网时领取设备；新增可配置按键长按清除 WiFi 功能 |
| [`wifi_provisioning_softap_manufacturer`](examples/02.WiFi_Provisioning_AP_Mode/wifi_provisioning_softap_manufacturer/) | 新增 SoftAP 厂商模式 | 固件预设 ProductKey，通过 App 下发的领取上下文获取并保存平台接入凭据 |
| [`wifi_provisioning_reset`](examples/02.WiFi_Provisioning_AP_Mode/wifi_provisioning_reset/) | 配网重置示例 | 通过长按按键重置配网信息 |

---

### 📡 03.MQTT_Communicate - MQTT 通信

设备与云平台的双向 MQTT 通信示例。

| 示例名称 | 说明 | 涉及功能 |
|----------|------|----------|
| [`mqtt_attributes`](examples/03.MQTT_Communicate/mqtt_attributes/) | 属性上报与下发 | 设备上报属性、接收云平台下发属性、读取云平台设备属性 |
| [`mqtt_report_event`](examples/03.MQTT_Communicate/mqtt_report_event/) | 事件上报 | 设备向云平台上报事件 |
| [`mqtt_recv_command`](examples/03.MQTT_Communicate/mqtt_recv_command/) | 命令接收 | 设备接收云平台下发的命令 |
| [`mqtt_custom_data`](examples/03.MQTT_Communicate/mqtt_custom_data/) | 自定义数据流 | 自定义 Topic 收发数据 |

---

### 📲 04.WiFi_Provisioning_BLE - 蓝牙配网

设备通过 BLE 接收 WiFi 配置和领取上下文，再连接路由器并接入 ThingsCloud。SDK 内置配网使用的裁剪版 NimBLE，无需另行安装 NimBLE-Arduino；请勿同时链接另一份 NimBLE-Arduino。

| 示例名称 | 说明 | 功能特点 |
|----------|------|----------|
| [`wifi_provisioning_ble_project`](examples/04.WiFi_Provisioning_BLE/wifi_provisioning_ble_project/) | 新增 BLE 项目模式 | 固件指定项目及设备类型，通过蓝牙完成配网和领取设备 |
| [`wifi_provisioning_ble_manufacturer`](examples/04.WiFi_Provisioning_BLE/wifi_provisioning_ble_manufacturer/) | 新增 BLE 厂商模式 | 固件指定 ProductKey，通过蓝牙接收领取上下文并保存平台接入凭据 |

BLE 配网要求芯片同时具备 WiFi 和 BLE 能力，例如 ESP32、ESP32-C3、ESP32-S3、ESP32-C5、ESP32-C6。ESP8266、ESP8285 和 ESP32-S2 不支持 BLE 配网。具体构建条件以示例中的说明和编译检查为准；下方编译矩阵分别记录各子系列的构建结果，实际手机互通仍需在目标硬件上验证。

四种完整配网示例均通过 `BUTTON_PIN` 配置重置按键：使用内部上拉，按键另一端接 GND，按住至少 3 秒后松开触发清除与重启。项目模式仅清除 WiFi；厂商模式还清除已保存的平台接入凭据。DeviceKey、ProductKey 等固件配置不变，设备与平台的解绑需另行操作。请根据实际开发板选择 GPIO，避开串口及其他外设使用的引脚。

BLE 示例的 Flash 容量及分区设置见“配网模式参考”；默认应用分区不足时，编译器会报告 `Sketch too big`。

---

## 📐 配网模式参考

### 项目模式与厂商模式

SoftAP 和 BLE 决定手机向设备传输配网信息的方式；项目模式和厂商模式决定设备如何确定目标项目、领取设备并获取接入凭据，两者可以组合成四种配网方案。

| 比较项 | 项目模式 | 厂商模式 |
|--------|----------|----------|
| 固件预设信息 | MQTT / 设备 API 接入地址、ProjectKey、TypeKey；DeviceKey 可自动生成 | ProductKey；DeviceKey 可自动生成，无需预置目标项目的 ProjectKey |
| App 提供的信息 | WiFi 配置及领取用户的 customer_id | WiFi 配置、provisioning_token、设备 API 和 MQTT 接入地址 |
| 项目选择 | 由固件中的项目配置确定 | 由平台授权的领取上下文确定，同一固件可用于不同目标项目 |
| 接入凭据 | 通过项目证书接口动态获取 AccessToken，重启后重新获取 | 通过厂商领取接口获取 device_secret 等接入凭据并持久化，后续启动从存储恢复 |
| 适用场景 | 设备归属固定项目，项目和设备类型在烧录前已确定 | 厂商统一生产固件，在交付或配网时确定目标项目 |

### 编译环境与程序大小

不同 ESP32 子系列采用的处理器架构、底层无线协议栈及编译配置不同，同一示例生成的固件大小也会不同。以下为 2026-11-03 对 SDK 1.0.16 工作区代码的本地编译结果，覆盖 8 个标准开发板目标、共 26 个适用的“芯片 × 配网模式”组合，本轮通过 26/26 项。

| 项目 | 配置 |
|------|------|
| 构建工具 | Arduino CLI 1.5.0 |
| Arduino-ESP32 | 3.3.8 |
| ESP8266 Arduino core | 3.1.2（用于 ESP8266 和 ESP8285） |
| ArduinoJson | 6.21.5 |
| PubSubClient | 2.8 |
| 其他选项 | 使用下表 FQBN；未显式指定的 CPU、Flash 频率、USB 等选项保持开发板包默认值，不启用 PSRAM |

为统一跨架构的比较口径，下面统计**应用固件 `.bin` 文件大小，单位为字节**，不包含单独的 bootloader、分区表、文件系统镜像或合并烧录镜像。ESP32 编译器的 `Sketch uses` 与 ESP8266 的 `Code in flash` 不是相同统计口径，因此不混入本表直接比较；文件大小与程序空间占用也会因镜像头、对齐等因素略有不同。

| 芯片目标 | SoftAP 项目模式 | SoftAP 厂商模式 | BLE 项目模式 | BLE 厂商模式 |
|----------|----------------:|----------------:|-------------:|-------------:|
| ESP32（经典系列） | 1,181,024 | 1,181,184 | 1,398,240 | 1,398,128 |
| ESP32-S2 | 1,116,784 | 1,101,888 | 不支持 BLE | 不支持 BLE |
| ESP32-S3 | 1,159,056 | 1,137,696 | 1,303,024 | 1,302,928 |
| ESP32-C3 | 1,240,640 | 1,223,040 | 1,410,480 | 1,410,384 |
| ESP32-C5 | 1,302,032 | 1,285,920 | 1,620,496 | 1,620,400 |
| ESP32-C6 | 1,254,976 | 1,237,712 | 1,567,328 | 1,567,248 |
| ESP8266 | 514,944 | 514,672 | 不支持 BLE | 不支持 BLE |
| ESP8285 | 514,912 | 514,640 | 不支持 BLE | 不支持 BLE |

四列依次对应 `wifi_provisioning_cliam_device`、`wifi_provisioning_softap_manufacturer`、`wifi_provisioning_ble_project`、`wifi_provisioning_ble_manufacturer`。ESP32-S2、ESP8266、ESP8285 不具备 BLE 能力，表中的“不支持 BLE”不是编译失败。

为便于复现，各目标的完整 FQBN 及 Flash 布局如下。ESP32 系列统一使用 4MB Flash 和 `min_spiffs`，便于比较同一容量规划下的四种方案；这不表示所有子系列、所有 SoftAP 示例都必须使用该分区。

| 芯片目标 | FQBN | Flash 与布局 |
|----------|------|--------------|
| ESP32（经典系列） | `esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs` | 4MB；Minimal SPIFFS，单个应用分区 1,966,080 字节，文件系统 128KiB |
| ESP32-S2 | `esp32:esp32:esp32s2:FlashSize=4M,PartitionScheme=min_spiffs` | 4MB；Minimal SPIFFS，单个应用分区 1,966,080 字节，文件系统 128KiB |
| ESP32-S3 | `esp32:esp32:esp32s3:FlashSize=4M,PartitionScheme=min_spiffs` | 4MB；Minimal SPIFFS，单个应用分区 1,966,080 字节，文件系统 128KiB |
| ESP32-C3 | `esp32:esp32:esp32c3:FlashSize=4M,PartitionScheme=min_spiffs` | 4MB；Minimal SPIFFS，单个应用分区 1,966,080 字节，文件系统 128KiB |
| ESP32-C5 | `esp32:esp32:esp32c5:FlashSize=4M,PartitionScheme=min_spiffs` | 4MB；Minimal SPIFFS，单个应用分区 1,966,080 字节，文件系统 128KiB |
| ESP32-C6 | `esp32:esp32:esp32c6:FlashSize=4M,PartitionScheme=min_spiffs` | 4MB；Minimal SPIFFS，单个应用分区 1,966,080 字节，文件系统 128KiB |
| ESP8266 | `esp8266:esp8266:generic:eesz=4M1M` | 4MB；4M1M，文件系统约 1MB |
| ESP8285 | `esp8266:esp8266:esp8285:eesz=1M64` | 1MB；1M64，文件系统 64KiB |

该矩阵覆盖上述标准目标的适用配网组合，不穷举所有第三方开发板、主频、PSRAM 和 Flash 容量配置。ESP32-C2、ESP32-C61 需要额外的 Arduino-ESP32 构建支持，未纳入本轮验证；ESP32-H2、ESP32-P4 没有片上 WiFi，不适用这些独立 WiFi 配网示例。

表中是包含按键、指示灯或模拟数据上报等逻辑的完整示例固件，不是 SDK 库文件或配网模块单独占用的空间；示例之间的大小差值也不能全部归因于配网协议本身。

这些结果仅验证编译、链接及镜像容量，不代表已完成各芯片上的手机配网、传感器运行、OTA 或掉电恢复实测。尤其是 ESP8285 的 1MB 配置，固件可烧录不等于已验证其 OTA 空间满足要求；ESP8266/ESP8285 厂商模式仍需准备可用的持久化存储。依赖版本、构建选项和业务代码变化都会影响大小，应以最终产品的实际构建结果为准。

### Flash 容量与分区选择

Flash 总容量与应用分区容量是两个不同的指标。即使模组具有 4MB Flash，默认分区也不会将全部空间分配给单个应用；OTA 通常需要保留两个应用分区，此外还有 NVS 和文件系统等区域。

- **4MB Flash（ESP32 系列）：**以经典 ESP32 为例，默认分区的单个应用容量为 1,310,720 字节，不足以容纳约 1.40MB 的 BLE 示例。可在 Arduino IDE 中选择 `Tools > Partition Scheme > Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)`；该方案的单个应用容量为 1,966,080 字节，保留 OTA，并将文件系统容量缩小为 128KiB。经典 ESP32 对应 CLI 目标为 `esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs`，其他子系列的目标名称见上表。ESP8266 / ESP8285 的布局选项不同，不使用这套 ESP32 分区表。
- **8MB Flash：**可为应用扩展、后续 OTA 固件增长及文件系统提供更充裕的容量。请按模组实际容量设置 `Flash Size`，并选择与 8MB Flash 匹配、应用分区容量足够且满足 OTA 要求的分区方案。仅更换更大容量的模组而继续使用原分区表，不会自动扩大应用分区。

菜单名称及可用方案以所选开发板为准。本轮已验证的 ESP32 系列固件均可容纳在 4MB Flash 的 `min_spiffs` 布局中，无需仅为这些配网示例升级至 8MB；量产选型应同时为业务功能、OTA 升级和文件存储预留空间。8MB 的说明为容量规划建议，本表未单独验证 8MB 构建。以上结果仅验证编译和容量，实际配网、OTA 及掉电恢复仍需在目标硬件上验证。更改分区表应在开发或生产烧录阶段规划，已有设备需另行制定迁移方案。

### ESP8266 厂商模式的存储准备

厂商模式需要持久保存平台接入凭据。ESP32 默认使用 NVS；ESP8266 默认使用 LittleFS。SDK 已负责挂载、读写和清除凭据，但不会自动格式化应用的文件系统。

ESP8266 / ESP8285 使用默认厂商存储前，需要选择包含文件系统空间且与实际 Flash 容量匹配的布局，例如前表中的 `4M1M` / `1M64`。厂商示例 `wifi_provisioning_softap_manufacturer` 已在配网开始前，通过 `ProvisioningStorage.h` 完成存储准备：优先挂载已有 LittleFS；挂载失败时，只有在确认整个文件系统分区均为擦除状态（全为 `0xFF`）后，才允许首次格式化并重新挂载。

若未配置文件系统空间、分区范围无效、Flash 读取失败，或挂载失败且分区内已有数据，示例会输出错误并停止配网，不会自动格式化。挂载失败可能由文件系统损坏或分区布局不匹配导致，应核查布局和已有数据，再由应用或生产流程明确执行恢复操作。其他应用使用默认厂商存储时，也应在配网前完成存储准备；应用也可通过 `setCredentialStorage(load, save, clear)` 提供其他持久化存储实现。

项目模式和厂商模式的 WiFi 名称、密码均由底层 WiFi SDK 持久保存，无需应用额外初始化 LittleFS。LittleFS 在这里用于厂商模式的平台接入凭据（如 `device_secret`、`access_token` 和平台端点）；项目模式不依赖这套厂商凭据存储，也无需为此配置文件系统空间。

---

## 🛠️ 10.IoT_Tutorials - 物联网应用实例

完整的物联网应用示例，涵盖传感器、继电器、DTU、OTA 等场景。

#### 🌡️ 传感器类

| 示例名称 | 说明 | 教程链接 |
|----------|------|----------|
| [`dht_sensor`](examples/10.IoT_Tutorials/dht_sensor/) | DHT11/DHT21/DHT22 温湿度传感器上报数据 | [ESP32 + DHT11/DHT22 温湿度传感器接入 ThingsCloud](https://www.thingscloud.xyz/docs/tutorials/connect-device/esp32-arduino-dht11-sensor.html) |
| [`bh1750_sensor`](examples/10.IoT_Tutorials/bh1750_sensor/) | BH1750 光照传感器采集环境光照亮度（I2C） | - |
| [`mpu6050_sensor`](examples/10.IoT_Tutorials/mpu6050_sensor/) | MPU6050 六轴传感器采集加速度、陀螺仪、温度（I2C） | - |

#### 🔌 继电器控制类

| 示例名称 | 说明 | 适用场景 |
|----------|------|----------|
| [`relay_control`](examples/10.IoT_Tutorials/relay_control/) | ThingsCloud 下发属性控制继电器 | 智能开关、灯光控制 |
| [`relay_control_advanced`](examples/10.IoT_Tutorials/relay_control_advanced/) | 属性控制 + 命令控制延迟反转 | 电磁阀、电磁锁等短时间上电设备 |
| [`esp01_relay`](examples/10.IoT_Tutorials/esp01_relay/) | ESP8266 ESP01 继电器板属性控制 | ESP01 继电器模块 |
| [`esp01_relay_advanced`](examples/10.IoT_Tutorials/esp01_relay_advanced/) | ESP01 继电器板高级控制（属性 + 命令延迟反转） | ESP01 电磁阀/电磁锁 |
| [`esp01_relay_wifi_provisioning`](examples/10.IoT_Tutorials/esp01_relay_wifi_provisioning/) | ESP01 继电器板 WiFi 配网版 | 支持 ThingsX App 配网并自动创建设备，动态获取证书 |
| [`esp8266_smart_power_socket`](examples/10.IoT_Tutorials/esp8266_smart_power_socket/) | ESP8266 插座，SoftAP 厂商配网、开关控制、HLW8032 计量及累计电量 | 智能插座、用电监测 |

#### 💡 执行器控制类

| 示例名称 | 说明 | 功能特点 |
|----------|------|----------|
| [`pwm_control`](examples/10.IoT_Tutorials/pwm_control/) | ESP32 PWM 输出控制 | 属性/命令下发控制 LED 亮度，支持渐变效果 |
| [`servo_control`](examples/10.IoT_Tutorials/servo_control/) | 舵机角度控制 | 属性/命令下发控制舵机角度，支持平滑移动和扫描 |

#### 🔗 DTU 透传类

| 示例名称 | 说明 | 数据格式 | 特点 |
|----------|------|----------|------|
| [`dtu_uart_stream`](examples/10.IoT_Tutorials/dtu_uart_stream/) | UART 透传 DTU | 二进制/文本/JSON | 自定义数据流，支持规则引擎转换 |
| [`dtu_uart_json`](examples/10.IoT_Tutorials/dtu_uart_json/) | UART 透传 DTU | JSON | 属性上报和属性下发 |

> 💡 DTU 示例中，主控 MCU 可通过 UART 和 ESP32 UART1 通信，支持 WiFi 配网。

#### 📷 摄像头/图像类

| 示例名称 | 说明 | 功能特点 |
|----------|------|----------|
| [`esp32s3_gc2145_image_upload`](examples/10.IoT_Tutorials/esp32s3_gc2145_image_upload/) | ESP32S3 + GC2145 摄像头拍照上传 | BOOT 键/命令触发拍照，JPEG 压缩后上传 ThingsCloud，支持属性调节分辨率与画质 |
| [`esp32s3_gc2145_image_upload_wifi_provisioning`](examples/10.IoT_Tutorials/esp32s3_gc2145_image_upload_wifi_provisioning/) | ESP32S3 + GC2145 摄像头拍照上传（AP 配网版） | 基于上例增加 AP 配网模式，支持 ThingsX App 配网并自动创建设备，长按 BOOT 键 5 秒清空配网信息 |
| [`esp32s3-ov3660-image-upload`](examples/10.IoT_Tutorials/esp32s3-ov3660-image-upload/) | ESP32S3 + OV3660 摄像头拍照上传 | BOOT 键/命令触发拍照，JPEG 直出上传 ThingsCloud，支持 OV3660 特有画质参数（锐度、降噪等）调节，拍照带重试机制 |

#### 🔄 OTA 升级

| 示例名称 | 说明 | 功能 |
|----------|------|------|
| [`command_ota`](examples/10.IoT_Tutorials/command_ota/) | ThingsCloud OTA 固件升级 | 通过命令触发 ESP32 固件升级 |

---

> 🎯 **更多示例代码持续更新中……**

---

## 🏢 关于 ThingsCloud

ThingsCloud 是物联网设备统一接入平台和低代码应用开发平台。可以帮助任何需要数字化改造的行业客户，在极短的时间内搭建物联网应用，并适应不断变化的发展需求。

### 核心能力

- 🌐 支持智能传感器、执行器、控制器、智能硬件等设备接入
- 📡 支持 MQTT/HTTP/TCP/Modbus/LoRa/Zigbee/WiFi/BLE 等通信协议
- 📊 实现数据采集、分析、监控
- ⚙️ 灵活配置各种规则
- 🖥️ 生成项目应用 SaaS 和用户应用 App
- 🚀 无需任何云端代码开发

### 🔗 相关链接

| 类型 | 链接 |
|------|------|
| 🏠 官网 | https://www.thingscloud.xyz/ |
| 🎛️ 控制台 | https://console.thingscloud.xyz/ |
| 📖 教程 | https://docs.thingscloud.xyz/tutorials/ |
| 📚 使用文档 | https://docs.thingscloud.xyz |
| 🔌 设备接入 | https://docs.thingscloud.xyz/guide/connect-device/ |
| 📝 博客 | https://www.thingscloud.xyz/blog/ |
| 📺 B站 | https://space.bilibili.com/1953347444 |

---

## 🖼️ 平台截图

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230112114634_afd61232cd029fca77eaebe67e12beaf.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303162529_7d47018b2466053ef3af13dcfd23b703.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303194054_fe9320028f7b499a18893b7a0d25b3c7.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303163508_4b2e3b2052e282bcf2e36143fe90d101.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303164617_c0f98e1ae66b5987aba3408faf86ac1d.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303163103_40fe1d013e8d1d665bdd3cd0ae42adc0.png)

---

## 📞 技术支持

联系 ThingsCloud 技术支持：

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/service/support-qrcode-wlww-1208.png)
