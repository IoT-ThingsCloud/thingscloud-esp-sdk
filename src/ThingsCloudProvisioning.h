#pragma once
#include <Arduino.h>
#include <functional>

enum class TCProvResultCode { Ok, InvalidArgument, Unsupported, InvalidState, Busy, RestartRequired, InternalError };
struct TCProvResult { TCProvResultCode code; bool ok() const { return code == TCProvResultCode::Ok; } };
enum class TCProvisioningMode { Project, Manufacturer };
enum class TCWiFiManagement { Internal, External };
enum class TCProvState { Idle, Advertising, Connected, SessionReady, ContextReady, WifiConnecting, CloudRegistering, CloudReady, Failed, TimedOut, Stopped };
enum class TCProvError { None, BluetoothFailure, ConnectionTimeout, SessionTimeout, InvalidRequest, ContextRequired, ContextConflict, WifiConnectTimeout, WifiRejected, CertificateFailed, CertificateRejected, Cancelled, InternalError };
enum class TCProvEventType { Started, Connected, SessionReady, ContextReady, WifiConnecting, WifiConnected, CloudRegistering, CloudReady, Failed, TimedOut, Stopped };
struct TCProvEvent {
    TCProvEventType type;
    TCProvState state;
    TCProvError error = TCProvError::None;
    String request_id;
    int native_code = 0;
};
using TCProvEventCallback = std::function<void(const TCProvEvent&)>;
struct TCClaimRegistrationContext {
    String request_id;
    TCProvisioningMode provisioning_mode = TCProvisioningMode::Project;
    String customer_id, provisioning_token, device_api_endpoint, mqtt_endpoint;
    uint32_t timeout_ms = 60000;
};
struct TCClaimRegistrationResult {
    String request_id;
    bool success = false;
    TCProvError error = TCProvError::None;
    int http_status = 0;
};
using TCClaimRegistrationCallback = std::function<void(const TCClaimRegistrationResult&)>;
struct TCManufacturerConfig {
    String device_key;  // Empty derives the existing stable chip identity.
    String product_key;
};
struct ThingsCloudBLEProvisioningOptions {
    String service_uuid;
    TCProvisioningMode provisioning_mode = TCProvisioningMode::Project;
    String product_key;
    String device_name_prefix = "TC";
    uint32_t provisioning_window_ms = 0; // 0: wait indefinitely; nonzero: bounded pre-Apply window
    uint32_t wifi_connect_timeout_ms = 45000;
    uint32_t cloud_register_timeout_ms = 60000;
    bool auto_start_if_unprovisioned = true;
};
const char* tcProvisioningModeName(TCProvisioningMode mode);
const char* tcProvisioningErrorName(TCProvError error);
bool tcValidRequestId(const String& value);
bool tcValidCustomerId(const String& value);
bool tcValidProductKey(const String& value);
// Full scheme/host/port validation. Plaintext is an explicit development option.
bool tcParseEndpoint(const String& uri, bool mqtt, bool allowPlaintext, String& host, uint16_t& port);

// Shared transport ownership without linking Bluetooth into SoftAP sketches.
bool tcAcquireProvisioningTransport(void* owner);
void tcReleaseProvisioningTransport(void* owner);
bool tcProvisioningTransportBusy();
