# thingscloud-esp-sdk

ThingsCloud IoT Platform WiFi and MQTT client library for ESP8266/ESP32 based boards using arduino platform.

ThingsCloud 推出了基于 ESP32/ESP8266 Arduino 框架的 SDK，方便智能硬件厂商和开发者快速将设备接入 ThingsCloud 云平台并生成物联网应用，完成物联网方案的快速落地。

## SDK 支持特性

- 实现 WiFi 基本连接，可指定 WiFi SSID/Password，连接到固定 AP。
- 实现 WiFi 手机配网，可使用 [ThingsX](https://www.thingscloud.xyz/docs/guide/customer-app/common-app.html) iOS/Android App，为模组快速配置 WiFi 连接信息，而不需要事先将 Wifi SSID/Password 写入固件中。适合设备量产，由使用者自行配置 WiFi。
- 实现 MQTT 一机一密接入 ThingsCloud，为每个模组单独烧录 MQTT 证书。
- 实现 MQTT 一型一密接入 ThingsCloud，为所有模组烧录相同的固件，每个模组自动获取 MQTT 证书。适合设备量产。
- 全面支持 [ThingsCloud MQTT 接入协议](https://www.thingscloud.xyz/docs/guide/connect-device/mqtt.html)，几行代码就可以实现设备和云平台的双向数据实时传输，包括属性上报、属性下发接收、事件上报、命令下发接收、自定义数据流上报和接收等。
- 支持固件 OTA 升级，结合 ThingsCloud 的 OTA 版本管理功能。

## 支持模组型号

- ESP32
    - ESP32-S2
    - ESP32-S3
    - ESP32-C3
    - ESP32-C6（需安装 ESP32 开发板 v3.x.x）
- ESP8266/ESP8285


## 安装开发板依赖

SDK 基于 Arduino 框架，因此您需要先完成 Arduino 框架中 ESP32/ESP8266 的开发板依赖安装。

这里以 Arduino IDE 为例，进入 **文件 > 首选项**。

ESP32 开发板添加以下开发板仓库地址： 
```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

ESP8266 开发板添加以下开发板仓库地址：
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

如下图：

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611101858_2180114cc1fea55dcdd37cce87e5ff3c.png)

然后在开发板管理器中，根据您的实际需要，搜索并安装 ESP32 或 ESP8266 开发板。

ESP32 开发板支持 v2.x.x 和 v3.x.x，如下图：

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611101202_80999e59ebfbc152360b35027a225247.png)

ESP8266 开发板支持 v2.x.x 和 v3.x.x，如下图：

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611101336_032f553d8e401dabd72f729ba022f093.png)


## 安装 SDK

更详细的安装方法可参考官网 [ThingsCloud ESP32/ESP8266 Arduino SDK](https://www.thingscloud.xyz/docs/tutorials/connect-device/esp32-arduino-sdk.html)

支持通过以下方式安装：


### **Arduino**

#### 方法1. 通过 Arduino 库管理器安装（推荐）

进入 Arduino 菜单的 `工具 > 管理库`，搜索 `ThingsCloud`，**选择最新版本**，点击安装。有版本更新时，需要在这里点击更新。

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2024/20250611102233_59bc8eb4bed4c471c3d1203977d14d09.png)


#### 方法2. 手动安装

下载代码仓库最新的 release 压缩包，解压缩后，将整个目录放置在 `Arduino\libraries` 中。

### **PlatformIO**

使用 VSCode + PlatformIO 开发方式，可以直接在 PlatformIO Library 中搜索并添加到项目中。

![articles/2022/20230110235709_ae88b059b93a179e98945a207f6576f9.png](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2022/20230110235709_ae88b059b93a179e98945a207f6576f9.png)


## 安装其它依赖库

SDK 正常运行需要以下依赖库：

- [MQTT PubSub Client](https://github.com/knolleary/pubsubclient)
- [ArduinoJSON](https://github.com/bblanchon/ArduinoJson)

如使用 PlatformIO IDE，依赖库将自动安装。如使用 Arduino IDE，需要单独安装这些依赖库，方法同上。

## ThingsCloud 准备工作

- [快速上手控制台](https://www.thingscloud.xyz/docs/guide/quickstart/signin-console.html)
- [如何获得设备证书？](https://www.thingscloud.xyz/docs/guide/connect-device/device-certificate.html)
- [ThingsCloud MQTT 接入文档](https://docs.thingscloud.xyz/guide/connect-device/mqtt.html)


## 示例

如使用 Arduino IDE，可在 `文件 > 示例` 中，直接打开以下示例代码。

### 01.WiFi_Basic

普通 WiFi 连接方式，指定 WiFi SSID 和密码。

- mqtt_connect_with_access_token：每个设备使用自己的 AccessToken 连接 ThingsCloud
- mqtt_connect_with_device_key：每个设备使用 DeviceKey 请求 AccessToken，连接 ThingsCloud
- mqtt_connect_with_device_key_auto_create_device：每个设备使用 DeviceKey 请求 AccessToken，连接 ThingsCloud。如果 DeviceKey 的设备不存在，支持自动创建设备。
- mqtt_connect_with_auto_device_key：每个设备自动生成唯一的 DeviceKey，请求 AccessToken，连接 ThingsCloud。

### 02.WiFi_Provisioning_AP_Mode

不需要指定 WiFi SSID 和密码，用户通过 ThingsX App 完成 WiFi 配网。

- wifi_provisioning_cliam_device：WiFi 配网模式的示例，用户完成配网后领取设备。
- wifi_provisioning_reset：通过长按按键重置配网的示例。

### 03.MQTT_Communicate

设备 MQTT 通信示例。

- mqtt_attributes：设备上报设备属性、接收云平台下发属性、读取云平台的设备属性。
- mqtt_report_event：设备上报事件的示例。
- mqtt_recv_command：设备接收云平台下发命令的示例。

### 10.IoT_Tutorials

应用示例。

- dht_sensor：dht11/21/22 温湿度传感器上报数据。[ESP32 + DHT11/DHT22 温湿度传感器接入 ThingsCloud](https://www.thingscloud.xyz/docs/tutorials/connect-device/esp32-arduino-dht11-sensor.html)
- relay_control：ThingsCloud 下发控制继电器。[ESP32 + 继电器模块接入 ThingsCloud](https://www.thingscloud.xyz/docs/tutorials/connect-device/esp32-arduino-relay-control.html)
- relay_control_advanced：ThingsCloud 下发控制继电器，以及命令下发控制继电器延迟反转（适合电磁阀、电磁锁等短时间上电）。
- esp01_relay：针对 ESP8266 ESP01 继电器板，支持下发属性控制继电器。
- esp01_relay_advanced：针对 ESP8266 ESP01 继电器板，支持 ThingsCloud 下发属性控制继电器，以及命令下发控制继电器延迟反转（适合电磁阀、电磁锁等短时间上电）。
- dtu_uart_stream：实现透传 DTU，主控 MCU 可通过 UART 和 ESP32 UART1 通信，云平台设备使用自定义数据流，支持二进制、文本、JSON，可通过规则引擎和设备属性进行互转。支持 WiFi 配网。
- dtu_uart_json：实现透传 DTU，主控 MCU 可通过 UART 和 ESP32 UART1 通信，上下行数据使用 JSON 格式，实现设备属性上报和属性下发。支持 WiFi 配网。
- command_ota：使用 ThingsCloud OTA，实现 ESP32 固件升级。

更多示例代码即将推出……


## 关于 ThingsCloud

ThingsCloud 是物联网设备统一接入平台和低代码应用开发平台。可以帮助任何需要数字化改造的行业客户，在极短的时间内搭建物联网应用，并适应不断变化的发展需求。ThingsCloud 支持智能传感器、执行器、控制器、智能硬件等设备接入，支持 MQTT/HTTP/TCP/Modbus/LoRa/Zigbee/WiFi/BLE 等通信协议，实现数据采集、分析、监控，还可以灵活配置各种规则，生成项目应用 SaaS 和用户应用 App，这一切无需任何云端代码开发。

- 官网：https://www.thingscloud.xyz/
- 控制台：https://console.thingscloud.xyz/
- 教程：https://docs.thingscloud.xyz/tutorials/
- 使用文档：https://docs.thingscloud.xyz
- 设备接入：https://docs.thingscloud.xyz/guide/connect-device/
- 博客：https://www.thingscloud.xyz/blog/
- B站：https://space.bilibili.com/1953347444


![articles/2023/20230112114634_afd61232cd029fca77eaebe67e12beaf.png](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230112114634_afd61232cd029fca77eaebe67e12beaf.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303162529_7d47018b2466053ef3af13dcfd23b703.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303194054_fe9320028f7b499a18893b7a0d25b3c7.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303163508_4b2e3b2052e282bcf2e36143fe90d101.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303164617_c0f98e1ae66b5987aba3408faf86ac1d.png)

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230303163103_40fe1d013e8d1d665bdd3cd0ae42adc0.png)

### 技术支持

联系 ThingsCloud 技术支持

![](https://img-1300291923.cos.ap-beijing.myqcloud.com/service/support-qrcode-wlww-1208.png)