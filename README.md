# thingscloud-esp-sdk
ThingsCloud IoT Platform WiFi and MQTT client library for ESP8266/ESP32 based boards using arduino platform.

ThingsCloud 推出了基于 ESP32/ESP8266 Arduino 的 SDK，方便智能硬件厂商和开发者快速将设备接入 ThingsCloud 云平台并生成物联网应用，完成物联网方案的快速落地。

## SDK 支持特性

- WiFi 基本连接，指定 WiFi SSID/Password，连接到固定 AP。
- WiFi 配网，可使用 ThingsX iOS/Android App，为模组快速配置 WiFi 连接信息。适合多设备的量产。
- MQTT 一机一密，为每个模组烧录独立的证书。
- MQTT 一型一密，为所有模组烧录相同的固件，每个模组自动获取证书。适合多设备的量产。
- 支持 ThingsCloud MQTT 接入协议，几行代码就可以实现设备和云平台的双向数据实时传输，包括属性上报和下发、事件上报、命令接收、自定义数据流等。
- 固件 OTA 升级，结合 ThingsCloud 的 OTA 版本管理功能。

ThingsCloud 是物联网设备统一接入平台和低代码应用开发平台。可以帮助任何需要数字化改造的行业客户，在极短的时间内搭建物联网应用，并适应不断变化的发展需求。ThingsCloud 支持智能传感器、执行器、控制器、智能硬件等设备接入，支持 MQTT/HTTP/TCP/Modbus/LoRa/Zigbee/WiFi/BLE 等通信协议，实现数据采集、分析、监控，还可以灵活配置各种规则，生成项目应用 SaaS 和用户应用 App，这一切无需任何云端代码开发。

- 官网：https://www.thingscloud.xyz/
- 控制台：https://console.thingscloud.xyz/
- 教程：https://docs.thingscloud.xyz/tutorials
- 文档：https://docs.thingscloud.xyz
- 设备接入：https://docs.thingscloud.xyz/guide/connect-device/
- 博客：https://www.thingscloud.xyz/blog/
- B站：https://space.bilibili.com/1953347444


![articles/2023/20230112114634_afd61232cd029fca77eaebe67e12beaf.png](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230112114634_afd61232cd029fca77eaebe67e12beaf.png)


## 安装 SDK

支持通过以下方式安装：

### **PlatformIO**

使用 VSCode + PlatformIO 开发方式，可以直接在 PlatformIO Library 中搜索并添加到项目中。

![articles/2022/20230110235709_ae88b059b93a179e98945a207f6576f9.png](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2022/20230110235709_ae88b059b93a179e98945a207f6576f9.png)

### **Arduino**

#### 方法1. 通过 Arduino 库管理器安装（推荐）

进入 Arduino 菜单的 `工具 > 管理库`，搜索 `ThingsCloud`，选择最新版本，点击安装。有版本更新时，需要在这里点击更新。

![articles/2023/20230112114700_f7bb8f38867201e9301b4450e31b65e0.png](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2023/20230112114700_f7bb8f38867201e9301b4450e31b65e0.png)

#### 方法2. 手动安装

下载代码仓库最新的 release 压缩包，解压缩后，将整个目录放置在 `Arduino\libraries` 中。


## 安装其它依赖

SDK 正常运行需要以下依赖库：

- [MQTT PubSub Client](https://github.com/knolleary/pubsubclient)
- [ArduinoJSON](https://github.com/bblanchon/ArduinoJson)

如使用 PlatformIO IDE，依赖库将自动安装。如使用 Arduino IDE，需要单独安装这些依赖库，方法同上。


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

更多示例代码即将推出……

