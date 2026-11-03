#define THINGSCLOUD_BLE_IMPLEMENTATION
#include "ThingsCloudBLEProvisioning.h"
#include "ThingsCloudProvisioningProtocol.h"
#if TC_HAS_BLE_PROVISIONING
#include "internal/nimble/NimBLEDevice.h"
#include <protocomm.h>
#include <protocomm_security1.h>
#include <network_provisioning/network_config.h>
#include <network_provisioning/network_scan.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory>
#include <cstring>

namespace {
static_assert(MYNEWT_VAL(MSYS_1_BLOCK_COUNT) >= (TCProvisioningSession::FrameLimit + 17) / 18 + 4,
              "Keep enough BLE buffers for the provisioning profile at a low MTU");
struct Guard {
    SemaphoreHandle_t m;
    explicit Guard(SemaphoreHandle_t value):m(value){if(m)xSemaphoreTake(m,portMAX_DELAY);}
    ~Guard(){if(m)xSemaphoreGive(m);}
};
struct TransportGuard {
    SemaphoreHandle_t m;
    explicit TransportGuard(SemaphoreHandle_t value):m(value){if(m)xSemaphoreTakeRecursive(m,portMAX_DELAY);}
    ~TransportGuard(){if(m)xSemaphoreGiveRecursive(m);}
};
bool decodeUUID(const String& s,uint8_t* bytes) {
    if(s.length()!=36)return false;int count=0,high=-1;
    for(size_t i=0;i<s.length();++i) {char c=s.c_str()[i];
        if(i==8||i==13||i==18||i==23) {if(c!='-')return false;continue;}
        if(!isxdigit((unsigned char)c))return false;int v=c<='9'?c-'0':tolower(c)-'a'+10;
        if(high<0)high=v;else {bytes[15-count++]=(high<<4)|v;high=-1;}}
    return count==16;
}
}
// Own only the transport/coordination. Official codecs and Security1 retain
// the existing phone protocol without linking the multi-security manager.
struct ThingsCloudBLEProvisioning::Impl : NimBLEServerCallbacks {
    ThingsCloudMQTT* client=nullptr;
    ThingsCloudBLEProvisioningOptions options;
    TCProvEventCallback callback;
    std::unique_ptr<TCProvisioningSession> session;
    SemaphoreHandle_t mutex=xSemaphoreCreateMutex();
    SemaphoreHandle_t transportMutex=xSemaphoreCreateRecursiveMutex();
    esp_event_handler_instance_t wifiHandler=nullptr,ipHandler=nullptr;
    std::vector<TCProvEvent> events;
    protocomm_t* pc=nullptr;
    NimBLEServer* server=nullptr;
    bool initialized=false,running=false,stopping=false,registered=false,connected=false,secured=false;
    bool transportActive=false,pendingStop=false;
    uint32_t lastStopAttempt=0,lastWifiRetry=0;
    bool wifiFresh=false,sawAssociation=false,recoverWifi=false,terminal=false;
    bool credentialsSet=false,connectPending=false,wifiAttemptStarted=false;
    bool wifiRetryPending=false,recoveryInProgress=false,wifiFailed=false;
    network_prov_wifi_sta_fail_reason_t wifiFailureReason=NETWORK_PROV_WIFI_STA_AUTH_ERROR;
    wifi_config_t pendingWifi={};
    uint32_t windowStart=0,applyStart=0,connectStart=0,finishStart=0,lastReconnect=0,generation=0,associationGeneration=0;
    int32_t connection=-1;
    String appliedSSID;
    TCProvState current=TCProvState::Idle;
    uint8_t uuid[16]={0};
    network_prov_config_handlers_t configHandlers={};
    network_prov_scan_handlers_t scanHandlers={};
    struct Binding {Impl* self;bool info;};
    Binding infoBinding{this,true},contextBinding{this,false};
    struct Endpoint : NimBLECharacteristicCallbacks {
        Impl* self;const char* name;uint16_t id;NimBLECharacteristic* characteristic=nullptr;
        Endpoint(Impl* p,const char* n,uint16_t i):self(p),name(n),id(i){}
        void onWrite(NimBLECharacteristic* c,NimBLEConnInfo& info) override {
            self->request(*this,c,info.getConnHandle());
        }
    } endpoints[6]={{this,"prov-scan",0xff50},{this,"prov-session",0xff51},
        {this,"prov-config",0xff52},{this,"proto-ver",0xff53},
        {this,"tc-device-info",0xff54},{this,"tc-claim-context",0xff55}};
    ~Impl(){if(mutex)vSemaphoreDelete(mutex);if(transportMutex)vSemaphoreDelete(transportMutex);}
    bool windowExpired(uint32_t now) const {
        return options.provisioning_window_ms != 0 &&
            (uint32_t)(now-windowStart) >= options.provisioning_window_ms;
    }
    void emit(TCProvEventType type,TCProvError error=TCProvError::None,int code=0) {
        TCProvEvent event;event.type=type;event.state=current;event.error=error;event.native_code=code;
        if(session)event.request_id=session->context().request_id;
        if(events.size()<24)events.push_back(event);
    }
    void clearValues() {for(auto& ep:endpoints)if(ep.characteristic)ep.characteristic->setValue((const uint8_t*)"",0);}
    void request(Endpoint& ep,NimBLECharacteristic* c,uint16_t handle) {
        TransportGuard transport(transportMutex);
        auto value=c->getValue();
        c->setValue((const uint8_t*)"",0); // Never echo unprocessed requests or stale responses.
        if(!transportActive||!pc||connection!=handle||value.size()>512)return;
        uint8_t* response=nullptr;ssize_t length=0;
        esp_err_t err=protocomm_req_handle(pc,ep.name,handle,
            reinterpret_cast<const uint8_t*>(value.data()),value.size(),&response,&length);
        if(err==ESP_OK&&length>=0&&length<=512&&(response||length==0))c->setValue(response?response:(const uint8_t*)"",length);
        else server->disconnect(handle); // Fail closed; never fall back to Security0.
        free(response);
    }
    void onConnect(NimBLEServer* s,NimBLEConnInfo& info) override {
        TransportGuard transport(transportMutex);
        if(!transportActive||!pc){s->disconnect(info.getConnHandle());return;}
        Guard lock(mutex);
        if(connected||!running||stopping||protocomm_open_session(pc,info.getConnHandle())!=ESP_OK) {
            s->disconnect(info.getConnHandle());return;
        }
        clearValues();connected=true;secured=false;connection=info.getConnHandle();connectStart=millis();
        if(!session->committed())current=TCProvState::Connected;
        emit(TCProvEventType::Connected);
    }
    void onDisconnect(NimBLEServer*,NimBLEConnInfo& info,int) override {
        TransportGuard transport(transportMutex);
        if(pc)protocomm_close_session(pc,info.getConnHandle());
        {Guard lock(mutex);
            if(connection==info.getConnHandle()) {
                connected=false;secured=false;connection=-1;clearValues();
                if(session) {session->disconnected();if(!session->committed()) {
                    memset(&pendingWifi,0,sizeof(pendingWifi));credentialsSet=false;current=TCProvState::Advertising;
                }}
            }
        }
        if(transportActive)NimBLEDevice::startAdvertising();
    }
    static esp_err_t endpoint(uint32_t id,const uint8_t* in,ssize_t length,uint8_t** out,ssize_t* outLength,void* arg) {
        auto* binding=static_cast<Binding*>(arg);Impl* self=binding->self;
        if(length<0||!out||!outLength)return ESP_ERR_INVALID_ARG;
        Guard lock(self->mutex);
        if(!self->running||self->stopping||!self->session||self->connection!=static_cast<int32_t>(id))return ESP_ERR_INVALID_STATE;
        // protocomm decrypted this request using the same Security1 session.
        if(!self->secured) {
            self->secured=true;
            // A phone can reconnect while the committed Wi-Fi attempt continues.
            // Reading info must not replace WifiConnecting/Failed with SessionReady.
            if(!self->session->committed())self->current=TCProvState::SessionReady;
            self->emit(TCProvEventType::SessionReady);
        }
        bool before=self->session->contextReady();
        String reply=self->session->handle(binding->info,in,(size_t)length);
        if(!before&&self->session->contextReady()) {self->current=TCProvState::ContextReady;self->emit(TCProvEventType::ContextReady);}
        *out=(uint8_t*)malloc(reply.length());if(!*out)return ESP_ERR_NO_MEM;
        memcpy(*out,reply.c_str(),reply.length());*outLength=reply.length();return ESP_OK;
    }
    static Impl* owner(network_prov_ctx_t** ctx){return reinterpret_cast<Impl*>(*ctx);}
    static Impl* owner(network_prov_scan_ctx_t** ctx){return reinterpret_cast<Impl*>(*ctx);}
    static esp_err_t setConfig(const network_prov_config_set_wifi_data_t* data,network_prov_ctx_t** ctx) {
        auto* self=owner(ctx);Guard lock(self->mutex);
        if(!data||!self->running||self->stopping||self->registered||self->recoverWifi||self->recoveryInProgress||(self->session->committed()&&self->session->state()!=TCProvState::Failed)||!data->ssid[0]||
            strnlen(data->ssid,sizeof(data->ssid))>32)return ESP_ERR_INVALID_STATE;
        memset(&self->pendingWifi,0,sizeof(self->pendingWifi));
        memcpy(self->pendingWifi.sta.ssid,data->ssid,32);
        memcpy(self->pendingWifi.sta.password,data->password,64);
        memcpy(self->pendingWifi.sta.bssid,data->bssid,6);
        for(auto c:data->bssid)if(c)self->pendingWifi.sta.bssid_set=true;
        self->pendingWifi.sta.channel=data->channel;
        self->credentialsSet=true;return ESP_OK;
    }
    static esp_err_t applyConfig(network_prov_ctx_t** ctx) {
        auto* self=owner(ctx);Guard lock(self->mutex);uint32_t now=millis();
        if(!self->running||self->stopping||self->registered||self->recoverWifi||self->recoveryInProgress||!self->credentialsSet||
            self->windowExpired(now))return ESP_ERR_INVALID_STATE;
        // Commit the claim synchronously before any Wi-Fi action or BLE disconnect.
        if(!self->session->apply()) {
            self->emit(TCProvEventType::Failed,TCProvError::ContextRequired);return ESP_ERR_INVALID_STATE;
        }
        char ssid[33]={};memcpy(ssid,self->pendingWifi.sta.ssid,32);self->appliedSSID=ssid;
        ++self->generation;self->sawAssociation=false;self->wifiFresh=false;self->applyStart=now;
        self->connectPending=true;self->wifiAttemptStarted=false;self->wifiRetryPending=false;self->wifiFailed=false;self->current=TCProvState::WifiConnecting;
        self->emit(TCProvEventType::WifiConnecting);return ESP_OK;
    }
    static esp_err_t getStatus(network_prov_config_get_wifi_data_t* data,network_prov_ctx_t** ctx) {
        auto* self=owner(ctx);Guard lock(self->mutex);memset(data,0,sizeof(*data));
        if(self->wifiFresh&&WiFi.status()==WL_CONNECTED) {
            data->wifi_state=NETWORK_PROV_WIFI_STA_CONNECTED;
            auto ip=WiFi.localIP().toString();strlcpy(data->conn_info.ip_addr,ip.c_str(),sizeof(data->conn_info.ip_addr));
            strlcpy(data->conn_info.ssid,self->appliedSSID.c_str(),sizeof(data->conn_info.ssid));
            wifi_ap_record_t ap={};if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK) {
                memcpy(data->conn_info.bssid,ap.bssid,6);data->conn_info.channel=ap.primary;data->conn_info.auth_mode=ap.authmode;
            }
        } else if(self->wifiFailed) {
            data->wifi_state=NETWORK_PROV_WIFI_STA_DISCONNECTED;data->fail_reason=self->wifiFailureReason;
        } else data->wifi_state=NETWORK_PROV_WIFI_STA_CONNECTING;
        return ESP_OK;
    }
    static esp_err_t scanStart(bool blocking,bool passive,uint8_t,uint32_t period,network_prov_scan_ctx_t** ctx) {
        auto* self=owner(ctx);
        {Guard lock(self->mutex);
            if(!self->running||self->stopping||self->registered||self->recoverWifi||self->recoveryInProgress||
                (self->session->committed()&&self->session->state()!=TCProvState::Failed))return ESP_ERR_INVALID_STATE;
        }
        if(WiFi.scanComplete()==WIFI_SCAN_RUNNING)return ESP_OK;
        WiFi.scanDelete();
        int count=WiFi.scanNetworks(!blocking,true,passive,period?std::min<uint32_t>(period,500):120);
        return count>=0||count==WIFI_SCAN_RUNNING?ESP_OK:ESP_FAIL;
    }
    static esp_err_t scanStatus(bool* done,uint16_t* count,network_prov_scan_ctx_t**) {
        int n=WiFi.scanComplete();*done=n!=WIFI_SCAN_RUNNING;*count=n>0?n:0;return ESP_OK;
    }
    static esp_err_t scanResult(uint16_t index,network_prov_scan_wifi_result_t* result,network_prov_scan_ctx_t**) {
        int n=WiFi.scanComplete();if(n<0||index>=n)return ESP_ERR_INVALID_ARG;
        memset(result,0,sizeof(*result));String ssid=WiFi.SSID(index);
        memcpy(result->ssid,ssid.c_str(),std::min(ssid.length(),sizeof(result->ssid)));
        memcpy(result->bssid,WiFi.BSSID(index),6);result->channel=WiFi.channel(index);
        result->rssi=WiFi.RSSI(index);result->auth=WiFi.encryptionType(index);return ESP_OK;
    }
    static void wifiEvent(void* arg,esp_event_base_t base,int32_t event,void* data) {
        auto* self=static_cast<Impl*>(arg);Guard lock(self->mutex);
        if(!self->running||self->stopping||!self->session||!self->session->committed()||self->registered||!self->wifiAttemptStarted||self->current!=TCProvState::WifiConnecting)return;
        if(base==WIFI_EVENT&&event==WIFI_EVENT_STA_CONNECTED) {
            auto* c=static_cast<wifi_event_sta_connected_t*>(data);
            if(c&&self->appliedSSID.length()==c->ssid_len&&!memcmp(c->ssid,self->appliedSSID.c_str(),c->ssid_len)) {
                self->sawAssociation=true;self->associationGeneration=self->generation;self->wifiRetryPending=false;
            }
        } else if(base==WIFI_EVENT&&event==WIFI_EVENT_STA_DISCONNECTED) {
            self->sawAssociation=false;self->wifiFresh=false;
            auto* disconnected=static_cast<wifi_event_sta_disconnected_t*>(data);
            int reason=disconnected?disconnected->reason:0;
            bool auth=reason==WIFI_REASON_AUTH_FAIL||reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT||
                reason==WIFI_REASON_HANDSHAKE_TIMEOUT||reason==WIFI_REASON_MIC_FAILURE;
            bool missing=reason==WIFI_REASON_NO_AP_FOUND;
            if(auth||missing) {
                self->wifiFailed=true;self->wifiRetryPending=false;
                self->wifiFailureReason=missing?NETWORK_PROV_WIFI_STA_AP_NOT_FOUND:NETWORK_PROV_WIFI_STA_AUTH_ERROR;
                self->session->fail(TCProvError::WifiRejected);self->current=TCProvState::Failed;
                self->recoverWifi=true;self->emit(TCProvEventType::Failed,TCProvError::WifiRejected,reason);
            } else {
                // Preserve the normal manager's transient reconnect behavior.
                // Retry in loop, with backoff and the original Apply deadline.
                self->wifiRetryPending=true;self->lastWifiRetry=millis();
            }
        } else if(base==IP_EVENT&&event==IP_EVENT_STA_GOT_IP&&self->sawAssociation&&self->associationGeneration==self->generation)self->wifiFresh=true;
    }
    bool stopTransport() {
        {TransportGuard transport(transportMutex);transportActive=false;}
        if(wifiHandler){esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiHandler);wifiHandler=nullptr;}
        if(ipHandler){esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,ipHandler);ipHandler=nullptr;}
        if(WiFi.scanComplete()==WIFI_SCAN_RUNNING)esp_wifi_scan_stop();
        // Neither mutex may be held while waiting for host callbacks to stop.
        // On a stop error the live host still owns its GATT objects and pc.
        if(initialized&&!NimBLEDevice::deinit(true))return false;
        initialized=false;
        {TransportGuard transport(transportMutex);
            if(pc){protocomm_delete(pc);pc=nullptr;}
            server=nullptr;for(auto& ep:endpoints)ep.characteristic=nullptr;
        }
        // Do not race Arduino's asynchronous SCAN_DONE allocation/cleanup.
        if(WiFi.scanComplete()!=WIFI_SCAN_RUNNING)WiFi.scanDelete();
        return true;
    }
    void stopFailed() {
        Guard lock(mutex);
        if(!pendingStop) {
            if(session)session->fail(TCProvError::BluetoothFailure);
            current=TCProvState::Failed;emit(TCProvEventType::Failed,TCProvError::BluetoothFailure);
        }
        pendingStop=true;running=false;stopping=true;lastStopAttempt=millis();
    }
    bool closeTransportForCloud() {
        {Guard lock(mutex);stopping=true;}
        if(!stopTransport()){stopFailed();return false;}
        {Guard lock(mutex);connected=false;secured=false;connection=-1;stopping=false;}
        return true;
    }
    bool shutdown(bool cancel) {
        String id;
        {Guard lock(mutex);stopping=true;if(session)id=session->context().request_id;}
        if(cancel&&client&&id.length())client->cancelClaimRegistration(id,true);
        if(!stopTransport()){stopFailed();return false;}
        {Guard lock(mutex);running=false;connected=false;secured=false;connection=-1;stopping=false;pendingStop=false;
            connectPending=false;credentialsSet=false;wifiAttemptStarted=false;wifiRetryPending=false;recoveryInProgress=false;
            memset(&pendingWifi,0,sizeof(pendingWifi));if(session)session->clear();}
        tcReleaseProvisioningTransport(this);return true;
    }
    void dispatchEvents() {
        std::vector<TCProvEvent> pending;
        {Guard lock(mutex);pending.swap(events);}
        for(const auto& event:pending)if(callback)callback(event);
    }
    TCProvResult launch(bool newWindow) {
        if(!client||!session)return {TCProvResultCode::InvalidState};
        if(pendingStop)return {TCProvResultCode::Busy};
        if(running)return {TCProvResultCode::Ok};
        wifi_config_t saved={};
        if(esp_wifi_get_config(WIFI_IF_STA,&saved)!=ESP_OK)return {TCProvResultCode::InternalError};
        if(saved.sta.ssid[0])return {TCProvResultCode::RestartRequired};
        if(!tcAcquireProvisioningTransport(this))return {TCProvResultCode::Busy};
        if((WiFi.getMode()&WIFI_AP)||NimBLEDevice::isInitialized()) {
            tcReleaseProvisioningTransport(this);return {TCProvResultCode::Busy};
        }
        WiFi.setAutoReconnect(false);
        if(!WiFi.mode(WIFI_STA)){tcReleaseProvisioningTransport(this);return {TCProvResultCode::InternalError};}
        {Guard lock(mutex);session->clear();running=true;stopping=false;registered=false;terminal=false;recoverWifi=false;wifiFresh=false;
            credentialsSet=false;connectPending=false;wifiAttemptStarted=false;wifiRetryPending=false;recoveryInProgress=false;wifiFailed=false;memset(&pendingWifi,0,sizeof(pendingWifi));
            if(newWindow)windowStart=millis();current=TCProvState::Advertising;}
        pc=protocomm_new();
        esp_err_t err=pc?protocomm_set_security(pc,"prov-session",&protocomm_security1,nullptr):ESP_ERR_NO_MEM;
        int sec=1;uint8_t patch=0;if(err==ESP_OK)protocomm_get_sec_version(pc,&sec,&patch);
        char version[320];snprintf(version,sizeof(version),
            "{\"prov\":{\"ver\":\"netprov-v1.2\",\"sec_ver\":1,\"sec_patch_ver\":%u,\"cap\":[\"no_pop\",\"wifi_scan\"]},"
            "\"thingscloud\":{\"ver\":\"1\",\"cap\":[\"tc-device-info\",\"tc-claim-context\"]}}",patch);
        if(err==ESP_OK)err=protocomm_set_version(pc,"proto-ver",version);
        configHandlers.wifi_get_status_handler=getStatus;configHandlers.wifi_set_config_handler=setConfig;
        configHandlers.wifi_apply_config_handler=applyConfig;configHandlers.ctx=reinterpret_cast<network_prov_ctx_t*>(this);
        scanHandlers.wifi_scan_start=scanStart;scanHandlers.wifi_scan_status=scanStatus;
        scanHandlers.wifi_scan_result=scanResult;scanHandlers.ctx=reinterpret_cast<network_prov_scan_ctx_t*>(this);
        if(err==ESP_OK)err=protocomm_add_endpoint(pc,"prov-config",network_prov_config_data_handler,&configHandlers);
        if(err==ESP_OK)err=protocomm_add_endpoint(pc,"prov-scan",network_prov_scan_handler,&scanHandlers);
        if(err==ESP_OK)err=protocomm_add_endpoint(pc,"tc-device-info",endpoint,&infoBinding);
        if(err==ESP_OK)err=protocomm_add_endpoint(pc,"tc-claim-context",endpoint,&contextBinding);
        if(err==ESP_OK)err=esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiEvent,this,&wifiHandler);
        if(err==ESP_OK)err=esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifiEvent,this,&ipHandler);
        uint64_t mac=ESP.getEfuseMac();char suffix[7];snprintf(suffix,sizeof(suffix),"%02X%02X%02X",(unsigned)((mac>>24)&255),(unsigned)((mac>>32)&255),(unsigned)((mac>>40)&255));
        String name=options.device_name_prefix+"_"+suffix;
        if(err==ESP_OK) {
            bool ready=NimBLEDevice::init(name.c_str());
            initialized=ready||NimBLEDevice::isInitialized();
            if(!ready)err=ESP_FAIL;
        }
        if(err==ESP_OK) {
            NimBLEDevice::setMTU(256);server=NimBLEDevice::createServer();
            if(!server)err=ESP_ERR_NO_MEM;
        }
        if(err==ESP_OK) {
            server->setCallbacks(this,false);server->advertiseOnDisconnect(false);
            auto* service=server->createService(NimBLEUUID(uuid,16));
            if(!service)err=ESP_ERR_NO_MEM;
            else for(auto& ep:endpoints) {
                uint8_t characteristicUUID[16];memcpy(characteristicUUID,uuid,16);
                characteristicUUID[12]=ep.id&255;characteristicUUID[13]=ep.id>>8;
                ep.characteristic=service->createCharacteristic(NimBLEUUID(characteristicUUID,16),NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::WRITE,512);
                if(!ep.characteristic){err=ESP_ERR_NO_MEM;break;}
                ep.characteristic->setCallbacks(&ep);
                auto* descriptor=ep.characteristic->createDescriptor("2901",NIMBLE_PROPERTY::READ,strlen(ep.name));
                if(!descriptor){err=ESP_ERR_NO_MEM;break;}descriptor->setValue(ep.name);
            }
            if(err==ESP_OK&&!server->start())err=ESP_FAIL;
            if(err==ESP_OK) {
                auto* advertising=NimBLEDevice::getAdvertising();
                advertising->enableScanResponse(true);
                if(!advertising->setName(name.c_str())||!advertising->addServiceUUID(service->getUUID()))err=ESP_FAIL;
                {TransportGuard transport(transportMutex);transportActive=true;}
                if(err==ESP_OK&&!advertising->start())err=ESP_FAIL;
            }
        }
        if(err!=ESP_OK){shutdown(false);return {TCProvResultCode::InternalError};}
        {Guard lock(mutex);emit(TCProvEventType::Started);}
        client->setClaimRegistrationRequired(true);return {TCProvResultCode::Ok};
    }
};
ThingsCloudBLEProvisioning::ThingsCloudBLEProvisioning():_impl(new Impl()) {}
ThingsCloudBLEProvisioning::~ThingsCloudBLEProvisioning(){
    stop();
    if(!_impl->initialized)delete _impl;
    else {
        // A host that refused to stop may still invoke its callbacks. Retain
        // its inactive owner until reboot rather than leave dangling pointers.
        _impl->client=nullptr;_impl->callback=nullptr;
    }
}
TCProvResult ThingsCloudBLEProvisioning::begin(ThingsCloudMQTT& client,const ThingsCloudBLEProvisioningOptions& o) {
    auto* p=_impl;if(!p->mutex||!p->transportMutex)return {TCProvResultCode::InternalError};
    if(p->client)return {TCProvResultCode::InvalidState};
    if(o.provisioning_mode!=client.provisioningMode()||!client.validProvisioningConfiguration()||
       !decodeUUID(o.service_uuid,p->uuid)||o.device_name_prefix.length()==0||o.device_name_prefix.length()>22||
       (o.provisioning_window_ms!=0&&(o.provisioning_window_ms<15000||o.provisioning_window_ms>600000))||o.wifi_connect_timeout_ms<1000||o.wifi_connect_timeout_ms>45000||
       o.cloud_register_timeout_ms<1000||o.cloud_register_timeout_ms>60000||
       (o.provisioning_mode==TCProvisioningMode::Manufacturer&&o.product_key!=client.productKey()))return {TCProvResultCode::InvalidArgument};
    for(size_t i=0;i<o.device_name_prefix.length();++i)if((unsigned char)o.device_name_prefix.c_str()[i]<33||(unsigned char)o.device_name_prefix.c_str()[i]>126)return {TCProvResultCode::InvalidArgument};
    uint64_t mac=ESP.getEfuseMac();char suffix[7];snprintf(suffix,sizeof(suffix),"%02X%02X%02X",(unsigned)((mac>>24)&255),(unsigned)((mac>>32)&255),(unsigned)((mac>>40)&255));
    p->session.reset(new TCProvisioningSession(o.provisioning_mode,client.getDeviceKey(),suffix,client.allowsPlaintextDevelopment()));
    if(!p->session->validIdentity())return {TCProvResultCode::InvalidArgument};
    auto mode=client.setWiFiManagementMode(TCWiFiManagement::External);if(!mode.ok())return mode;
    p->client=&client;p->options=o;
    WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);
    wifi_config_t cfg={};if(esp_wifi_get_config(WIFI_IF_STA,&cfg)!=ESP_OK)return {TCProvResultCode::InternalError};
    if(cfg.sta.ssid[0]&&o.provisioning_mode==TCProvisioningMode::Manufacturer&&!client.restoreManufacturerCredentials()) {
        // A power loss after WiFi save but before certificate completion leaves
        // no usable platform record. Recover into provisioning, not dead WiFi.
        client.setClaimRegistrationRequired(true);wifi_config_t empty={};
        if(esp_wifi_set_storage(WIFI_STORAGE_FLASH)!=ESP_OK||esp_wifi_set_config(WIFI_IF_STA,&empty)!=ESP_OK)
            return {TCProvResultCode::InternalError};
        cfg=empty;
    }
    if(cfg.sta.ssid[0]) {WiFi.begin();return {TCProvResultCode::Ok};}
    if(o.auto_start_if_unprovisioned)return p->launch(true);
    return {TCProvResultCode::Ok};
}
TCProvResult ThingsCloudBLEProvisioning::start(){return _impl->launch(true);}
void ThingsCloudBLEProvisioning::loop() {
    auto* p=_impl;if(!p->client)return;
    if(p->pendingStop) {
        if((uint32_t)(millis()-p->lastStopAttempt)>=1000&&p->shutdown(true)) {
            Guard lock(p->mutex);p->current=TCProvState::Stopped;p->emit(TCProvEventType::Stopped);
        }
        p->dispatchEvents();return;
    }
    bool timeout=false,restart=false,recover=false,finish=false,registerCloud=false,connectWifi=false,retryWifi=false;
    wifi_config_t wifiToApply={};
    TCClaimRegistrationContext context;
    {Guard lock(p->mutex);uint32_t now=millis();
        if(p->running) {
            if(!p->session->committed()&&p->windowExpired(now)) {
                p->current=TCProvState::TimedOut;p->emit(TCProvEventType::TimedOut,TCProvError::ConnectionTimeout);timeout=true;
            } else if(p->connected&&!p->secured&&(uint32_t)(now-p->connectStart)>=15000&&!p->session->committed()) {
                p->emit(TCProvEventType::Failed,TCProvError::SessionTimeout);restart=true;
            } else if(p->current==TCProvState::WifiConnecting&&!p->registered) {
                if((uint32_t)(now-p->applyStart)>=p->options.wifi_connect_timeout_ms) {
                    p->wifiFailed=true;p->wifiFailureReason=NETWORK_PROV_WIFI_STA_AP_NOT_FOUND;p->wifiRetryPending=false;
                    p->session->fail(TCProvError::WifiConnectTimeout);p->current=TCProvState::Failed;p->recoverWifi=true;
                    p->emit(TCProvEventType::Failed,TCProvError::WifiConnectTimeout);
                } else if(p->wifiFresh&&WiFi.status()==WL_CONNECTED&&(uint32_t)(now-p->applyStart)>=1000) {
                    p->registered=true;p->session->registering();p->current=TCProvState::CloudRegistering;
                    context=p->session->context();context.timeout_ms=p->options.cloud_register_timeout_ms;
                    p->emit(TCProvEventType::WifiConnected);p->emit(TCProvEventType::CloudRegistering);registerCloud=true;
                }
            }
            if(p->recoverWifi) {recover=true;p->recoverWifi=false;p->recoveryInProgress=true;}
            if(p->terminal&&(uint32_t)(now-p->finishStart)>=2000)finish=true;
            if(!p->registered&&p->current==TCProvState::Failed&&p->windowExpired(now))timeout=true;
            if(!timeout&&!restart&&!finish&&!recover&&p->current==TCProvState::WifiConnecting&&!p->registered) {
                if(p->connectPending&&(uint32_t)(now-p->applyStart)>=100) {
                    connectWifi=true;wifiToApply=p->pendingWifi;p->connectPending=false;
                } else if(p->wifiRetryPending&&(uint32_t)(now-p->lastWifiRetry)>=500) {
                    retryWifi=true;p->wifiRetryPending=false;
                }
            }
        }
    }
    if(connectWifi) {
        // Persist only after the protected Apply has committed this transaction.
        if(WiFi.scanComplete()==WIFI_SCAN_RUNNING)esp_wifi_scan_stop();
        // Invalidate the old account durably BEFORE persisting a new network.
        // If clearing fails, neither the new SSID nor its password is written.
        bool cleared=p->options.provisioning_mode!=TCProvisioningMode::Manufacturer||p->client->clearPlatformCredentials().ok();
        esp_err_t err=cleared?esp_wifi_set_storage(WIFI_STORAGE_FLASH):ESP_FAIL;
        if(err==ESP_OK)err=esp_wifi_set_config(WIFI_IF_STA,&wifiToApply);
        if(err==ESP_OK) {
            {Guard lock(p->mutex);p->wifiAttemptStarted=true;}
            err=esp_wifi_connect();
        }
        memset(&wifiToApply,0,sizeof(wifiToApply));
        if(err!=ESP_OK) {Guard lock(p->mutex);p->wifiFailed=true;p->wifiFailureReason=NETWORK_PROV_WIFI_STA_AP_NOT_FOUND;p->session->fail(TCProvError::WifiRejected);p->current=TCProvState::Failed;
            p->recoverWifi=true;p->emit(TCProvEventType::Failed,TCProvError::WifiRejected,err);}
    }
    if(retryWifi&&esp_wifi_connect()!=ESP_OK) {
        Guard lock(p->mutex);p->wifiRetryPending=true;p->lastWifiRetry=millis();
    }
    if(registerCloud&&!p->closeTransportForCloud()) {
        p->client->cancelClaimRegistration(context.request_id,true);registerCloud=false;
    }
    if(registerCloud) {
        auto result=p->client->requestClaimRegistration(context,[p](const TCClaimRegistrationResult& r) {
            Guard lock(p->mutex);if(!p->running||!p->session||p->session->context().request_id!=r.request_id)return;
            if(r.success) {p->session->ready();p->current=TCProvState::CloudReady;p->emit(TCProvEventType::CloudReady);}
            else {p->session->fail(r.error);p->current=TCProvState::Failed;p->emit(TCProvEventType::Failed,r.error,r.http_status);}
            p->terminal=true;p->finishStart=millis();
        });
        if(!result.ok()) {Guard lock(p->mutex);p->session->fail(TCProvError::InternalError);p->current=TCProvState::Failed;p->emit(TCProvEventType::Failed,TCProvError::InternalError);p->terminal=true;p->finishStart=millis();}
    }
    if(recover) {
        esp_wifi_disconnect();
        wifi_config_t empty={};esp_wifi_set_config(WIFI_IF_STA,&empty);
        Guard lock(p->mutex);p->credentialsSet=false;p->connectPending=false;p->wifiAttemptStarted=false;p->wifiRetryPending=false;p->recoveryInProgress=false;
        memset(&p->pendingWifi,0,sizeof(p->pendingWifi));
        // Keep the committed context for corrected credentials within
        // the original provisioning window. Do not extend registration budgets.

    }
    if(timeout||finish)p->shutdown(true);
    if(restart) {p->shutdown(false);if(!p->windowExpired(millis()))p->launch(false);}
    if(!p->running&&!p->pendingStop&&WiFi.status()!=WL_CONNECTED&&(uint32_t)(millis()-p->lastReconnect)>=10000) {p->lastReconnect=millis();wifi_config_t cfg={};if(esp_wifi_get_config(WIFI_IF_STA,&cfg)==ESP_OK&&cfg.sta.ssid[0])WiFi.reconnect();}
    p->dispatchEvents();
}
void ThingsCloudBLEProvisioning::stop(){if(state()==TCProvState::Stopped)return;if(_impl->client&&!_impl->shutdown(true))return;Guard lock(_impl->mutex);_impl->current=TCProvState::Stopped;_impl->emit(TCProvEventType::Stopped);}
TCProvState ThingsCloudBLEProvisioning::state() const {Guard lock(_impl->mutex);return _impl->current;}
TCProvisioningMode ThingsCloudBLEProvisioning::provisioningMode() const {return _impl->options.provisioning_mode;}
void ThingsCloudBLEProvisioning::onEvent(TCProvEventCallback cb){_impl->callback=cb;}
void ThingsCloudBLEProvisioning::resetWifi(){stop();WiFi.disconnect(false,true);}
#else
struct ThingsCloudBLEProvisioning::Impl {TCProvEventCallback callback;};
ThingsCloudBLEProvisioning::ThingsCloudBLEProvisioning():_impl(new Impl()){}
ThingsCloudBLEProvisioning::~ThingsCloudBLEProvisioning(){delete _impl;}
TCProvResult ThingsCloudBLEProvisioning::begin(ThingsCloudMQTT&,const ThingsCloudBLEProvisioningOptions&){return {TCProvResultCode::Unsupported};}
TCProvResult ThingsCloudBLEProvisioning::start(){return {TCProvResultCode::Unsupported};}
void ThingsCloudBLEProvisioning::loop(){}
void ThingsCloudBLEProvisioning::stop(){}
TCProvState ThingsCloudBLEProvisioning::state() const{return TCProvState::Idle;}
TCProvisioningMode ThingsCloudBLEProvisioning::provisioningMode() const{return TCProvisioningMode::Project;}
void ThingsCloudBLEProvisioning::onEvent(TCProvEventCallback c){_impl->callback=c;}
void ThingsCloudBLEProvisioning::resetWifi(){} // unsupported never silently resets a device
#endif
