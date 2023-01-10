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

### examples/wifi_basic_connect

普通 WiFi 接入 + 设备一机一密


### examples/wifi_ap_provisioning

WiFi 配网 + 设备一型一密

### examples/wifi_ap_provisioning_reset_button

WiFi 配网 + 设备一型一密 + WiFi 复位按键