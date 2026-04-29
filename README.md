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
| 📱 WiFi 手机配网 | 使用 [ThingsX](https://www.thingscloud.xyz/docs/guide/customer-app/common-app.html) iOS/Android App 配网，无需预先写入 WiFi 信息，适合设备量产 |
| 🔐 MQTT 一机一密 | 为每个模组单独烧录 MQTT 证书 |
| 🔑 MQTT 一型一密 | 所有模组烧录相同固件，自动获取 MQTT 证书，适合设备量产 |
| 📡 完整协议支持 | 全面支持 [ThingsCloud MQTT 接入协议](https://www.thingscloud.xyz/docs/guide/connect-device/mqtt.html)，实现双向数据实时传输 |
| 🔄 OTA 升级 | 支持固件 OTA 升级，结合 ThingsCloud 版本管理功能 |

## 🎯 支持模组型号

### ESP32 系列

| 型号 | 说明 |
|------|------|
| ESP32 | 双核、WiFi + BLE |
| ESP32-S2 | 低功耗、USB OTG |
| ESP32-S3 | AI 加速、USB OTG |
| ESP32-C3 | RISC-V、低成本 |
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

> 💡 ESP32 开发板支持 v2.x.x 和 v3.x.x
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

### 📱 02.WiFi_Provisioning_AP_Mode - WiFi 配网

不需要指定 WiFi SSID 和密码，用户通过 ThingsX App 完成 WiFi 配网。

| 示例名称 | 说明 | 功能特点 |
|----------|------|----------|
| [`wifi_provisioning_cliam_device`](examples/02.WiFi_Provisioning_AP_Mode/wifi_provisioning_cliam_device/) | WiFi 配网模式示例 | 用户完成配网后领取设备 |
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

### 🛠️ 10.IoT_Tutorials - 物联网应用实例

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
| [`esp01_relay_wifi_provisioning`](examples/10.IoT_Tutorials/esp01_relay_wifi_provisioning/) | ESP01 继电器板 WiFi 配网版 | 用户自行配网 |

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
