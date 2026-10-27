# SDK 验证

## 全部示例编译

从仓库根目录运行：

```powershell
python tools/compile_examples.py --libraries C:/Users/your-name/Documents/Arduino/libraries --parallel 3 --jobs 3
```

`--libraries` 指向第三方库所在目录。脚本显式指定当前仓库为 SDK，使用独立 sketchbook 安装 ArduinoJson 6.21.5，避免引用 Arduino IDE 中已安装的 SDK 或 ArduinoJson 7。开发板包由本机 Arduino CLI 提供。

需要安装 Arduino CLI、ESP32/ESP8266 开发板包，以及下列第三方库。本次验证的版本如下：

| 依赖 | 版本 |
| --- | --- |
| Arduino CLI | 1.5.0 |
| ESP32 Arduino core | 3.3.8 |
| ESP8266 Arduino core | 3.1.2 |
| ArduinoJson | 6.21.5（脚本自动安装到隔离目录） |
| PubSubClient | 2.8 |
| DHT sensor library | 1.4.7 |
| Adafruit Unified Sensor | 1.1.15 |
| Adafruit BusIO | 1.17.4 |
| Adafruit MPU6050 | 2.2.9 |
| BH1750 | 1.3.0 |
| ESP32Servo | 3.2.0 |

矩阵覆盖全部 25 个 `.ino` 示例，共 38 项编译：

- ESP01 示例使用 `esp8266:esp8266:generic`。
- 摄像头示例使用 `esp32:esp32:esp32s3:PSRAM=opi`。
- 其他示例使用 `esp32:esp32:esp32`。
- 基础连接、MQTT 通信、普通继电器、传感器和领取设备配网示例额外在 ESP8266 上编译。
- `mqtt_custom_data` 目前只有占位 README，没有可编译的 sketch。

输出保存在 `.build-tests/`（已加入 gitignore）：

- `results.json`：逐项结果、开发板、耗时和 SDK 源码路径核验结果。
- `logs/`：完整编译日志。
- `build/`：各项编译产物。

脚本检查 Arduino CLI 的 `libraries.cache`，确保包含的 SDK 目录恰好是仓库 `src`。每项均编译和链接生成固件，不烧录、不连接真实云平台。

可用 `--match` 筛选示例或平台。`--resume` 会跳过之前通过的记录，只适用于 SDK、示例和依赖均未变更的情况；源码变化后应运行完整矩阵。

如果上次编译被强制中断，使用 `--clean` 重建受影响的项目，避免复用不完整的编译缓存。

## MQTT 逻辑回归

```powershell
python tools/run_host_tests.py
```

需要本机 GCC/Clang 或 Visual Studio 2022 C++ 工具，以及上一步安装的 ArduinoJson 6。可通过 `--json-library` 指定其 `src` 路径。

测试直接包含仓库 `ThingsCloudMQTT.cpp`，替换 WiFi、HTTP 和 MQTT 传输层，使用真实 ArduinoJson 解析器，验证：

- HTTP 初始化失败、网络错误、非 2xx、无效 JSON、业务拒绝、缺失/空/类型错误 Token 均返回失败，HTTP 资源正确释放。
- 证书请求失败后等待并重试，成功后才进入后续连接流程。
- WiFi/MQTT 连接、断开回调的触发、状态和去重，以及空回调处理。
- 同一 Topic 的四种回调类型均可替换，不影响其他 Topic；订阅失败保留原回调。
- 自定义 MQTT 缓冲区跨重连保留。
- 空消息、缓冲区边界消息不修改输入内存；超过 512 字节、包含大量字段的 JSON 可正常解析。

Windows/MSVC 测试启用 AddressSanitizer。测试输出保存在 `.build-tests/host/`。

这些测试分别验证构建兼容性与被修复的逻辑；实机 WiFi 配网、传感器读数、摄像头和 OTA 效果仍需硬件验证。
