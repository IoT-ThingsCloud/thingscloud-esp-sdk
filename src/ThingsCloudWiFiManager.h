/*
  ThingsCloudWiFiManager.h - WiFi client for ThingsCloud.
  https://www.thingscloud.xyz
*/

#ifndef ThingsCloudWiFiManager_h
#define ThingsCloudWiFiManager_h

#include <ThingsCloudMQTT.h>

#if defined(ESP8266) || defined(ESP32)

#ifdef ESP8266
#include <core_version.h>
#endif

// #define WM_MDNS            // includes MDNS, also set MDNS with sethostname
// #define WM_FIXERASECONFIG  // use erase flash fix
// #define WM_ERASE_NVS       // esp32 erase(true) will erase NVS

#ifdef ARDUINO_ESP8266_RELEASE_2_3_0
#warning "ARDUINO_ESP8266_RELEASE_2_3_0, some WM features disabled"
// @todo check failing on platform = espressif8266@1.7.3
#define WM_NOASYNC      // esp8266 no async scan wifi
#define WM_NOCOUNTRY    // esp8266 no country
#endif

// #define esp32autoreconnect    // implement esp32 autoreconnect event listener kludge, @DEPRECATED
// autoreconnect is WORKING https://github.com/espressif/arduino-esp32/issues/653#issuecomment-405604766

#ifdef ESP8266

extern "C"
{
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#ifdef WM_MDNS
#include <ESP8266mDNS.h>
#endif

#ifndef ESP_getChipId
#define ESP_getChipId() ESP.getChipId()
#endif

#define WM_WIFIOPEN ENC_TYPE_NONE

#elif defined(ESP32)

#include <WiFi.h>
#include <esp_wifi.h>

#ifndef ESP_getChipId
#define ESP_getChipId() (uint64_t) ESP.getEfuseMac()
#endif

#define WM_WIFIOPEN WIFI_AUTH_OPEN

#ifndef WEBSERVER_H
#include <WebServer.h>
#endif

#ifdef WM_ERASE_NVS
#include <nvs.h>
#include <nvs_flash.h>
#endif

#ifdef WM_MDNS
#include <ESPmDNS.h>
#endif

#endif

#include <DNSServer.h>
#include <memory>

const char HTTP_STATUS_OFFPW[] PROGMEM = "Authentication Failure"; // STATION_WRONG_PASSWORD,  no eps32
const char HTTP_STATUS_OFFNOAP[] PROGMEM = "AP not found";         // WL_NO_SSID_AVAIL
const char HTTP_STATUS_OFFFAIL[] PROGMEM = "Could not Connect";    // WL_CONNECT_FAILED

// Provisioning diagnostics
#ifdef ESP32
const char HTTP_INFO_esphead[] PROGMEM = "esp32";
#else
const char HTTP_INFO_esphead[] PROGMEM = "esp8266";
#endif

const char HTTP_INFO_wifihead[] PROGMEM = "WiFi";

const char S_debugPrefix[] PROGMEM = "*wm:";
const char S_NA[] PROGMEM = "Unknown";

// debug strings
const char D_HR[] PROGMEM = "--------------------";

// Strings
const char S_ip[] PROGMEM = "ip";
const char S_gw[] PROGMEM = "gw";
const char S_sn[] PROGMEM = "sn";
const char S_dns[] PROGMEM = "dns";

// http
const char HTTP_HEAD_JSON[] PROGMEM = "application/json";
const char HTTP_HEAD_CORS[] PROGMEM = "Access-Control-Allow-Origin";
const char HTTP_HEAD_CORS_ALLOW_ALL[] PROGMEM = "*";

const char *const WIFI_STA_STATUS[] PROGMEM{
    "WL_IDLE_STATUS",           // 0 STATION_IDLE
    "WL_NO_SSID_AVAIL",         // 1 STATION_NO_AP_FOUND
    "WL_SCAN_COMPLETED",        // 2
    "WL_CONNECTED",             // 3 STATION_GOT_IP
    "WL_CONNECT_FAILED",        // 4 STATION_CONNECT_FAIL, STATION_WRONG_PASSWORD(NI)
    "WL_CONNECTION_LOST",       // 5
    "WL_DISCONNECTED",          // 6
    "WL_STATION_WRONG_PASSWORD" // 7 KLUDGE
};

#ifdef ESP32
const char *const AUTH_MODE_NAMES[] PROGMEM{
    "OPEN",
    "WEP",
    "WPA_PSK",
    "WPA2_PSK",
    "WPA_WPA2_PSK",
    "WPA2_ENTERPRISE",
    "MAX"};
#elif defined(ESP8266)
const char *const AUTH_MODE_NAMES[] PROGMEM{
    "",
    "",
    "WPA_PSK", // 2 ENC_TYPE_TKIP
    "",
    "WPA2_PSK", // 4 ENC_TYPE_CCMP
    "WEP",      // 5 ENC_TYPE_WEP
    "",
    "OPEN",         // 7 ENC_TYPE_NONE
    "WPA_WPA2_PSK", // 8 ENC_TYPE_AUTO
};
#endif

const char *const WIFI_MODES[] PROGMEM = {"NULL", "STA", "AP", "STA+AP"};

#ifdef ESP32
// as 2.5.2
const wifi_country_t WM_COUNTRY_US{"US", 1, 11, CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER, WIFI_COUNTRY_POLICY_AUTO};
const wifi_country_t WM_COUNTRY_CN{"CN", 1, 13, CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER, WIFI_COUNTRY_POLICY_AUTO};
const wifi_country_t WM_COUNTRY_JP{"JP", 1, 14, CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER, WIFI_COUNTRY_POLICY_AUTO};
#elif defined(ESP8266) && !defined(WM_NOCOUNTRY)
const wifi_country_t WM_COUNTRY_US{"US", 1, 11, WIFI_COUNTRY_POLICY_AUTO};
const wifi_country_t WM_COUNTRY_CN{"CN", 1, 13, WIFI_COUNTRY_POLICY_AUTO};
const wifi_country_t WM_COUNTRY_JP{"JP", 1, 14, WIFI_COUNTRY_POLICY_AUTO};
#endif

class ThingsCloudWiFiManager
{
public:
    ThingsCloudWiFiManager(Stream &consolePort);
    ThingsCloudWiFiManager(char const *apNamePrefix, char const *apPassword = NULL);
    ~ThingsCloudWiFiManager();
    void WiFiManagerInit();

    // auto connect to saved wifi, or custom, and start config portal on failures
    boolean autoConnect();

    // manually start the config portal, autoconnect does this automatically on connect failure
    boolean startConfigPortal(); // auto generates apname

    // manually stop the config portal if started manually, stop immediatly if non blocking, flag abort if blocking
    bool stopConfigPortal();

    // Run webserver processing, if setConfigPortalBlocking(false)
    boolean process();

    // get the AP name of the config portal, so it can be used in the callback
    String getConfigPortalSSID();
    int getRSSIasQuality(int RSSI);

    // erase wifi credentials
    void resetSettings();

    // reboot esp
    void reboot();

    // disconnect wifi, without persistent saving or erasing
    bool disconnect();

    // erase esp
    bool erase();
    bool erase(bool opt);

    void linkMQTTClient(ThingsCloudMQTT *client);

    void setDeviceKey(String deviceKey);

    String getCustomerId();

    // SET CALLBACKS

    // called after AP mode and config portal has started
    void setAPCallback(std::function<void(ThingsCloudWiFiManager *)> func);

    // called after webserver has started
    void setWebServerCallback(std::function<void()> func);

    // called when settings reset have been triggered
    void setConfigResetCallback(std::function<void()> func);

    // called when wifi settings have been changed and connection was successful ( or setBreakAfterConfig(true) )
    void setSaveConfigCallback(std::function<void()> func);

    // sets timeout before AP,webserver loop ends and exits even if there has been no setup.
    // useful for devices that failed to connect at some point and got stuck in a webserver loop
    // in seconds; applies to both blocking and non-blocking config portals
    void setConfigPortalTimeout(unsigned long seconds);
    void setTimeout(unsigned long seconds); // @deprecated, alias

    // sets timeout for which to attempt connecting, useful if you get a lot of failed connects
    void setConnectTimeout(unsigned long seconds);

    // sets number of retries for autoconnect, force retry after wait failure exit
    void setConnectRetries(uint8_t numRetries); // default 1

    // sets timeout for which to attempt connecting on saves, useful if there are bugs in esp waitforconnectloop
    void setSaveConnectTimeout(unsigned long seconds);

    // lets you disable automatically connecting after provisioning credentials are saved
    void setSaveConnect(bool connect = true);

    // toggle debug output
    void setDebugOutput(boolean debug);
    void setDebugOutput(boolean debug, String prefix); // log line prefix, default "*wm:"

    // set min quality percentage to include in scan, defaults to 8% if not specified
    void setMinimumSignalQuality(int quality = 8);

    // sets a custom ip /gateway /subnet configuration
    void setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);

    // sets config for a static IP
    void setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);

    // sets config for a static IP with DNS
    void setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn, IPAddress dns);

    // if this is set, it will exit after config, even if connection is unsuccessful.
    void setBreakAfterConfig(boolean shouldBreak);

    // if this is set, portal will be blocking and wait until save or exit,
    // is false user must manually `process()` to handle config portal,
    // process() also checks the configured timeout in non-blocking mode
    void setConfigPortalBlocking(boolean shouldBlock);

    // if this is true, remove duplicated Access Points - defaut true
    void setRemoveDuplicateAPs(boolean removeDuplicates);

    // setter for ESP wifi.persistent so we can remember it and restore user preference, as WIFi._persistent is protected
    void setRestorePersistent(boolean persistent);

    // if false, disable captive portal redirection
    void setCaptivePortalEnable(boolean enabled);

    // if false, timeout captive portal even if a STA client connected to softAP (false), suggest disabling if captiveportal is open
    void setAPClientCheck(boolean enabled);

    // if true, enable autoreconnecting
    void setWiFiAutoReconnect(boolean enabled);

    // if true (default) then start the config portal from autoConnect if connection failed
    void setEnableConfigPortal(boolean enable);

    // set a custom hostname, sets sta and ap dhcp client id for esp32, and sta for esp8266
    bool setHostname(const char *hostname);
    bool setHostname(String hostname);

    // set ap channel
    void setWiFiAPChannel(int32_t channel);

    // set ap hidden
    void setWiFiAPHidden(bool hidden); // default false

    // clean connect, always disconnect before connecting
    void setCleanConnect(bool enable); // default false

    // get last connection result, includes autoconnect and wifisave
    uint8_t getLastConxResult();

    // get a status as string
    String getWLStatusString(uint8_t status);
    String getWLStatusString();

    // get wifi mode as string
    String getModeString(uint8_t mode);

    // check if the module has a saved ap to connect to
    bool getWiFiIsSaved();

    // helper to get saved password, if persistent get stored, else get current if connected
    String getWiFiPass(bool persistent = true);

    // helper to get saved ssid, if persistent get stored, else get current if connected
    String getWiFiSSID(bool persistent = true);

    // debug output the softap config
    void debugSoftAPConfig();

    // debug output platform info and versioning
    void debugPlatformInfo();

    // helper for html
    String htmlEntities(String str);

    // set the country code for wifi settings, CN
    void setCountry(String cc);

    // get default ap esp uses , esp_chipid etc
    String getDefaultAPName();

    // get esp chip unique id
    String getEspChipUniqueId();

    // set port of provisioning server, defaults to 8080
    void setHttpPort(uint16_t port);

    // check if config portal is active (true)
    bool getConfigPortalActive();

    // to preload autoconnect for test fixtures or other uses that skip esp sta config
    bool preloadWiFi(String ssid, String pass);

    // get hostname helper
    String getWiFiHostname();

    std::unique_ptr<DNSServer> dnsServer;

#ifdef ESP32
    using WM_WebServer = WebServer;
#else
    using WM_WebServer = ESP8266WebServer;
#endif

    std::unique_ptr<WM_WebServer> server;

private:
    // ip configs @todo struct ?
    IPAddress _ap_static_ip;
    IPAddress _ap_static_gw;
    IPAddress _ap_static_sn;
    IPAddress _sta_static_ip;
    IPAddress _sta_static_gw;
    IPAddress _sta_static_sn;
    IPAddress _sta_static_dns;

    // defaults
    const byte DNS_PORT = 53;
    String _apName = "";
    String _apPassword = "";
    String _ssid = "";
    String _pass = "";
    String _defaultssid = "";
    String _defaultpass = "";
    String _deviceKey = "";
    String _customerId = "";
    TCClaimRegistrationContext _claimContext;
    bool _claimContextReady = false;
    ThingsCloudMQTT *_mqttClient = NULL;

    // options flags
    unsigned long _configPortalTimeout = 0;   // ms before the AP config portal times out
    unsigned long _connectTimeout = 0;        // ms stop trying to connect to ap if set
    unsigned long _saveTimeout = 0;           // ms stop trying to connect to ap on saves, in case bugs in esp waitforconnectresult
    unsigned long _configPortalStart = 0;     // ms config portal start time (updated for timeouts)
    WiFiMode_t _usermode = WIFI_STA;          // Default user mode
    String _wifissidprefix = "esp32";         // auto apname prefix prefix+chipid
    uint8_t _lastconxresult = WL_IDLE_STATUS; // store last result when doing connect operations
    int _numNetworks = 0;                     // init index for numnetworks wifiscans
    unsigned long _lastscan = 0;              // ms for timing wifi scans
    unsigned long _startscan = 0;             // ms for timing wifi scans
    int _cpclosedelay = 2000;                 // delay before wifisave, prevents captive portal from closing to fast.
    bool _cleanConnect = false;               // disconnect before connect in connectwifi, increases stability on connects
    bool _connectonsave = true;               // connect to wifi when saving creds
    bool _disableSTA = false;                 // disable sta when starting ap, always
    bool _disableSTAConn = true;              // disable sta when starting ap, if sta is not connected ( stability )
    bool _channelSync = false;                // use same wifi sta channel when starting ap
    int32_t _apChannel = 0;                   // channel to use for ap
    bool _apHidden = false;                   // store softap hidden value
    uint16_t _httpPort = 8080;                // port for webserver
    uint8_t _connectRetries = 1;   // number of sta connect retries, force reconnect, wait loop (connectimeout) does not always work and first disconnect bails
    unsigned long _startconn = 0;  // ms for timing wifi connects
    bool _aggresiveReconn = false; // use an agrressive reconnect strategy, WILL delay conxs
                                   // on some conn failure modes will add delays and many retries to work around esp and ap bugs, ie, anti de-auth protections
    bool _allowExit = true;        // allow exit non blocking

#ifdef ESP32
    wifi_event_id_t wm_event_id;
    static uint8_t _lastconxresulttmp; // tmp var for esp32 callback
#endif

#ifndef WL_STATION_WRONG_PASSWORD
    uint8_t WL_STATION_WRONG_PASSWORD = 7; // @kludge define a WL status for wrong password
#endif

    // parameter options
    int _minimumQuality = -1;                // filter wifiscan ap by this rssi
    boolean _removeDuplicateAPs = true;      // remove dup aps from wifiscan
    boolean _shouldBreakAfterConfig = false; // stop configportal on save failure
    boolean _configPortalIsBlocking = true;  // configportal enters blocking loop
    boolean _enableCaptivePortal = false;    // enable captive portal redirection
    boolean _userpersistent = true;          // users preffered persistence to restore
    boolean _wifiAutoReconnect = true;       // there is no platform getter for this, we must assume its true and make it so
    boolean _apClientCheck = false;          // keep cp alive if ap have station
    boolean _enableConfigPortal = true;      // use config portal if autoconnect failed
    String _hostname = "";                   // hostname for esp8266 for dhcp, and or MDNS

    // Optional scan optimizations; runtime caching and refresh are handled by WiFi_scanNetworks().
    boolean _preloadwifiscan = false;    // preload wifiscan if true
    boolean _asyncScan = false;          // perform wifi network scan async

    String _wificountry = ""; // country code, @todo define in strings lang

    // wrapper functions for handling setting and unsetting persistent for now.
    bool esp32persistent = false;
    bool _hasBegun = false; // flag wm loaded,unloaded
    void _begin();
    void _end();

    void setupConfigPortal();
    bool shutdownConfigPortal();
    bool setupHostname(bool restart);

    bool startAP();
    void setupDNSD();

    uint8_t connectWifi(String ssid, String pass, bool connect = true);
    bool setSTAConfig();
    bool wifiConnectDefault();
    bool wifiConnectNew(String ssid, String pass, bool connect = true);

    uint8_t waitForConnectResult();
    uint8_t waitForConnectResult(uint32_t timeout);
    void updateConxResult(uint8_t status);

    // webserver handlers
    void handleWifiList();
    void handleWifiSave();
    void handleInfo();
    void handleProvisioning();
    void handleReset();
    void handleNotFound();
    void handleWiFiStatus();

    boolean configPortalHasTimeout();
    uint8_t processConfigPortal();

    // wifi platform abstractions
    bool WiFi_Mode(WiFiMode_t m);
    bool WiFi_Mode(WiFiMode_t m, bool persistent);
    bool WiFi_Disconnect();
    bool WiFi_enableSTA(bool enable);
    bool WiFi_enableSTA(bool enable, bool persistent);
    bool WiFi_eraseConfig();
    uint8_t WiFi_softap_num_stations();
    bool WiFi_hasAutoConnect();
    void WiFi_autoReconnect();
    String WiFi_SSID(bool persistent = true) const;
    String WiFi_psk(bool persistent = true) const;
    bool WiFi_scanNetworks();
    bool WiFi_scanNetworks(bool force, bool async);
    void WiFi_scanComplete(int networksFound);
    bool WiFiSetCountry();

#ifdef ESP32

// check for arduino or system event system, handle esp32 arduino v2 and IDF
#if defined(ESP_ARDUINO_VERSION) && defined(ESP_ARDUINO_VERSION_VAL)

#define WM_ARDUINOVERCHECK ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)

#ifdef WM_ARDUINOVERCHECK
#define WM_ARDUINOEVENTS
#endif

#endif

#ifdef WM_ARDUINOEVENTS
    void WiFiEvent(WiFiEvent_t event, arduino_event_info_t info);
#else
    void WiFiEvent(WiFiEvent_t event, system_event_info_t info);
#endif
#endif

    String getScanItems();
    boolean validApPassword();
    String encryptionTypeStr(uint8_t authmode);

    // flags
    boolean connect;
    boolean abort;
    boolean configPortalActive = false;
    boolean portalTimeoutResult = false;
    boolean portalAbortResult = false;
    boolean storeSTAmode = true; // option store persistent STA mode in connectwifi
    int timer = 0;               // timer for debug throttle for numclients, and portal timeout messages

    // debugging
    typedef enum
    {
        DEBUG_ERROR = 0,
        DEBUG_NOTIFY = 1, // default stable
        DEBUG_VERBOSE = 2,
        DEBUG_DEV = 3, // default dev
        DEBUG_MAX = 4
    } wm_debuglevel_t;

    boolean _debug = true;
    String _debugPrefix = FPSTR(S_debugPrefix);

    wm_debuglevel_t debugLvlShow = DEBUG_VERBOSE; // at which level start showing [n] level tags

// build debuglevel support
// @todo use DEBUG_ESP_x?

// Set default debug level
#ifndef WM_DEBUG_LEVEL
#define WM_DEBUG_LEVEL DEBUG_VERBOSE // development default, not release
#endif

// override debug level OFF
#ifdef WM_NODEBUG
#undef WM_DEBUG_LEVEL
#endif

#ifdef WM_DEBUG_LEVEL
    uint8_t _debugLevel = (uint8_t)WM_DEBUG_LEVEL;
#else
    uint8_t _debugLevel = DEBUG_VERBOSE; // default debug level
#endif

// @todo use DEBUG_ESP_PORT ?
#ifdef WM_DEBUG_PORT
    Stream &_debugPort = WM_DEBUG_PORT;
#else
    Stream &_debugPort = Serial;         // debug output stream ref
#endif

    template <typename Generic>
    void DEBUG_WM(Generic text);

    template <typename Generic>
    void DEBUG_WM(wm_debuglevel_t level, Generic text);
    template <typename Generic, typename Genericb>
    void DEBUG_WM(Generic text, Genericb textb);
    template <typename Generic, typename Genericb>
    void DEBUG_WM(wm_debuglevel_t level, Generic text, Genericb textb);

    // callbacks
    // @todo use cb list (vector) maybe event ids, allow no return value
    std::function<void(ThingsCloudWiFiManager *)> _apcallback;
    std::function<void()> _webservercallback;
    std::function<void()> _savewificallback;
    std::function<void()> _resetcallback;

    template <class T>
    auto optionalIPFromString(T *obj, const char *s) -> decltype(obj->fromString(s))
    {
        return obj->fromString(s);
    }
    auto optionalIPFromString(...) -> bool
    {
        // DEBUG_WM("NO fromString METHOD ON IPAddress, you need ESP8266 core 2.1.0 or newer for Custom IP configuration to work.");
        return false;
    }
};

#endif

#endif
