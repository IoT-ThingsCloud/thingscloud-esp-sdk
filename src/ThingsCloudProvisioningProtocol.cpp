#include "ThingsCloudProvisioningProtocol.h"
#include <string>
#include <cstring>
#include <cctype>

namespace {
class JsonValidator {
    const char* p; const char* end;
    void space() { while(p<end&&(*p==' '||*p=='\n'||*p=='\r'||*p=='\t')) ++p; }
    bool string(std::string* key = nullptr) {
        if(p==end||*p!='"') return false;
        const char* start=p++;
        while(p<end) {
            unsigned char c=*p++;
            if(c=='"') {
                // Decode keys using the same JSON implementation, so escaped
                // and literal spellings of the same name compare equal.
                if(key) {
                    DynamicJsonDocument d((p-start)*2+64);
                    if(deserializeJson(d,start,p-start)||!d.is<const char*>()) return false;
                    JsonString s=d.as<JsonString>();
                    if(strlen(s.c_str())!=s.size()) return false;
                    key->assign(s.c_str(),s.size());
                }
                return true;
            }
            if(c<32) return false;
            if(c>=128) {
                unsigned value=0,need=0,min=0;
                if(c>=0xc2&&c<=0xdf){value=c&31;need=1;min=0x80;}
                else if(c>=0xe0&&c<=0xef){value=c&15;need=2;min=0x800;}
                else if(c>=0xf0&&c<=0xf4){value=c&7;need=3;min=0x10000;}
                else return false;
                if((size_t)(end-p)<need)return false;
                for(unsigned i=0;i<need;++i){unsigned char next=*p++;if((next&0xc0)!=0x80)return false;value=(value<<6)|(next&63);}
                if(value<min||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
            }
            if(c=='\\') {
                if(p==end) return false;
                char e=*p++;
                if(e=='u') {
                    if(end-p<4) return false;
                    unsigned v=0;
                    for(int i=0;i<4;++i) { char h=*p++; if(!isxdigit((unsigned char)h)) return false; v=v*16+(h<='9'?h-'0':tolower(h)-'a'+10); }
                    if(v==0||(v>=0xdc00&&v<=0xdfff)) return false;
                    if(v>=0xd800&&v<=0xdbff) {
                        if(end-p<6||p[0]!='\\'||p[1]!='u')return false;p+=2;
                        unsigned low=0;
                        for(int i=0;i<4;++i){char h=*p++;if(!isxdigit((unsigned char)h))return false;low=low*16+(h<='9'?h-'0':tolower(h)-'a'+10);}
                        if(low<0xdc00||low>0xdfff)return false;
                    }
                } else if(!strchr("\"\\/bfnrt",e)) return false;
            }
        }
        return false;
    }
    bool value(unsigned depth) {
        space(); if(p==end||depth>8) return false;
        if(*p=='"') return string();
        if(*p=='{'||*p=='[') {
            bool object=*p++=='{'; char close=object?'}':']';
            std::vector<std::string> keys;
            space(); if(p<end&&*p==close) {++p;return true;}
            while(p<end) {
                if(object) {
                    std::string key;
                    if(!string(&key)) return false;
                    for(const auto& k:keys) if(k==key) return false;
                    keys.push_back(key); space();
                    if(p==end||*p++!=':') return false;
                }
                if(!value(depth+1)) return false;
                space(); if(p==end) return false;
                if(*p==close) {++p;return true;}
                if(*p++!=',') return false;
                space();
            }
            return false;
        }
        for(const char* literal:{"true","false","null"}) {
            size_t n=strlen(literal);
            if((size_t)(end-p)>=n&&memcmp(p,literal,n)==0) {p+=n;return true;}
        }
        if(*p=='-') ++p;
        if(p==end) return false;
        if(*p=='0') ++p;
        else {
            if(*p<'1'||*p>'9') return false;
            while(p<end&&isdigit((unsigned char)*p)) ++p;
        }
        if(p<end&&*p=='.') {++p; const char* b=p; while(p<end&&isdigit((unsigned char)*p)) ++p; if(b==p) return false;}
        if(p<end&&(*p=='e'||*p=='E')) {++p;if(p<end&&(*p=='+'||*p=='-'))++p;const char* b=p;while(p<end&&isdigit((unsigned char)*p))++p;if(b==p)return false;}
        return true;
    }
public:
    JsonValidator(const uint8_t* d,size_t n):p((const char*)d),end((const char*)d+n) {}
    bool run() { if(!value(0)) return false; space();return p==end; }
};
bool text(JsonVariantConst v, String& result) {
    if(!v.is<const char*>()) return false;
    JsonString s=v.as<JsonString>();
    if(!s.c_str()||strlen(s.c_str())!=s.size()) return false;
    result=String(s.c_str()); return result.length()==s.size();
}
bool nonempty(JsonVariantConst v,String& s) { return text(v,s)&&s.length()>0; }
int b64(char c) {
    if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;
    if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;
}
bool decode(const String& s,std::vector<uint8_t>& out) {
    if(s.length()==0||s.length()>96||s.length()%4) return false;
    const char* p=s.c_str();
    for(size_t i=0;i<s.length();i+=4) {
        int a=b64(p[i]),b=b64(p[i+1]);bool cpad=p[i+2]=='=',dpad=p[i+3]=='=';
        int c=cpad?0:b64(p[i+2]),d=dpad?0:b64(p[i+3]);
        if(a<0||b<0||c<0||d<0||(cpad&&!dpad)||((cpad||dpad)&&i+4!=s.length()))return false;
        if((cpad&&(b&15))||(!cpad&&dpad&&(c&3)))return false;
        out.push_back((a<<2)|(b>>4));
        if(!cpad)out.push_back((b<<4)|(c>>2));if(!dpad)out.push_back((c<<6)|d);
    }
    return out.size()<=TCProvisioningSession::ChunkLimit;
}
String response(JsonDocument& doc) {
    if(doc.overflowed()||measureJson(doc)>TCProvisioningSession::FrameLimit)
        return "{\"v\":1,\"result\":false,\"error\":\"payload_too_large\"}";
    String result; serializeJson(doc,result);return result;
}
}
bool tcStrictJson(const uint8_t* d,size_t n) {return d&&n&& !memchr(d,0,n)&&JsonValidator(d,n).run();}
bool tcReadManufacturerContext(JsonObjectConst obj,TCClaimRegistrationContext& c,bool plain) {
    // The logical object deliberately has exactly these three fields.
    if(obj.size()!=3||!nonempty(obj["provisioning_token"],c.provisioning_token)||c.provisioning_token.length()>4096||
       !nonempty(obj["device_api_endpoint"],c.device_api_endpoint)||!nonempty(obj["mqtt_endpoint"],c.mqtt_endpoint)) return false;
    const char* token=c.provisioning_token.c_str();
    for(size_t i=0;i<c.provisioning_token.length();++i)if((unsigned char)token[i]<=32||(unsigned char)token[i]>=127)return false;
    String host;uint16_t port;
    return tcParseEndpoint(c.device_api_endpoint,false,plain,host,port)&&tcParseEndpoint(c.mqtt_endpoint,true,plain,host,port);
}
TCProvisioningSession::TCProvisioningSession(TCProvisioningMode m,const String& key,const String& instance,bool plain)
    :_mode(m),_key(key),_instance(instance),_allowPlaintext(plain) {_context.provisioning_mode=m;}
bool TCProvisioningSession::validIdentity() const {
    if(_key.length()==0||_key.length()>64||_instance.length()!=6)return false;
    for(size_t i=0;i<_key.length();++i) {
        unsigned char c=_key.c_str()[i];if(c<33||c>126)return false;
        if(_mode==TCProvisioningMode::Manufacturer&&!isalnum(c)&&!strchr("_.:@-",c))return false;
    }
    for(size_t i=0;i<_instance.length();++i)if(!strchr("0123456789ABCDEF",_instance.c_str()[i]))return false;
    StaticJsonDocument<768> d;
    d["v"]=1;d["op"]="get_info";d["request_id"]="12345678-1234-4234-8234-123456789abc";d["result"]=true;
    d["info"]["device_key"]=_key;d["info"]["instance_id"]=_instance;d["info"]["profile_version"]=1;
    return !d.overflowed()&&measureJson(d)<=FrameLimit;
}
const char* TCProvisioningSession::statusName() const {
    switch(_state) {
    case TCProvState::ContextReady:return "context_ready";
    case TCProvState::WifiConnecting:return "wifi_connecting";
    case TCProvState::CloudRegistering:return "cloud_registering";
    case TCProvState::CloudReady:return "cloud_ready";
    case TCProvState::Failed:case TCProvState::TimedOut:return "failed";
    default:return "session_ready";
    }
}
String TCProvisioningSession::handle(bool info,const uint8_t* data,size_t n) {
    // GATT callbacks run on the BLE host task; keep the response arena off its stack.
    DynamicJsonDocument reply(1024);
    reply["v"]=1;reply["result"]=false;
    auto error=[&](const char* e)->String {reply["error"]=e;return response(reply);};
    if(n>FrameLimit)return error("payload_too_large");
    if(!tcStrictJson(data,n))return error("invalid_request");
    DynamicJsonDocument d(n*3+512);
    if(deserializeJson(d,data,n)||!d.is<JsonObject>())return error("invalid_request");
    String op,id;
    if(!text(d["op"],op)||op.length()==0||op.length()>32||!text(d["request_id"],id)||!tcValidRequestId(id))return error("invalid_request");
    reply["op"]=op;reply["request_id"]=id;
    if(!d["v"].is<int>())return error("invalid_request");
    if(d["v"].as<int>()!=1)return error("unsupported_version");
    if(info) {
        if(op=="get_info") {reply["info"]["device_key"]=_key;reply["info"]["instance_id"]=_instance;reply["info"]["profile_version"]=1;}
        else if(op=="get_provisioning")reply["info"]["provisioning_mode"]=tcProvisioningModeName(_mode);
        else return error("unsupported_operation");
    } else {
        if((_ready&&id!=_context.request_id)||(_expected&&id!=_draftId))return error("context_conflict");
        if(op=="get_status") {reply["status"]=statusName();if(_error!=TCProvError::None)reply["error"]=tcProvisioningErrorName(_error);}
        else if(op=="set_context") {
            if(_mode!=TCProvisioningMode::Project)return error("unsupported_provisioning_mode");
            String customer;
            if(!text(d["customer_id"],customer)||!tcValidCustomerId(customer))return error("invalid_customer_id");
            for(const char* key:{"provisioning_token","device_api_endpoint","mqtt_endpoint","product_key"})if(d.containsKey(key))return error("invalid_request");
            if(_ready&&customer!=_context.customer_id)return error("context_conflict");
            if(!_ready) {_context.request_id=id;_context.customer_id=customer;_ready=true;_state=TCProvState::ContextReady;}
            reply["status"]="context_ready";
        } else if(op=="context_begin") {
            if(_mode!=TCProvisioningMode::Manufacturer)return error("unsupported_provisioning_mode");
            if(!d["size"].is<unsigned int>())return error("invalid_request");
            size_t size=d["size"].as<unsigned int>();if(size==0||size>ContextLimit)return error("invalid_provisioning_context");
            if(_expected&&size!=_expected)return error("context_conflict");
            if(!_expected) {if(_ready||_applied)return error("invalid_state");_expected=size;_draftId=id;_draft.reserve(size);}
            reply["offset"]=_draft.size();
        } else if(op=="context_chunk") {
            if(_mode!=TCProvisioningMode::Manufacturer)return error("unsupported_provisioning_mode");
            if(!_expected)return error("invalid_state");
            if(!d["offset"].is<unsigned int>())return error("invalid_request");
            size_t offset=d["offset"].as<unsigned int>();String encoded;std::vector<uint8_t> chunk;
            if(!text(d["data"],encoded)||!decode(encoded,chunk)||offset>_expected||chunk.size()>_expected-offset)return error("invalid_provisioning_context");
            if(offset>_draft.size())return error("invalid_state");
            if(offset<_draft.size()) {
                if(chunk.size()>_draft.size()-offset||memcmp(_draft.data()+offset,chunk.data(),chunk.size()))return error("context_conflict");
            } else {if(_ready||_applied)return error("invalid_state");_draft.insert(_draft.end(),chunk.begin(),chunk.end());}
            reply["offset"]=_draft.size();
        } else if(op=="context_commit") {
            if(_mode!=TCProvisioningMode::Manufacturer)return error("unsupported_provisioning_mode");
            if(!_ready) {
                if(!_expected||_draft.size()!=_expected)return error("invalid_state");
                DynamicJsonDocument full(_expected*2+1024);TCClaimRegistrationContext c;
                bool valid=tcStrictJson(_draft.data(),_draft.size())&&!deserializeJson(full,_draft.data(),_draft.size())&&full.is<JsonObject>()&&tcReadManufacturerContext(full.as<JsonObjectConst>(),c,_allowPlaintext);
                if(!valid) {clear();return error("invalid_provisioning_context");}
                c.request_id=id;c.provisioning_mode=_mode;_context=c;_ready=true;_state=TCProvState::ContextReady;
                // Keep bytes until disconnect/stop for byte-identical duplicate ACKs.
            }
            reply["status"]="context_ready";
        } else return error("unsupported_operation");
    }
    reply["result"]=true;return response(reply);
}
bool TCProvisioningSession::apply() {
    if(!_ready) {fail(TCProvError::ContextRequired);return false;}
    if(_cloudStarted||(_applied&&_state!=TCProvState::Failed))return false;
    _applied=true;_state=TCProvState::WifiConnecting;_error=TCProvError::None;return true;
}
void TCProvisioningSession::disconnected() {if(!_applied)clear();}
void TCProvisioningSession::clear() {
    _context=TCClaimRegistrationContext();_context.provisioning_mode=_mode;
    _draftId="";std::vector<uint8_t>().swap(_draft);_expected=0;
    _ready=false;_applied=false;_cloudStarted=false;_state=TCProvState::SessionReady;_error=TCProvError::None;
}
void TCProvisioningSession::fail(TCProvError e) {_state=TCProvState::Failed;_error=e;}
void TCProvisioningSession::registering() {_cloudStarted=true;_state=TCProvState::CloudRegistering;}
void TCProvisioningSession::ready() {_state=TCProvState::CloudReady;_error=TCProvError::None;_context.provisioning_token="";std::vector<uint8_t>().swap(_draft);_expected=0;}
