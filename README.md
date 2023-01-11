# thingscloud-esp-sdk
ThingsCloud IoT Platform WiFi and MQTT client library for ESP8266/ESP32 based boards using arduino platform.

ThingsCloud 是物联网设备统一接入平台和低代码应用开发平台。可以帮助任何需要数字化改造的行业客户，在极短的时间内搭建物联网应用，并适应不断变化的发展需求。ThingsCloud 支持智能传感器、执行器、控制器、智能硬件等设备接入，支持 MQTT/HTTP/TCP/Modbus/LoRa/Zigbee/WiFi/BLE 等通信协议，实现数据采集、分析、监控，还可以灵活配置各种规则，生成项目应用 SaaS 和用户应用 App，这一切无需任何云端代码开发。

- 官网：https://www.thingscloud.xyz/
- 控制台：https://console.thingscloud.xyz/
- 教程：https://docs.thingscloud.xyz/tutorials
- 文档：https://docs.thingscloud.xyz
- 设备接入：https://docs.thingscloud.xyz/guide/connect-device/
- 博客：https://www.thingscloud.xyz/blog/
- B站：https://space.bilibili.com/1953347444


![articles/2022/20230110235119_bffb83e580e27575f42d41b419a1b1ae.png](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2022/20230110235119_bffb83e580e27575f42d41b419a1b1ae.png)



## 安装 SDK

支持通过以下方式安装：

### **PlatformIO**

使用 VSCode + PlatformIO 开发方式，可以直接在 PlatformIO Library 中搜索并添加到项目中。

![articles/2022/20230110235709_ae88b059b93a179e98945a207f6576f9.png](https://img-1300291923.cos.ap-beijing.myqcloud.com/articles/2022/20230110235709_ae88b059b93a179e98945a207f6576f9.png)

### **Arduino**

暂时不支持通过库管理器添加，请手动下载整个库，放置在 `Arduino\libraries` 中。

## 安装其它依赖

请安装以下依赖库：

- [MQTT PubSub Client](https://github.com/knolleary/pubsubclient)
- [ArduinoJSON](https://github.com/bblanchon/ArduinoJson)


## 示例

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

