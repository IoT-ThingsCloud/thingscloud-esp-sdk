# thingscloud-esp-sdk
ThingsCloud IoT Platform WiFi and MQTT client library for ESP8266/ESP32 based boards using arduino platform.

## 安装 SDK

ThingsCloud ESP SDK 支持通过以下方式安装：

- PlatformIO
- 手动安装 Library

另外，请安装以下依赖库：

- [MQTT PubSub Client](https://github.com/knolleary/pubsubclient)
- [ArduinoJSON](https://github.com/bblanchon/ArduinoJson)


## 示例

### examples/wifi_basic_connect

普通 WiFi 接入 + 设备一机一密


### examples/wifi_ap_provisioning

WiFi 配网 + 设备一型一密

### examples/wifi_ap_provisioning_reset_button

WiFi 配网 + 设备一型一密 + WiFi 复位按键