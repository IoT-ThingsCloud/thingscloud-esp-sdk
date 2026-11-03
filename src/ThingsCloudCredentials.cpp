#include "ThingsCloudCredentials.h"
#include "ThingsCloudProvisioningProtocol.h"
#include <cstring>
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <nvs.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LittleFS.h>
#else
#include <HTTPClient.h>
#endif

bool tcCredentialsValid(const TCDeviceCredentials& c,bool plain) {
    if(!c.project_id.length()||c.project_id.length()>128||!c.device_id.length()||c.device_id.length()>128||
       !c.access_token.length()||c.access_token.length()>1024||!c.device_secret.length())return false;
    // Reject controls; MQTT query values are percent encoded before use.
    for(const String* s:{&c.project_id,&c.device_id,&c.access_token})
        for(size_t i=0;i<s->length();++i) {char v=s->c_str()[i];if((unsigned char)v<=32||(unsigned char)v>=127)return false;}
    String host;uint16_t port;
    return tcParseEndpoint(c.device_api_endpoint,false,plain,host,port)&&tcParseEndpoint(c.mqtt_endpoint,true,plain,host,port);
}
String tcEncodeCredentials(const TCDeviceCredentials& c,const String& key,const String& product) {
    DynamicJsonDocument d(16384);
    d["v"]=1;d["device_key"]=key;d["product_key"]=product;
    d["project_id"]=c.project_id;d["device_id"]=c.device_id;d["device_secret"]=c.device_secret;d["access_token"]=c.access_token;
    d["device_api_endpoint"]=c.device_api_endpoint;d["mqtt_endpoint"]=c.mqtt_endpoint;
    String blob;if(!d.overflowed())serializeJson(d,blob);return blob;
}
bool tcDecodeCredentials(const String& blob,const String& key,const String& product,bool plain,TCDeviceCredentials& c) {
    if(blob.length()>8192||!tcStrictJson((const uint8_t*)blob.c_str(),blob.length()))return false;
    DynamicJsonDocument d(blob.length()*2+1024);
    if(deserializeJson(d,blob)||!d["v"].is<int>()||d["v"].as<int>()!=1||!d["device_key"].is<const char*>()||!d["product_key"].is<const char*>()||
       String(d["device_key"].as<const char*>())!=key||String(d["product_key"].as<const char*>())!=product)return false;
    TCDeviceCredentials parsed;
    const char* names[]={"project_id","device_id","device_secret","access_token","device_api_endpoint","mqtt_endpoint"};
    String* fields[]={&parsed.project_id,&parsed.device_id,&parsed.device_secret,&parsed.access_token,&parsed.device_api_endpoint,&parsed.mqtt_endpoint};
    for(size_t i=0;i<6;++i) {if(!d[names[i]].is<const char*>())return false;*fields[i]=d[names[i]].as<const char*>();}
    if(!tcCredentialsValid(parsed,plain))return false;c=parsed;return true;
}
// NVS replaces a single CRC-protected blob atomically; no multi-key partial
// identity can be accepted. identity is verified inside the blob on every load.
bool tcLoadCredentialsBlob(String& blob,const String& identity) {
    (void)identity;
#if defined(ARDUINO_ARCH_ESP32)
    Preferences p;if(!p.begin("thingscloud",true))return false;
    size_t size=p.getBytesLength("identity");
    if(!size||size>8192){p.end();return false;}
    std::vector<char> bytes(size+1,0);
    bool ok=p.getBytes("identity",bytes.data(),size)==size;p.end();
    if(!ok||memchr(bytes.data(),0,size))return false;
    blob=bytes.data();return blob.length()==size;
#elif defined(ARDUINO_ARCH_ESP8266)
    // Never auto-format an application's filesystem. Initialize/format it in the
    // application when needed. Rename leaves either the old or new full record.
    LittleFSConfig cfg;cfg.setAutoFormat(false);LittleFS.setConfig(cfg);
    if(!LittleFS.begin())return false;
    File f=LittleFS.open("/tc-identity.json","r");if(!f||f.size()>8192)return false;
    blob=f.readString();f.close();return blob.length()>0;
#else
    (void)blob;return false;
#endif
}
bool tcClearCredentialsBlob() {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle;
    if(nvs_open("thingscloud",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t err=nvs_erase_key(handle,"identity");
    if(err==ESP_ERR_NVS_NOT_FOUND){nvs_close(handle);return true;}
    if(err==ESP_OK)err=nvs_commit(handle);
    size_t size=0;
    bool cleared=err==ESP_OK&&nvs_get_blob(handle,"identity",nullptr,&size)==ESP_ERR_NVS_NOT_FOUND;
    nvs_close(handle);return cleared;
#elif defined(ARDUINO_ARCH_ESP8266)
    LittleFSConfig cfg;cfg.setAutoFormat(false);LittleFS.setConfig(cfg);
    if(!LittleFS.begin())return false;
    for(const char* path:{"/tc-identity.new","/tc-identity.json"}) {
        if(LittleFS.exists(path)&&!LittleFS.remove(path))return false;
        if(LittleFS.exists(path))return false;
    }
    return true;
#else
    return false;
#endif
}
bool tcSaveCredentialsBlob(const String& blob,const String& identity) {
    (void)identity;if(!blob.length()||blob.length()>8192)return false;
#if defined(ARDUINO_ARCH_ESP32)
    Preferences p;if(!p.begin("thingscloud",false))return false;
    // Use a single NVS BLOB rather than NVS STR (which is limited to 4000
    // bytes). This preserves the protocol's long-address budget atomically.
    bool ok=p.putBytes("identity",blob.c_str(),blob.length())==blob.length();
    size_t size=p.getBytesLength("identity");
    if(!ok||size!=blob.length()){p.end();return false;}
    std::vector<char> readback(size);
    ok=p.getBytes("identity",readback.data(),size)==size&&memcmp(readback.data(),blob.c_str(),size)==0;
    p.end();return ok;
#elif defined(ARDUINO_ARCH_ESP8266)
    LittleFSConfig cfg;cfg.setAutoFormat(false);LittleFS.setConfig(cfg);if(!LittleFS.begin())return false;
    File f=LittleFS.open("/tc-identity.new","w");if(!f)return false;
    bool ok=f.print(blob)==blob.length();f.flush();f.close();
    if(!ok)return false;
    f=LittleFS.open("/tc-identity.new","r");ok=f&&f.readString()==blob;f.close();
    return ok&&LittleFS.rename("/tc-identity.new","/tc-identity.json");
#else
    return false;
#endif
}

String tcQueryEncode(const String& value) {
    String out;const char* hex="0123456789ABCDEF";
    for(size_t i=0;i<value.length();++i) {
        unsigned char c=value.c_str()[i];
        if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') {char text[2]={(char)c,0};out+=text;}
        else {char text[4]={'%',hex[c>>4],hex[c&15],0};out+=text;}
    }
    return out;
}
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
namespace {
class TCResponseSink:public Stream {
public:
    String value;
    size_t write(uint8_t byte) override{return write(&byte,1);}
    size_t write(const uint8_t* bytes,size_t length) override {
        if(length>8192-value.length())return 0;
        return value.concat((const char*)bytes,(unsigned int)length)?length:0;
    }
    int available() override{return 0;}
    int read() override{return -1;}
    int peek() override{return -1;}
    void flush() override{}
};
}
#endif
void tcRunCredentialJob(TCCredentialJob* j) {
    // Project transient retries stay within a single fixed deadline. A
    // manufacturer token is sent ONCE even after an ambiguous network failure.
    for(;;) {
        if(j->cancelled.load())break;
        uint32_t elapsed=(uint32_t)(millis()-j->started);
        if(elapsed>=j->budget)break;
        HTTPClient http;
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
        uint32_t remaining=j->budget-elapsed;
        WiFiClient plain;
#if defined(ARDUINO_ARCH_ESP32)
        WiFiClientSecure secure;
        // The core's default TLS handshake budget is 120 seconds, independent
        // of HTTPClient's TCP timeout. Keep it inside our bounded attempt.
        secure.setHandshakeTimeout(remaining>=5000?5:(remaining+999)/1000);
#else
        BearSSL::WiFiClientSecure secure;
#endif
        // No CA or certificate verification in this SDK version.
        secure.setInsecure();
        bool https=j->api.startsWith("https://");
#if defined(ARDUINO_ARCH_ESP32)
        http.setConnectTimeout(remaining<5000?remaining:5000);
#endif
        http.setTimeout(remaining<10000?remaining:10000);
#endif
        String path=j->api;
        if(path.endsWith("/"))path=path.substring(0,path.length()-1);
        bool manufacturer=j->context.provisioning_mode==TCProvisioningMode::Manufacturer;
        path=path+(manufacturer?"/device/v1/provisioning_certificate":"/device/v1/certificate");
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
        bool opened=https?http.begin(secure,path.c_str()):http.begin(plain,path.c_str());
#else
        bool opened=http.begin(path.c_str());
#endif
        if(opened) {
            http.addHeader("Content-Type","application/json");
            if(!manufacturer)http.addHeader("Project-Key",j->projectKey.c_str());
            String payload;
            bool encoded;
            {
                size_t capacity=JSON_OBJECT_SIZE(3)+j->deviceKey.length()+j->productKey.length()+
                    (manufacturer?j->context.provisioning_token.length():j->context.customer_id.length())+3;
                DynamicJsonDocument body(capacity);
                body["device_key"]=j->deviceKey;
                if(manufacturer) {body["product_key"]=j->productKey;body["provisioning_token"]=j->context.provisioning_token;}
                else {body["type_key"]=j->productKey;body["customer_id"]=j->context.customer_id;}
                encoded=!body.overflowed();
                if(encoded)serializeJson(body,payload);
            } // Free JSON storage before the TLS handshake, while BLE is active.
#if defined(ARDUINO_ARCH_ESP32)
            if(j->debug)Serial.printf("Provisioning certificate: free_heap=%u largest_block=%u\n",ESP.getFreeHeap(),ESP.getMaxAllocHeap());
#endif
            j->httpStatus=encoded?http.POST(payload):-1;
            if(j->debug)Serial.printf("Provisioning certificate: HTTP status=%d encoded=%d\n",j->httpStatus,encoded);
#if defined(ARDUINO_ARCH_ESP32)
            if(j->debug&&https&&encoded&&j->httpStatus<=0) {
                char error[80];
                Serial.printf("Provisioning certificate: TLS error=%d\n",secure.lastError(error,sizeof(error)));
            }
#endif
            if(j->httpStatus>=200&&j->httpStatus<300) {
                // HTTPClient de-chunks into a bounded sink. Unknown Content-
                // Length is supported without allocating an unbounded String.
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
                int size=http.getSize();
                TCResponseSink sink;
                int read=size>8192?-1:http.writeToStream(&sink);
                String response=sink.value;http.end();
                if(read<0||!response.length()) {j->error=TCProvError::CertificateRejected;break;}
#else
                String response=http.getString();http.end();
#endif
                DynamicJsonDocument d(response.length()*2+1024);
                bool valid=response.length()<=8192&&tcStrictJson((const uint8_t*)response.c_str(),response.length())&&!deserializeJson(d,response);
                if(manufacturer) {
                    valid=valid&&d["result"].is<bool>()&&d["result"].as<bool>();
                    const char* names[]={"project_id","device_id","device_secret","access_token"};
                    String* fields[]={&j->credentials.project_id,&j->credentials.device_id,&j->credentials.device_secret,&j->credentials.access_token};
                    for(size_t i=0;i<4;++i) {if(!d["info"][names[i]].is<const char*>())valid=false;else *fields[i]=d["info"][names[i]].as<const char*>();}
                    j->credentials.device_api_endpoint=j->context.device_api_endpoint;j->credentials.mqtt_endpoint=j->context.mqtt_endpoint;
                    valid=valid&&tcCredentialsValid(j->credentials,j->allowPlaintext);
                } else {
                    valid=valid&&d["result"].is<int>()&&d["result"].as<int>()==1&&d["device"]["access_token"].is<const char*>();
                    if(valid)j->credentials.access_token=d["device"]["access_token"].as<const char*>();
                    valid=valid&&j->credentials.access_token.length()>0;
                }
                if(valid) {j->success=true;j->error=TCProvError::None;}
                else j->error=TCProvError::CertificateRejected;
                break;
            }
            http.end();
            if(j->httpStatus>=400&&j->httpStatus<500&&j->httpStatus!=408&&j->httpStatus!=429) {j->error=TCProvError::CertificateRejected;break;}
        } else {
            if(j->debug)Serial.println("Provisioning certificate: HTTP initialization failed");
            http.end();
        }
        if(manufacturer)break;
#if defined(ARDUINO_ARCH_ESP32)
        for(unsigned i=0;i<100&&!j->cancelled.load()&&(uint32_t)(millis()-j->started)<j->budget;++i)delay(100);
#else
        // ESP8266 has no BLE worker. Application loop schedules its next attempt.
        break;
#endif
    }
    if((uint32_t)(millis()-j->started)>=j->budget||j->cancelled.load()) {j->success=false;j->error=TCProvError::CertificateFailed;}
    j->done.store(true,std::memory_order_release);
}
