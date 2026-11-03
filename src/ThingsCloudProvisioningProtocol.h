#pragma once
#include "ThingsCloudProvisioning.h"
#include <ArduinoJson.h>
#include <vector>

// Transport-independent business protocol. Owner serializes ALL access; BLE uses
// one mutex for endpoint handlers, synchronous Apply, disconnect and loop().
class TCProvisioningSession {
public:
    static constexpr size_t FrameLimit = 240, ContextLimit = 8192, ChunkLimit = 72;
    TCProvisioningSession(TCProvisioningMode mode, const String& key, const String& instance,
                          bool allowPlaintext = false);
    bool validIdentity() const;
    String handle(bool infoEndpoint, const uint8_t* data, size_t length);
    bool apply();                 // Synchronous protected Wi-Fi Apply handler only.
    void disconnected();          // Discard PRE-Apply RAM only.
    void clear();
    void fail(TCProvError error);
    void registering();
    void ready();
    const TCClaimRegistrationContext& context() const { return _context; }
    bool committed() const { return _applied; }
    bool contextReady() const { return _ready; }
    bool cloudStarted() const { return _cloudStarted; }
    TCProvState state() const { return _state; }
    TCProvError error() const { return _error; }
private:
    TCProvisioningMode _mode;
    String _key, _instance, _draftId;
    TCClaimRegistrationContext _context;
    std::vector<uint8_t> _draft;
    size_t _expected = 0;
    bool _ready = false, _applied = false, _cloudStarted = false, _allowPlaintext;
    TCProvState _state = TCProvState::SessionReady;
    TCProvError _error = TCProvError::None;
    const char* statusName() const;
};
// Strict JSON validation before ArduinoJson (which otherwise accepts duplicate
// keys). Also rejects escaped NULs, excessive nesting and trailing JSON values.
bool tcStrictJson(const uint8_t* data, size_t length);
bool tcReadManufacturerContext(JsonObjectConst object, TCClaimRegistrationContext& context, bool allowPlaintext);
