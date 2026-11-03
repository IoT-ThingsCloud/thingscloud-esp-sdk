#include "ThingsCloudProvisioning.h"
#include <cstring>
#include <cctype>
#include <atomic>

const char* tcProvisioningModeName(TCProvisioningMode m) { return m == TCProvisioningMode::Project ? "project" : "manufacturer"; }
const char* tcProvisioningErrorName(TCProvError e) {
    switch(e) {
    case TCProvError::None: return "";
    case TCProvError::BluetoothFailure: return "bluetooth_failed";
    case TCProvError::ConnectionTimeout: return "connection_timeout";
    case TCProvError::SessionTimeout: return "session_timeout";
    case TCProvError::ContextRequired: return "context_required";
    case TCProvError::ContextConflict: return "context_conflict";
    case TCProvError::WifiConnectTimeout: return "wifi_timeout";
    case TCProvError::WifiRejected: return "wifi_failed";
    case TCProvError::CertificateFailed: return "certificate_failed";
    case TCProvError::CertificateRejected: return "certificate_rejected";
    case TCProvError::Cancelled: return "cancelled";
    case TCProvError::InvalidRequest: return "invalid_request";
    default: return "internal_error";
    }
}
bool tcValidRequestId(const String& s) {
    if(s.length()!=36) return false;
    const char* p=s.c_str();
    for(size_t i=0;i<36;++i) {
        if(i==8||i==13||i==18||i==23) { if(p[i]!='-') return false; }
        else if(!isxdigit((unsigned char)p[i])) return false;
    }
    return p[14]=='4' && strchr("89abAB",p[19]);
}
bool tcValidCustomerId(const String& s) {
    // Treat the platform's customer identifier as an opaque, case-sensitive string.
    if(s.length()==0||s.length()>128) return false;
    for(size_t i=0;i<s.length();++i) {
        unsigned char c=(unsigned char)s.c_str()[i];
        if(!isalnum(c)&&c!='_'&&c!='-') return false;
    }
    return true;
}
bool tcValidProductKey(const String& s) {
    if(s.length()<1||s.length()>128) return false;
    for(size_t i=0;i<s.length();++i) {
        char c=s.c_str()[i];
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-')) return false;
    }
    return true;
}
namespace {
bool validIPv6(const char* p,size_t n) {
    size_t i=0;unsigned groups=0;bool compressed=false;
    if(!n)return false;
    if(p[0]==':') {if(n<2||p[1]!=':')return false;compressed=true;i=2;}
    while(i<n) {
        size_t start=i;
        while(i<n&&isxdigit((unsigned char)p[i]))++i;
        if(i==start||i-start>4)return false;
        if(i<n&&p[i]=='.') {
            // An embedded IPv4 address occupies the final two groups.
            i=start;for(unsigned part=0;part<4;++part) {
                unsigned v=0,digits=0;
                while(i<n&&isdigit((unsigned char)p[i])) {v=v*10+p[i++]-'0';if(++digits>3||v>255)return false;}
                if(!digits)return false;
                if(part<3) {if(i==n||p[i++]!='.')return false;}
            }
            if(i!=n)return false;groups+=2;break;
        }
        if(++groups>8)return false;
        if(i==n)break;
        if(p[i++]!=':'||i==n)return false;
        if(p[i]==':') {if(compressed)return false;compressed=true;++i;}
    }
    return compressed?groups<8:groups==8;
}
}
bool tcParseEndpoint(const String& uri, bool mqtt, bool plain, String& host, uint16_t& port) {
    const char* p=uri.c_str(); size_t n=uri.length();
    if(n==0||n>8192||strlen(p)!=n) return false;
    for(size_t i=0;i<n;++i) if((unsigned char)p[i]<=32||(unsigned char)p[i]>=127||p[i]=='@'||p[i]=='?'||p[i]=='#'||p[i]=='\\') return false;
    size_t start=0;
    if(mqtt) {
        if(uri.startsWith("mqtt://")) {start=7; port=1883;}
        else return false;
    } else {
        if(uri.startsWith("https://")) {start=8; port=443;}
        else if(plain&&uri.startsWith("http://")) {start=7; port=80;}
        else return false;
    }
    size_t end=start; while(end<n&&p[end]!='/') ++end;
    // First implementation accepts DNS/IPv4 and bracketed IPv6 authorities.
    size_t split=end;
    if(start<end&&p[start]=='[') {
        size_t close=start+1; while(close<end&&p[close]!=']') ++close;
        if(close==end||close==start+1) return false;
        if(!validIPv6(p+start+1,close-start-1))return false;
        if(close+1<end) {if(p[close+1]!=':') return false; split=close+1;}
        host=uri.substring(start+1,close);
    } else {
        for(size_t i=start;i<end;++i) if(p[i]==':') {if(split!=end) return false; split=i;}
        if(split==start) return false;
        for(size_t i=start;i<split;++i) if(!isalnum((unsigned char)p[i])&&p[i]!='.'&&p[i]!='-') return false;
        host=uri.substring(start,split);
        if(host.length()==0||host.length()>253||p[start]=='.'||p[start]=='-'||p[split-1]=='.'||p[split-1]=='-') return false;
    }
    if(split<end) {
        if(split+1==end) return false;
        uint32_t v=0;
        for(size_t i=split+1;i<end;++i) {if(p[i]<'0'||p[i]>'9') return false; v=v*10+p[i]-'0'; if(v>65535) return false;}
        if(v==0) return false; port=(uint16_t)v;
    }
    if(mqtt&&end<n&&!(end+1==n&&p[end]=='/')) return false;
    return true;
}

#if defined(ARDUINO_ARCH_ESP8266)
namespace {void* tcTransportOwner=nullptr;}
bool tcAcquireProvisioningTransport(void* owner){if(tcTransportOwner)return false;tcTransportOwner=owner;return true;}
void tcReleaseProvisioningTransport(void* owner){if(tcTransportOwner==owner)tcTransportOwner=nullptr;}
bool tcProvisioningTransportBusy(){return tcTransportOwner!=nullptr;}
#else
namespace {std::atomic<void*> tcTransportOwner{nullptr};}
bool tcAcquireProvisioningTransport(void* owner) {void* expected=nullptr;return tcTransportOwner.compare_exchange_strong(expected,owner);}
void tcReleaseProvisioningTransport(void* owner) {void* expected=owner;tcTransportOwner.compare_exchange_strong(expected,nullptr);}
bool tcProvisioningTransportBusy(){return tcTransportOwner.load()!=nullptr;}
#endif
