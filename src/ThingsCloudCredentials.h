#pragma once
#include "ThingsCloudProvisioning.h"
#include <atomic>

struct TCDeviceCredentials {
    String project_id, device_id, device_secret, access_token;
    String device_api_endpoint, mqtt_endpoint;
};
bool tcCredentialsValid(const TCDeviceCredentials& credentials, bool allowPlaintext);
String tcEncodeCredentials(const TCDeviceCredentials& credentials, const String& deviceKey, const String& productKey);
bool tcDecodeCredentials(const String& blob, const String& deviceKey, const String& productKey, bool allowPlaintext, TCDeviceCredentials& credentials);
bool tcLoadCredentialsBlob(String& blob, const String& identity);
bool tcSaveCredentialsBlob(const String& blob, const String& identity);
bool tcClearCredentialsBlob();

// One worker owns HTTP and its copied parameters; only the application loop
// applies its result. done is the release/acquire publication barrier.
struct TCCredentialJob {
    TCClaimRegistrationContext context;
    String deviceKey, productKey, projectKey, api;
    bool allowPlaintext = false, debug = false;
    uint32_t started = 0, budget = 60000, nextAttempt = 0;
    std::atomic<bool> done{false}, cancelled{false};
    bool success = false;
    TCProvError error = TCProvError::CertificateFailed;
    int httpStatus = 0;
    TCDeviceCredentials credentials;
};
void tcRunCredentialJob(TCCredentialJob* job);

String tcQueryEncode(const String& value);
