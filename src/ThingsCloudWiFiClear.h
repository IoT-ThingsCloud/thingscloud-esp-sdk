#pragma once

#include <ThingsCloudMQTT.h>

// 示例按需启用云端清除配网；不会自动改变其他 SDK 用户的属性处理。
class ThingsCloudWiFiClear
{
public:
    ThingsCloudWiFiClear(ThingsCloudMQTT &client, DelayedExecutionCallback reset)
        : _client(client), _reset(reset) {}

    void begin()
    {
        _client.onAttributesPush(MessageReceivedCallbackJSON([this](const JsonObject &attributes)
        {
            if (_pending || !attributes["wifi_clear"].is<bool>() || !attributes["wifi_clear"].as<bool>())
                return;

            if (!_client.reportAttributes("{\"wifi_clear_done\":true}"))
                return;

            _pending = true;
            // 留出发送时间，再清除 WiFi 并重启；publish 成功不代表平台已确认收到。
            _client.executeDelayed(1000, _reset);
        }));
    }

private:
    ThingsCloudMQTT &_client;
    DelayedExecutionCallback _reset;
    bool _pending = false;
};
