/*
  ThingsCloudWiFiManager.cpp - WiFi client for ThingsCloud.
  https://www.thingscloud.xyz
*/

#include "ThingsCloudWiFiManager.h"
#include <ArduinoJson.h>

#if defined(ESP8266) || defined(ESP32)

#ifdef ESP32
uint8_t ThingsCloudWiFiManager::_lastconxresulttmp = WL_IDLE_STATUS;
#endif

/**
 * --------------------------------------------------------------------------------
 *  ThingsCloudWiFiManager
 * --------------------------------------------------------------------------------
 **/

// constructors
ThingsCloudWiFiManager::ThingsCloudWiFiManager(Stream &consolePort) : _debugPort(consolePort)
{
    WiFiManagerInit();
}

ThingsCloudWiFiManager::ThingsCloudWiFiManager(char const *apNamePrefix, char const *apPassword)
{
    // setup AP
    _wifissidprefix = apNamePrefix;
    _apName = getDefaultAPName();
    _apPassword = apPassword;
    WiFiManagerInit();
}

void ThingsCloudWiFiManager::WiFiManagerInit()
{
    if (_debug && _debugLevel >= DEBUG_DEV)
        debugPlatformInfo();
}

// destructor
ThingsCloudWiFiManager::~ThingsCloudWiFiManager()
{
    _end();

#ifdef ESP32
    WiFi.removeEvent(wm_event_id);
#endif
}

void ThingsCloudWiFiManager::_begin()
{
    if (_hasBegun)
        return;
    _hasBegun = true;

#ifndef ESP32
    WiFi.persistent(false); // disable persistent so scannetworks and mode switching do not cause overwrites
#endif
}

void ThingsCloudWiFiManager::_end()
{
    _hasBegun = false;
    if (_userpersistent)
        WiFi.persistent(true); // reenable persistent, there is no getter we rely on _userpersistent
                               // if(_usermode != WIFI_OFF) WiFi.mode(_usermode);
}

/**
 * [autoConnect description]
 * @access public
 * @param  {[type]} char const         *apName     [description]
 * @param  {[type]} char const         *apPassword [description]
 * @return {[type]}      [description]
 */
boolean ThingsCloudWiFiManager::autoConnect()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("AutoConnect"));
#endif
    if (getWiFiIsSaved())
    {
        _startconn = millis();
        _begin();

        // attempt to connect using saved settings, on fail fallback to AP config portal
        if (!WiFi.enableSTA(true))
        {
// handle failure mode Brownout detector etc.
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_ERROR, F("[FATAL] Unable to enable wifi!"));
#endif
            return false;
        }

        WiFiSetCountry();

#ifdef ESP32
        if (esp32persistent)
            WiFi.persistent(false); // disable persistent for esp32 after esp_wifi_start or else saves wont work
#endif

        _usermode = WIFI_STA; // When using autoconnect , assume the user wants sta mode on permanently.

        // no getter for autoreconnectpolicy before this
        // https://github.com/esp8266/Arduino/pull/4359
        // so we must force it on else, if not connectimeout then waitforconnectionresult gets stuck endless loop
        WiFi_autoReconnect();

        // set hostname before stating
        if (_hostname != "")
        {
            setupHostname(true);
        }

        // if already connected, or try stored connect
        // @note @todo ESP32 has no autoconnect, so connectwifi will always be called unless user called begin etc before
        // @todo check if correct ssid == saved ssid when already connected
        bool connected = false;
        if (WiFi.status() == WL_CONNECTED)
        {
            connected = true;
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(F("AutoConnect: ESP Already Connected"));
#endif
            setSTAConfig();
            // @todo not sure if this check makes sense, causes dup setSTAConfig in connectwifi,
            // and we have no idea WHAT we are connected to
        }

        if (connected || connectWifi(_defaultssid, _defaultpass) == WL_CONNECTED)
        {
// connected
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(F("AutoConnect: SUCCESS"));
            DEBUG_WM(DEBUG_VERBOSE, F("Connected in"), (String)((millis() - _startconn)) + " ms");
            DEBUG_WM(F("STA IP Address:"), WiFi.localIP());
#endif
            _lastconxresult = WL_CONNECTED;

            if (_hostname != "")
            {
#ifdef WM_DEBUG_LEVEL
                DEBUG_WM(DEBUG_DEV, F("hostname: STA: "), getWiFiHostname());
#endif
            }
            return true; // connected success
        }

#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("AutoConnect: FAILED"));
#endif
    }
    else
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("No Credentials are Saved, skipping connect"));
#endif
    }

    // possibly skip the config portal
    if (!_enableConfigPortal)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("enableConfigPortal: FALSE, skipping "));
#endif

        return false; // not connected and not cp
    }

    // not connected start configportal
    bool res = startConfigPortal();
    return res;
}

bool ThingsCloudWiFiManager::setupHostname(bool restart)
{
    if (_hostname == "")
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_DEV, F("No Hostname to set"));
#endif
        return false;
    }
    else
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Setting Hostnames: "), _hostname);
#endif
    }
    bool res = true;
#ifdef ESP8266
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Setting WiFi hostname"));
#endif
    res = WiFi.hostname(_hostname.c_str());
// #ifdef ESP8266MDNS_H
#ifdef WM_MDNS
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Setting MDNS hostname, tcp 80"));
#endif
    if (MDNS.begin(_hostname.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
    }
#endif
#elif defined(ESP32)
    // @note hostname must be set after STA_START
    delay(200); // do not remove, give time for STA_START

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Setting WiFi hostname"));
#endif

    res = WiFi.setHostname(_hostname.c_str());
    // #ifdef ESP32MDNS_H
#ifdef WM_MDNS
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Setting MDNS hostname, tcp 80"));
#endif
    if (MDNS.begin(_hostname.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
    }
#endif
#endif

#ifdef WM_DEBUG_LEVEL
    if (!res)
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] hostname: set failed!"));
#endif

    if (restart && (WiFi.status() == WL_CONNECTED))
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("reconnecting to set new hostname"));
#endif
        // WiFi.reconnect(); // This does not reset dhcp
        WiFi_Disconnect();
        delay(200); // do not remove, need a delay for disconnect to change status()
    }

    return res;
}

// CONFIG PORTAL
bool ThingsCloudWiFiManager::startAP()
{
    bool ret = true;
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("StartAP with SSID: "), _apName);
#endif

#ifdef ESP8266
    // @bug workaround for bug #4372 https://github.com/esp8266/Arduino/issues/4372
    if (!WiFi.enableAP(true))
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] enableAP failed!"));
#endif
        return false;
    }
    delay(500); // workaround delay
#endif

    // setup optional soft AP static ip config
    if (_ap_static_ip)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("Custom AP IP/GW/Subnet:"));
#endif
        if (!WiFi.softAPConfig(_ap_static_ip, _ap_static_gw, _ap_static_sn))
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_ERROR, F("[ERROR] softAPConfig failed!"));
#endif
        }
    }

    //@todo add callback here if needed to modify ap but cannot use setAPStaticIPConfig
    //@todo rework wifi channelsync as it will work unpredictably when not connected in sta

    int32_t channel = 0;
    if (_channelSync)
        channel = WiFi.channel();
    else
        channel = _apChannel;

    if (channel > 0)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Starting AP on channel:"), channel);
#endif
    }

    // start soft AP with password or anonymous
    // default channel is 1 here and in esplib, @todo just change to default remove conditionals
    if (_apPassword != "")
    {
        if (channel > 0)
        {
            ret = WiFi.softAP(_apName.c_str(), _apPassword.c_str(), channel, _apHidden);
        }
        else
        {
            ret = WiFi.softAP(_apName.c_str(), _apPassword.c_str(), 1, _apHidden); // password option
        }
    }
    else
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("AP has anonymous access!"));
#endif
        if (channel > 0)
        {
            ret = WiFi.softAP(_apName.c_str(), "", channel, _apHidden);
        }
        else
        {
            ret = WiFi.softAP(_apName.c_str(), "", 1, _apHidden);
        }
    }

    if (_debugLevel >= DEBUG_DEV)
        debugSoftAPConfig();

    // @todo add softAP retry here

    delay(500); // slight delay to make sure we get an AP IP
#ifdef WM_DEBUG_LEVEL
    if (!ret)
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] There was a problem starting the AP"));
#endif

// set ap hostname
#ifdef ESP32
    if (ret && _hostname != "")
    {
        bool res = WiFi.softAPsetHostname(_hostname.c_str());
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("setting softAP Hostname:"), _hostname);
        if (!res)
            DEBUG_WM(DEBUG_ERROR, F("[ERROR] hostname: AP set failed!"));
        DEBUG_WM(DEBUG_DEV, F("hostname: AP: "), WiFi.softAPgetHostname());
#endif
    }
#endif

    return ret;
}

boolean ThingsCloudWiFiManager::configPortalHasTimeout()
{
    if (!configPortalActive)
        return false;
    uint16_t logintvl = 30000; // how often to emit timeing out counter logging

    // handle timeout portal client check
    if (_configPortalTimeout == 0 || (_apClientCheck && (WiFi_softap_num_stations() > 0)))
    {
        // debug num clients every 30s
        if (millis() - timer > logintvl)
        {
            timer = millis();
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("NUM CLIENTS: "), (String)WiFi_softap_num_stations());
#endif
        }
        _configPortalStart = millis(); // kludge, bump configportal start time to skew timeouts
        return false;
    }

    // handle timeout webclient check
    if (_webClientCheck && (_webPortalAccessed > _configPortalStart) > 0)
        _configPortalStart = _webPortalAccessed;

    // handle timed out
    if (millis() > _configPortalStart + _configPortalTimeout)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("config portal has timed out"));
#endif
        return true; // timeout bail, else do debug logging
    }
    else if (_debug && _debugLevel > 0)
    {
        // log timeout time remaining every 30s
        if ((millis() - timer) > logintvl)
        {
            timer = millis();
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("Portal Timeout In"), (String)((_configPortalStart + _configPortalTimeout - millis()) / 1000) + (String)F(" seconds"));
#endif
        }
    }

    return false;
}

void ThingsCloudWiFiManager::setupDNSD()
{
    dnsServer.reset(new DNSServer());

    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
#ifdef WM_DEBUG_LEVEL
    // DEBUG_WM("dns server started port: ",DNS_PORT);
    DEBUG_WM(DEBUG_DEV, F("dns server started with ip: "), WiFi.softAPIP()); // @todo not showing ip
#endif
    dnsServer->start(DNS_PORT, F("*"), WiFi.softAPIP());
}

void ThingsCloudWiFiManager::setupConfigPortal()
{

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("Starting Provisioning Server"));
#endif

    // setup dns and web servers
    server.reset(new WM_WebServer(_httpPort));

    if (_webservercallback != NULL)
    {
        _webservercallback();
    }
    // @todo add a new callback maybe, after webserver started, callback cannot override handlers, but can grab them first

    /* Setup httpd callbacks, web pages: root, wifi config pages, SO captive portal detectors and not found. */

    server->on(String(FPSTR("/api/wifilist")).c_str(), std::bind(&ThingsCloudWiFiManager::handleWifiList, this));
    server->on(String(FPSTR("/api/wifisave")).c_str(), std::bind(&ThingsCloudWiFiManager::handleWifiSave, this));
    server->on(String(FPSTR("/api/info")).c_str(), std::bind(&ThingsCloudWiFiManager::handleInfo, this));
    server->on(String(FPSTR("/api/reset")).c_str(), std::bind(&ThingsCloudWiFiManager::handleReset, this));
    server->on(String(FPSTR("/api/wifistatus")).c_str(), std::bind(&ThingsCloudWiFiManager::handleWiFiStatus, this));
    server->onNotFound(std::bind(&ThingsCloudWiFiManager::handleNotFound, this));

    server->begin(); // Web server start

    if (_preloadwifiscan)
        WiFi_scanNetworks(true, true); // preload wifiscan , async
}

boolean ThingsCloudWiFiManager::startConfigPortal()
{
    _begin();

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Starting Config Portal"));
#endif

    if (!validApPassword())
        return false;

    // HANDLE issues with STA connections, shutdown sta if not connected, or else this will hang channel scanning and softap will not respond
    // @todo sometimes still cannot connect to AP for no known reason, no events in log either
    if (_disableSTA || (!WiFi.isConnected() && _disableSTAConn))
    {
        // this fixes most ap problems, however, simply doing mode(WIFI_AP) does not work if sta connection is hanging, must `wifi_station_disconnect`
        WiFi_Disconnect();
        WiFi_enableSTA(false);
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Disabling STA"));
#endif
    }
    else
    {
        // @todo even if sta is connected, it is possible that softap connections will fail, IOS says "invalid password", windows says "cannot connect to this network" researching
        WiFi_enableSTA(true);
    }

    // init configportal globals to known states
    configPortalActive = true;
    bool result = connect = abort = false; // loop flags, connect true success, abort true break
    uint8_t state;

    _configPortalStart = millis();

// start access point
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Enabling AP"));
#endif
    startAP();
    WiFiSetCountry();

    // do AP callback if set
    if (_apcallback != NULL)
    {
        _apcallback(this);
    }

// init configportal
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("setupConfigPortal"));
#endif
    setupConfigPortal();

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("setupDNSD"));
#endif
    setupDNSD();

    if (!_configPortalIsBlocking)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Config Portal Running, non blocking/processing"));
        if (_configPortalTimeout > 0)
            DEBUG_WM(DEBUG_VERBOSE, F("Portal Timeout In"), (String)(_configPortalTimeout / 1000) + (String)F(" seconds"));
#endif
        return result; // skip blocking loop
    }

    // enter blocking loop, waiting for config

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Config Portal Running, blocking, waiting for clients..."));
    if (_configPortalTimeout > 0)
        DEBUG_WM(DEBUG_VERBOSE, F("Portal Timeout In"), (String)(_configPortalTimeout / 1000) + (String)F(" seconds"));
#endif

    while (1)
    {

        // if timed out or abort, break
        if (configPortalHasTimeout() || abort)
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_DEV, F("configportal loop abort"));
#endif
            shutdownConfigPortal();
            result = abort ? portalAbortResult : portalTimeoutResult; // false, false
            break;
        }

        state = processConfigPortal();

        // status change, break
        // @todo what is this for, should be moved inside the processor
        if (state != WL_IDLE_STATUS)
        {
            result = (state == WL_CONNECTED); // true if connected
            DEBUG_WM(DEBUG_DEV, F("configportal loop break"));
            break;
        }

        if (!configPortalActive)
            break;

        yield(); // watchdog
    }

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_NOTIFY, F("config portal exiting"));
#endif
    return result;
}

/**
 * [process description]
 * @access public
 * @return bool connected
 */
boolean ThingsCloudWiFiManager::process()
{
// process mdns, esp32 not required
#if defined(WM_MDNS) && defined(ESP8266)
    MDNS.update();
#endif

    if (configPortalActive && !_configPortalIsBlocking)
    {
        if (configPortalHasTimeout())
            shutdownConfigPortal();
    }

    if (webPortalActive || (configPortalActive && !_configPortalIsBlocking))
    {

        // if timed out or abort, break
        if (_allowExit && (configPortalHasTimeout() || abort))
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_DEV, F("process loop abort"));
#endif
            shutdownConfigPortal();
            return false;
        }

        uint8_t state = processConfigPortal();
        return state == WL_CONNECTED;
    }
    return false;
}

// using esp wl_status enums as returns for now, should be fine
uint8_t ThingsCloudWiFiManager::processConfigPortal()
{
    if (configPortalActive)
    {
        // DNS handler
        dnsServer->processNextRequest();
    }

    // HTTP handler
    server->handleClient();

    // Waiting for save...
    if (connect)
    {
        connect = false;
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("processing save"));
#endif
        if (_enableCaptivePortal)
            delay(_cpclosedelay); // keeps the captiveportal from closing to fast.

        // skip wifi if no ssid
        if (_ssid == "")
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("No ssid, skipping wifi save"));
#endif
        }
        else
        {
            // attempt sta connection to submitted _ssid, _pass
            uint8_t res = connectWifi(_ssid, _pass, _connectonsave) == WL_CONNECTED;
            if (res || (!_connectonsave))
            {
#ifdef WM_DEBUG_LEVEL
                if (!_connectonsave)
                {
                    DEBUG_WM(F("SAVED with no connect to new AP"));
                }
                else
                {
                    DEBUG_WM(F("Connect to new AP [SUCCESS]"));
                    DEBUG_WM(F("Got IP Address:"));
                    DEBUG_WM(WiFi.localIP());
                }
#endif

                if (_savewificallback != NULL)
                {
                    _savewificallback();
                }
                if (!_connectonsave)
                    return WL_IDLE_STATUS;
                shutdownConfigPortal();
                return WL_CONNECTED; // CONNECT SUCCESS
            }
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_ERROR, F("[ERROR] Connect to new AP Failed"));
#endif
        }

        if (_shouldBreakAfterConfig)
        {

            // do save callback
            // @todo this is more of an exiting callback than a save, clarify when this should actually occur
            // confirm or verify data was saved to make this more accurate callback
            if (_savewificallback != NULL)
            {
#ifdef WM_DEBUG_LEVEL
                DEBUG_WM(DEBUG_VERBOSE, F("WiFi/Param save callback"));
#endif
                _savewificallback();
            }
            shutdownConfigPortal();
            return WL_CONNECT_FAILED; // CONNECT FAIL
        }
        else if (_configPortalIsBlocking)
        {
            // clear save strings
            _ssid = "";
            _pass = "";
            // if connect fails, turn sta off to stabilize AP
            WiFi_Disconnect();
            WiFi_enableSTA(false);
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("Processing - Disabling STA"));
#endif
        }
        else
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("Portal is non blocking - remaining open"));
#endif
        }
    }

    return WL_IDLE_STATUS;
}

/**
 * [shutdownConfigPortal description]
 * @access public
 * @return bool success (softapdisconnect)
 */
bool ThingsCloudWiFiManager::shutdownConfigPortal()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("shutdownConfigPortal"));
#endif

    if (webPortalActive)
        return false;

    if (configPortalActive)
    {
        // DNS handler
        dnsServer->processNextRequest();
    }

    // HTTP handler
    server->handleClient();

    // @todo what is the proper way to shutdown and free the server up
    server->stop();
    server.reset();

    WiFi.scanDelete(); // free wifi scan results

    if (!configPortalActive)
        return false;

    dnsServer->stop(); //  free heap ?
    dnsServer.reset();

    // turn off AP
    // @todo bug workaround
    // https://github.com/esp8266/Arduino/issues/3793
    // [APdisconnect] set_config failed! *WM: disconnect configportal - softAPdisconnect failed
    // still no way to reproduce reliably

    bool ret = false;
    ret = WiFi.softAPdisconnect(false);

#ifdef WM_DEBUG_LEVEL
    if (!ret)
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] disconnect configportal - softAPdisconnect FAILED"));
    DEBUG_WM(DEBUG_VERBOSE, F("restoring usermode"), getModeString(_usermode));
#endif
    delay(1000);
    WiFi_Mode(_usermode); // restore users wifi mode, BUG https://github.com/esp8266/Arduino/issues/4372
    if (WiFi.status() == WL_IDLE_STATUS)
    {
        WiFi.reconnect(); // restart wifi since we disconnected it in startconfigportal
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("WiFi Reconnect, was idle"));
#endif
    }
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("wifi status:"), getWLStatusString(WiFi.status()));
    DEBUG_WM(DEBUG_VERBOSE, F("wifi mode:"), getModeString(WiFi.getMode()));
#endif
    configPortalActive = false;
    DEBUG_WM(DEBUG_VERBOSE, F("configportal closed"));
    _end();
    return ret;
}

// @todo refactor this up into seperate functions
// one for connecting to flash , one for new client
// clean up, flow is convoluted, and causes bugs
uint8_t ThingsCloudWiFiManager::connectWifi(String ssid, String pass, bool connect)
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("Connecting as wifi client..."));
#endif
    uint8_t retry = 1;
    uint8_t connRes = (uint8_t)WL_NO_SSID_AVAIL;

    setSTAConfig();
    //@todo catch failures in set_config

    // make sure sta is on before `begin` so it does not call enablesta->mode while persistent is ON ( which would save WM AP state to eeprom !)
    // WiFi.setAutoReconnect(false);
    if (_cleanConnect)
        WiFi_Disconnect(); // disconnect before begin, in case anything is hung, this causes a 2 seconds delay for connect
    // @todo find out what status is when this is needed, can we detect it and handle it, say in between states or idle_status

    // if retry without delay (via begin()), the IDF is still busy even after returning status
    // E (5130) wifi:sta is connecting, return error
    // [E][WiFiSTA.cpp:221] begin(): connect failed!

    while (retry <= _connectRetries && (connRes != WL_CONNECTED))
    {
        if (_connectRetries > 1)
        {
            if (_aggresiveReconn)
                delay(1000); // add idle time before recon
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(F("Connect Wifi, ATTEMPT #"), (String)retry + " of " + (String)_connectRetries);
#endif
        }
        // if ssid argument provided connect to that
        if (ssid != "")
        {
            wifiConnectNew(ssid, pass, connect);
            // @todo connect=false seems to disconnect sta in begin() so not sure if _connectonsave is useful at all
            // skip wait if not connecting
            // if(connect){
            if (_saveTimeout > 0)
            {
                connRes = waitForConnectResult(_saveTimeout); // use default save timeout for saves to prevent bugs in esp->waitforconnectresult loop
            }
            else
            {
                connRes = waitForConnectResult(0);
            }
            // }
        }
        else
        {
            // connect using saved ssid if there is one
            if (WiFi_hasAutoConnect())
            {
                wifiConnectDefault();
                connRes = waitForConnectResult();
            }
            else
            {
#ifdef WM_DEBUG_LEVEL
                DEBUG_WM(F("No wifi saved, skipping"));
#endif
            }
        }

#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Connection result:"), getWLStatusString(connRes));
#endif
        retry++;
    }

// WPS enabled? https://github.com/esp8266/Arduino/pull/4889
#ifdef NO_EXTRA_4K_HEAP
    // do WPS, if WPS options enabled and not connected and no password was supplied
    // @todo this seems like wrong place for this, is it a fallback or option?
    if (_tryWPS && connRes != WL_CONNECTED && pass == "")
    {
        startWPS();
        // should be connected at the end of WPS
        connRes = waitForConnectResult();
    }
#endif

    if (connRes != WL_SCAN_COMPLETED)
    {
        updateConxResult(connRes);
    }

    return connRes;
}

/**
 * connect to a new wifi ap
 * @since $dev
 * @param  String ssid
 * @param  String pass
 * @return bool success
 * @return connect only save if false
 */
bool ThingsCloudWiFiManager::wifiConnectNew(String ssid, String pass, bool connect)
{
    bool ret = false;
#ifdef WM_DEBUG_LEVEL
    // DEBUG_WM(DEBUG_DEV,F("CONNECTED: "),WiFi.status() == WL_CONNECTED ? "Y" : "NO");
    DEBUG_WM(F("Connecting to NEW AP:"), ssid);
    DEBUG_WM(DEBUG_DEV, F("Using Password:"), pass);
#endif
    WiFi_enableSTA(true, storeSTAmode); // storeSTAmode will also toggle STA on in default opmode (persistent) if true (default)
    WiFi.persistent(true);
    ret = WiFi.begin(ssid.c_str(), pass.c_str(), 0, NULL, connect);
    WiFi.persistent(false);
#ifdef WM_DEBUG_LEVEL
    if (!ret)
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] wifi begin failed"));
#endif
    return ret;
}

/**
 * connect to stored wifi
 * @since dev
 * @return bool success
 */
bool ThingsCloudWiFiManager::wifiConnectDefault()
{
    bool ret = false;

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("Connecting to SAVED AP:"), WiFi_SSID(true));
    DEBUG_WM(DEBUG_DEV, F("Using Password:"), WiFi_psk(true));
#endif

    ret = WiFi_enableSTA(true, storeSTAmode);
    delay(500); // THIS DELAY ?

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("Mode after delay: "), getModeString(WiFi.getMode()));
    if (!ret)
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] wifi enableSta failed"));
#endif

    ret = WiFi.begin();

#ifdef WM_DEBUG_LEVEL
    if (!ret)
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] wifi begin failed"));
#endif

    return ret;
}

/**
 * set sta config if set
 * @since $dev
 * @return bool success
 */
bool ThingsCloudWiFiManager::setSTAConfig()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("STA static IP:"), _sta_static_ip);
#endif
    bool ret = true;
    if (_sta_static_ip)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Custom static IP/GW/Subnet/DNS"));
#endif
        if (_sta_static_dns)
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("Custom static DNS"));
#endif
            ret = WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn, _sta_static_dns);
        }
        else
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("Custom STA IP/GW/Subnet"));
#endif
            ret = WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn);
        }

#ifdef WM_DEBUG_LEVEL
        if (!ret)
            DEBUG_WM(DEBUG_ERROR, F("[ERROR] wifi config failed"));
        else
            DEBUG_WM(F("STA IP set:"), WiFi.localIP());
#endif
    }
    else
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("setSTAConfig static ip not set, skipping"));
#endif
    }
    return ret;
}

// @todo change to getLastFailureReason and do not touch conxresult
void ThingsCloudWiFiManager::updateConxResult(uint8_t status)
{
    // hack in wrong password detection
    _lastconxresult = status;
#ifdef ESP8266
    if (_lastconxresult == WL_CONNECT_FAILED)
    {
        if (wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
        {
            _lastconxresult = WL_STATION_WRONG_PASSWORD;
        }
    }
#elif defined(ESP32)
    // if(_lastconxresult == WL_CONNECT_FAILED){
    if (_lastconxresult == WL_CONNECT_FAILED || _lastconxresult == WL_DISCONNECTED)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_DEV, F("lastconxresulttmp:"), getWLStatusString(_lastconxresulttmp));
#endif
        if (_lastconxresulttmp != WL_IDLE_STATUS)
        {
            _lastconxresult = _lastconxresulttmp;
            // _lastconxresulttmp = WL_IDLE_STATUS;
        }
    }
    DEBUG_WM(DEBUG_DEV, F("lastconxresult:"), getWLStatusString(_lastconxresult));
#endif
}

uint8_t ThingsCloudWiFiManager::waitForConnectResult()
{
#ifdef WM_DEBUG_LEVEL
    if (_connectTimeout > 0)
        DEBUG_WM(DEBUG_DEV, _connectTimeout, F("ms connectTimeout set"));
#endif
    return waitForConnectResult(_connectTimeout);
}

/**
 * waitForConnectResult
 * @param  uint16_t timeout  in seconds
 * @return uint8_t  WL Status
 */
uint8_t ThingsCloudWiFiManager::waitForConnectResult(uint32_t timeout)
{
    if (timeout == 0)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("connectTimeout not set, ESP waitForConnectResult..."));
#endif
        return WiFi.waitForConnectResult();
    }

    unsigned long timeoutmillis = millis() + timeout;
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, timeout, F("ms timeout, waiting for connect..."));
#endif
    uint8_t status = WiFi.status();

    while (millis() < timeoutmillis)
    {
        status = WiFi.status();
        // @todo detect additional states, connect happens, then dhcp then get ip, there is some delay here, make sure not to timeout if waiting on IP
        if (status == WL_CONNECTED || status == WL_CONNECT_FAILED)
        {
            return status;
        }
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("."));
#endif
        delay(100);
    }
    return status;
}

void ThingsCloudWiFiManager::handleWifiList()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("<- HTTP Wifi list"));
#endif
    String page;

    WiFi_scanNetworks(server->hasArg(F("refresh")), false); // wifiscan, force if arg refresh
    page += getScanItems();

    server->send(200, FPSTR(HTTP_HEAD_JSON), page);
    // server->close(); // testing reliability fix for content length mismatches during mutiple flood hits

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("Sent config page"));
#endif
}

// // is it possible in softap mode to detect aps without scanning
// bool ThingsCloudWiFiManager::WiFi_scanNetworksForAP(bool force){
//   WiFi_scanNetworks(force);
// }

void ThingsCloudWiFiManager::WiFi_scanComplete(int networksFound)
{
    _lastscan = millis();
    _numNetworks = networksFound;
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("WiFi Scan ASYNC completed"), "in " + (String)(_lastscan - _startscan) + " ms");
    DEBUG_WM(DEBUG_VERBOSE, F("WiFi Scan ASYNC found:"), _numNetworks);
#endif
}

bool ThingsCloudWiFiManager::WiFi_scanNetworks()
{
    return WiFi_scanNetworks(false, false);
}

bool ThingsCloudWiFiManager::WiFi_scanNetworks(unsigned int cachetime, bool async)
{
    return WiFi_scanNetworks(millis() - _lastscan > cachetime, async);
}
bool ThingsCloudWiFiManager::WiFi_scanNetworks(unsigned int cachetime)
{
    return WiFi_scanNetworks(millis() - _lastscan > cachetime, false);
}
bool ThingsCloudWiFiManager::WiFi_scanNetworks(bool force, bool async)
{
#ifdef WM_DEBUG_LEVEL
// DEBUG_WM(DEBUG_DEV,"scanNetworks async:",async == true);
// DEBUG_WM(DEBUG_DEV,_numNetworks,(millis()-_lastscan ));
// DEBUG_WM(DEBUG_DEV,"scanNetworks force:",force == true);
#endif
    if (_numNetworks == 0)
    {
        DEBUG_WM(DEBUG_DEV, "NO APs found forcing new scan");
        force = true;
    }
    if (force || (millis() - _lastscan > 60000))
    {
        int8_t res;
        _startscan = millis();
        if (async && _asyncScan)
        {
#ifdef ESP8266
#ifndef WM_NOASYNC // no async available < 2.4.0
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("WiFi Scan ASYNC started"));
#endif
            using namespace std::placeholders; // for `_1`
            WiFi.scanNetworksAsync(std::bind(&ThingsCloudWiFiManager::WiFi_scanComplete, this, _1));
#else
            DEBUG_WM(DEBUG_VERBOSE, F("WiFi Scan SYNC started"));
            res = WiFi.scanNetworks();
#endif
#else
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("WiFi Scan ASYNC started"));
#endif
            res = WiFi.scanNetworks(true);
#endif
            return false;
        }
        else
        {
            DEBUG_WM(DEBUG_VERBOSE, F("WiFi Scan SYNC started"));
            res = WiFi.scanNetworks();
        }
        if (res == WIFI_SCAN_FAILED)
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_ERROR, F("[ERROR] scan failed"));
#endif
        }
        else if (res == WIFI_SCAN_RUNNING)
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_ERROR, F("[ERROR] scan waiting"));
#endif
            while (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
            {
#ifdef WM_DEBUG_LEVEL
                DEBUG_WM(DEBUG_ERROR, ".");
#endif
                delay(100);
            }
            _numNetworks = WiFi.scanComplete();
        }
        else if (res >= 0)
            _numNetworks = res;
        _lastscan = millis();
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("WiFi Scan completed"), "in " + (String)(_lastscan - _startscan) + " ms");
#endif
        return true;
    }
    else
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Scan is cached"), (String)(millis() - _lastscan) + " ms ago");
#endif
    }
    return false;
}

String ThingsCloudWiFiManager::ThingsCloudWiFiManager::getScanItems()
{
    String json;
    StaticJsonDocument<1024> jsonObject;
    jsonObject["board"] = "esp32";
    jsonObject["time"] = 1351824120;
    JsonArray wifiArray = jsonObject.createNestedArray("wifi");

    if (!_numNetworks)
        WiFi_scanNetworks(); // scan in case this gets called before any scans

    int n = _numNetworks;
    if (n == 0)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("No networks found"));
#endif
        json = FPSTR("{\"result\":false}");
    }
    else
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(n, F("networks found"));
#endif
        // sort networks
        int indices[n];
        for (int i = 0; i < n; i++)
        {
            indices[i] = i;
        }

        // RSSI SORT
        for (int i = 0; i < n; i++)
        {
            for (int j = i + 1; j < n; j++)
            {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
                {
                    std::swap(indices[i], indices[j]);
                }
            }
        }

        // remove duplicates ( must be RSSI sorted )
        if (_removeDuplicateAPs)
        {
            String cssid;
            for (int i = 0; i < n; i++)
            {
                if (indices[i] == -1)
                    continue;
                cssid = WiFi.SSID(indices[i]);
                for (int j = i + 1; j < n; j++)
                {
                    if (cssid == WiFi.SSID(indices[j]))
                    {
#ifdef WM_DEBUG_LEVEL
                        DEBUG_WM(DEBUG_VERBOSE, F("DUP AP:"), WiFi.SSID(indices[j]));
#endif
                        indices[j] = -1; // set dup aps to index -1
                    }
                }
            }
        }

        // display networks in page
        for (int i = 0; i < n; i++)
        {
            if (indices[i] == -1)
                continue; // skip dups

#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("AP: "), (String)WiFi.RSSI(indices[i]) + " " + (String)WiFi.SSID(indices[i]));
#endif

            int rssiperc = getRSSIasQuality(WiFi.RSSI(indices[i]));
            uint8_t enc_type = WiFi.encryptionType(indices[i]);

            if (_minimumQuality == -1 || _minimumQuality < rssiperc)
            {
                if (WiFi.SSID(indices[i]) == "")
                {
                    // Serial.println(WiFi.BSSIDstr(indices[i]));
                    continue; // No idea why I am seeing these, lets just skip them for now
                }
                StaticJsonDocument<200> doc;
                doc["v"] = htmlEntities(WiFi.SSID(indices[i]));
                doc["e"] = encryptionTypeStr(enc_type);
                doc["r"] = (String)rssiperc;
                doc["R"] = (String)WiFi.RSSI(indices[i]);
                doc["q"] = (String) int(round(map(rssiperc, 0, 100, 1, 4)));
                if (enc_type != WM_WIFIOPEN)
                {
                    doc["q"] = F("l");
                }
                else
                {
                    doc["q"] = F("l");
                }
                wifiArray.add(doc);
                delay(0);
            }
            else
            {
#ifdef WM_DEBUG_LEVEL
                DEBUG_WM(DEBUG_VERBOSE, F("Skipping , does not meet _minimumQuality"));
#endif
            }
        }

        serializeJson(jsonObject, json);
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, json.c_str());
#endif
    }

    return json;
}

void ThingsCloudWiFiManager::handleWiFiStatus()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("<- HTTP WiFi status "));
#endif
    String page = "{\"result\":true,\"count\":1}";
    server->send(200, FPSTR(HTTP_HEAD_JSON), page);
}

void ThingsCloudWiFiManager::handleWifiSave()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("<- HTTP WiFi save "));
#endif
    String page;

    // SAVE/connect here
    _ssid = server->arg(F("ssid")).c_str();
    _pass = server->arg(F("password")).c_str();
    _customerId = server->arg(F("customer_id")).c_str();

    if (_customerId == "")
    {
        page = "{\"result\":false}";
        server->sendHeader(FPSTR(HTTP_HEAD_CORS), FPSTR(HTTP_HEAD_CORS_ALLOW_ALL));
        server->send(200, FPSTR(HTTP_HEAD_JSON), page);
        return;
    }
    if (this->_mqttClient)
    {
        this->_mqttClient->setCustomerId(this->getCustomerId());
    }

    if (server->arg(FPSTR(S_ip)) != "")
    {
        //_sta_static_ip.fromString(server->arg(FPSTR(S_ip));
        String ip = server->arg(FPSTR(S_ip));
        optionalIPFromString(&_sta_static_ip, ip.c_str());
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_DEV, F("static ip:"), ip);
#endif
    }
    if (server->arg(FPSTR(S_gw)) != "")
    {
        String gw = server->arg(FPSTR(S_gw));
        optionalIPFromString(&_sta_static_gw, gw.c_str());
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_DEV, F("static gateway:"), gw);
#endif
    }
    if (server->arg(FPSTR(S_sn)) != "")
    {
        String sn = server->arg(FPSTR(S_sn));
        optionalIPFromString(&_sta_static_sn, sn.c_str());
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_DEV, F("static netmask:"), sn);
#endif
    }
    if (server->arg(FPSTR(S_dns)) != "")
    {
        String dns = server->arg(FPSTR(S_dns));
        optionalIPFromString(&_sta_static_dns, dns.c_str());
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_DEV, F("static DNS:"), dns);
#endif
    }

    page = "{\"result\":true}";
    server->sendHeader(FPSTR(HTTP_HEAD_CORS), FPSTR(HTTP_HEAD_CORS_ALLOW_ALL));
    server->send(200, FPSTR(HTTP_HEAD_JSON), page);

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("Sent wifi save page"));
#endif

    connect = true; // signal ready to connect/reset process in processConfigPortal
}

/**
 * HTTPD CALLBACK info page
 */
void ThingsCloudWiFiManager::handleInfo()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("<- HTTP Info"));
#endif

    StaticJsonDocument<1024> infoObj;
    if (WiFi_SSID() != "")
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            infoObj["ip"] = WiFi.localIP().toString();
            infoObj["ssid"] = htmlEntities(WiFi_SSID());
        }
        else
        {
            infoObj["ssid"] = htmlEntities(WiFi_SSID());
            if (_lastconxresult == WL_STATION_WRONG_PASSWORD)
            {
                // wrong password
                infoObj["last_err"] = FPSTR(HTTP_STATUS_OFFPW);
            }
            else if (_lastconxresult == WL_NO_SSID_AVAIL)
            {
                // connect failed, or ap not found
                infoObj["last_err"] = FPSTR(HTTP_STATUS_OFFNOAP);
            }
            else if (_lastconxresult == WL_CONNECT_FAILED)
            {
                // connect failed
                infoObj["last_err"] = FPSTR(HTTP_STATUS_OFFFAIL);
            }
        }
    }
    else
    {
    }

    infoObj["device_key"] = _deviceKey;

    infoObj["esphead"] = FPSTR(HTTP_INFO_esphead);

    infoObj["wifihead"] = FPSTR(HTTP_INFO_wifihead);

    infoObj["uptime_mins"] = (String)(millis() / 1000 / 60);
    infoObj["uptime_secs"] = (String)((millis() / 1000) % 60);

    infoObj["chip_id"] = getEspChipUniqueId();

#ifdef ESP32

    String rev = (String)ESP.getChipRevision();
#ifdef _SOC_EFUSE_REG_H_
    String revb = (String)(REG_READ(EFUSE_BLK0_RDATA3_REG) >> (EFUSE_RD_CHIP_VER_RESERVE_S) && EFUSE_RD_CHIP_VER_RESERVE_V);
    infoObj["chip_rev"] = rev + "-" + revb;
#else
    infoObj["chip_rev"] = rev;
#endif
#endif
#ifdef ESP8266
    infoObj["fchip_id"] = (String)ESP.getFlashChipId();

#endif
    infoObj["fchip_size"] = (String)ESP.getFlashChipSize();
#ifdef ESP8266
    infoObj["flash_real_size"] = (String)ESP.getFlashChipRealSize();
#elif defined ESP32
    infoObj["psram_size"] = (String)ESP.getPsramSize();
#endif

#ifdef ESP32
    infoObj["idf_ver"] = (String)esp_get_idf_version();
#else
    infoObj["idf_ver"] = (String)system_get_sdk_version();
#endif

#ifdef ESP8266
    infoObj["core_ver"] = (String)ESP.getCoreVersion();
#endif

#ifdef ESP8266
    infoObj["boot_ver"] = (String)system_get_boot_version();

#endif

    infoObj["cpu_freq"] = (String)ESP.getCpuFreqMHz();
    infoObj["free_heap"] = (String)ESP.getFreeHeap();
    infoObj["sketch_size_used"] = (String)(ESP.getSketchSize());
    infoObj["sketch_size_total"] = (String)(ESP.getSketchSize() + ESP.getFreeSketchSpace());

    infoObj["soft_ap_ip"] = WiFi.softAPIP().toString();
    infoObj["soft_ap_mac"] = (String)WiFi.softAPmacAddress();

#ifdef ESP32

    infoObj["soft_ap_hostname"] = WiFi.softAPgetHostname();

#endif
#ifdef ESP8266

    p = FPSTR(HTTP_INFO_apssid);
    p.replace(FPSTR(T_1), htmlEntities(WiFi.softAPSSID()));
    infoObj["soft_ap_ssid"] = htmlEntities(WiFi.softAPSSID());

#endif

    infoObj["bssid"] = (String)WiFi.BSSIDstr();
    infoObj["sta_ssid"] = htmlEntities((String)WiFi_SSID());
    infoObj["local_ip"] = WiFi.localIP().toString();
    infoObj["gateway_ip"] = WiFi.gatewayIP().toString();
    infoObj["subnet_mask"] = WiFi.subnetMask().toString();
    infoObj["dns_ip"] = WiFi.dnsIP().toString();

#ifdef ESP32
    infoObj["hostname"] = WiFi.getHostname();
#else
    infoObj["hostname"] = WiFi.hostname();
#endif

    infoObj["mac_address"] = WiFi.macAddress();

    infoObj["wifi_connected"] = WiFi.isConnected() ? true : false;

#ifdef ESP8266

    infoObj["auto_connect_enable"] = WiFi.getAutoConnect() ? true : false;

#endif
#ifdef ESP32

    // temperature is not calibrated, varying large offsets are present, use for relative temp changes only
    infoObj["temperature_c"] = (String)temperatureRead();
    infoObj["temperature_f"] = (String)((temperatureRead() + 32) * 1.8);

#endif

    String json;
    serializeJson(infoObj, json);
    server->send(200, FPSTR(HTTP_HEAD_JSON), json);
}

/**
 * HTTPD CALLBACK reset page
 */
void ThingsCloudWiFiManager::handleReset()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("<- HTTP Reset"));
#endif
    String json = FPSTR("{\"result\":true}");
    server->send(200, FPSTR(HTTP_HEAD_JSON), json);

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("RESETTING ESP"));
#endif
    delay(1000);
    reboot();
}

/**
 * HTTPD CALLBACK 404
 */
void ThingsCloudWiFiManager::handleNotFound()
{
    String json = FPSTR("{\"result\":false}");
    server->sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
    server->sendHeader(F("Pragma"), F("no-cache"));
    server->sendHeader(F("Expires"), F("-1"));
    server->send(404, FPSTR(HTTP_HEAD_JSON), json);
}

void ThingsCloudWiFiManager::stopCaptivePortal()
{
    _enableCaptivePortal = false;
    // @todo maybe disable configportaltimeout(optional), or just provide callback for user
}

// PUBLIC

// METHODS

/**
 * reset wifi settings, clean stored ap password
 */

/**
 * [stopConfigPortal description]
 * @return {[type]} [description]
 */
bool ThingsCloudWiFiManager::stopConfigPortal()
{
    if (_configPortalIsBlocking)
    {
        abort = true;
        return true;
    }
    return shutdownConfigPortal();
}

/**
 * disconnect
 * @access public
 * @since $dev
 * @return bool success
 */
bool ThingsCloudWiFiManager::disconnect()
{
    if (WiFi.status() != WL_CONNECTED)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("Disconnecting: Not connected"));
#endif
        return false;
    }
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("Disconnecting"));
#endif
    return WiFi_Disconnect();
}

/**
 * reboot the device
 * @access public
 */
void ThingsCloudWiFiManager::reboot()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("Restarting"));
#endif
    ESP.restart();
}

/**
 * reboot the device
 * @access public
 */
bool ThingsCloudWiFiManager::erase()
{
    return erase(false);
}

bool ThingsCloudWiFiManager::erase(bool opt)
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM("Erasing");
#endif

#if defined(ESP32) && ((defined(WM_ERASE_NVS) || defined(nvs_flash_h)))
    // if opt true, do nvs erase
    if (opt)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("Erasing NVS"));
#endif
        esp_err_t err;
        err = nvs_flash_init();
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("nvs_flash_init: "), err != ESP_OK ? (String)err : "Success");
#endif
        err = nvs_flash_erase();
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("nvs_flash_erase: "), err != ESP_OK ? (String)err : "Success");
#endif
        return err == ESP_OK;
    }
#elif defined(ESP8266) && defined(spiffs_api_h)
    if (opt)
    {
        bool ret = false;
        if (SPIFFS.begin())
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(F("Erasing SPIFFS"));
#endif
            bool ret = SPIFFS.format();
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(DEBUG_VERBOSE, F("spiffs erase: "), ret ? "Success" : "ERROR");
#endif
        }
        else
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(F("[ERROR] Could not start SPIFFS"));
#endif
        }
        return ret;
    }
#else
    (void)opt;
#endif

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("Erasing WiFi Config"));
#endif
    return WiFi_eraseConfig();
}

/**
 * [resetSettings description]
 * ERASES STA CREDENTIALS
 * @access public
 */
void ThingsCloudWiFiManager::resetSettings()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("resetSettings"));
#endif
    WiFi_enableSTA(true, true); // must be sta to disconnect erase

    if (_resetcallback != NULL)
        _resetcallback();

#ifdef ESP32
    WiFi.disconnect(true, true);
#else
    WiFi.persistent(true);
    WiFi.disconnect(true);
    WiFi.persistent(false);
#endif
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("SETTINGS ERASED"));
#endif
}

// SETTERS

/**
 * [setTimeout description]
 * @access public
 * @param {[type]} unsigned long seconds [description]
 */
void ThingsCloudWiFiManager::setTimeout(unsigned long seconds)
{
    setConfigPortalTimeout(seconds);
}

/**
 * [setConfigPortalTimeout description]
 * @access public
 * @param {[type]} unsigned long seconds [description]
 */
void ThingsCloudWiFiManager::setConfigPortalTimeout(unsigned long seconds)
{
    _configPortalTimeout = seconds * 1000;
}

/**
 * [setConnectTimeout description]
 * @access public
 * @param {[type]} unsigned long seconds [description]
 */
void ThingsCloudWiFiManager::setConnectTimeout(unsigned long seconds)
{
    _connectTimeout = seconds * 1000;
}

/**
 * [setConnectRetries description]
 * @access public
 * @param {[type]} uint8_t numRetries [description]
 */
void ThingsCloudWiFiManager::setConnectRetries(uint8_t numRetries)
{
    _connectRetries = constrain(numRetries, 1, 10);
}

/**
 * toggle _cleanconnect, always disconnect before connecting
 * @param {[type]} bool enable [description]
 */
void ThingsCloudWiFiManager::setCleanConnect(bool enable)
{
    _cleanConnect = enable;
}

/**
 * [setConnectTimeout description
 * @access public
 * @param {[type]} unsigned long seconds [description]
 */
void ThingsCloudWiFiManager::setSaveConnectTimeout(unsigned long seconds)
{
    _saveTimeout = seconds * 1000;
}

/**
 * Set save portal connect on save option,
 * if false, will only save credentials not connect
 * @access public
 * @param {[type]} bool connect [description]
 */
void ThingsCloudWiFiManager::setSaveConnect(bool connect)
{
    _connectonsave = connect;
}

/**
 * [setDebugOutput description]
 * @access public
 * @param {[type]} boolean debug [description]
 */
void ThingsCloudWiFiManager::setDebugOutput(boolean debug)
{
    _debug = debug;
    if (_debug && _debugLevel == DEBUG_DEV)
        debugPlatformInfo();
}

void ThingsCloudWiFiManager::setDebugOutput(boolean debug, String prefix)
{
    _debugPrefix = prefix;
    setDebugOutput(debug);
}

/**
 * [setAPStaticIPConfig description]
 * @access public
 * @param {[type]} IPAddress ip [description]
 * @param {[type]} IPAddress gw [description]
 * @param {[type]} IPAddress sn [description]
 */
void ThingsCloudWiFiManager::setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn)
{
    _ap_static_ip = ip;
    _ap_static_gw = gw;
    _ap_static_sn = sn;
}

/**
 * [setSTAStaticIPConfig description]
 * @access public
 * @param {[type]} IPAddress ip [description]
 * @param {[type]} IPAddress gw [description]
 * @param {[type]} IPAddress sn [description]
 */
void ThingsCloudWiFiManager::setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn)
{
    _sta_static_ip = ip;
    _sta_static_gw = gw;
    _sta_static_sn = sn;
}

/**
 * [setSTAStaticIPConfig description]
 * @since $dev
 * @access public
 * @param {[type]} IPAddress ip [description]
 * @param {[type]} IPAddress gw [description]
 * @param {[type]} IPAddress sn [description]
 * @param {[type]} IPAddress dns [description]
 */
void ThingsCloudWiFiManager::setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn, IPAddress dns)
{
    setSTAStaticIPConfig(ip, gw, sn);
    _sta_static_dns = dns;
}

/**
 * [setMinimumSignalQuality description]
 * @access public
 * @param {[type]} int quality [description]
 */
void ThingsCloudWiFiManager::setMinimumSignalQuality(int quality)
{
    _minimumQuality = quality;
}

/**
 * [setBreakAfterConfig description]
 * @access public
 * @param {[type]} boolean shouldBreak [description]
 */
void ThingsCloudWiFiManager::setBreakAfterConfig(boolean shouldBreak)
{
    _shouldBreakAfterConfig = shouldBreak;
}

void ThingsCloudWiFiManager::linkMQTTClient(ThingsCloudMQTT *client)
{
    _mqttClient = client;
    this->setDeviceKey(_mqttClient->getDeviceKey());
}

void ThingsCloudWiFiManager::setDeviceKey(String deviceKey)
{
    _deviceKey = deviceKey;
}

String ThingsCloudWiFiManager::getCustomerId()
{
    return _customerId;
}

/**
 * setAPCallback, set a callback when softap is started
 * @access public
 * @param {[type]} void (*func)(ThingsCloudWiFiManager* wminstance)
 */
void ThingsCloudWiFiManager::setAPCallback(std::function<void(ThingsCloudWiFiManager *)> func)
{
    _apcallback = func;
}

/**
 * setWebServerCallback, set a callback after webserver is reset, and before routes are setup
 * if we set webserver handlers before wm, they are used and wm is not by esp webserver
 * on events cannot be overrided once set, and are not mutiples
 * @access public
 * @param {[type]} void (*func)(void)
 */
void ThingsCloudWiFiManager::setWebServerCallback(std::function<void()> func)
{
    _webservercallback = func;
}

/**
 * setSaveConfigCallback, set a save config callback after closing configportal
 * @note calls only if wifi is saved or changed, or setBreakAfterConfig(true)
 * @access public
 * @param {[type]} void (*func)(void)
 */
void ThingsCloudWiFiManager::setSaveConfigCallback(std::function<void()> func)
{
    _savewificallback = func;
}

/**
 * setConfigResetCallback, set a callback to occur when a resetSettings() occurs
 * @access public
 * @param {[type]} void(*func)(void)
 */
void ThingsCloudWiFiManager::setConfigResetCallback(std::function<void()> func)
{
    _resetcallback = func;
}

/**
 * setPreSaveConfigCallback, set a callback to fire before saving wifi or params
 * @access public
 * @param {[type]} void (*func)(void)
 */
void ThingsCloudWiFiManager::setPreSaveConfigCallback(std::function<void()> func)
{
    _presavecallback = func;
}

/**
 * setPreOtaUpdateCallback, set a callback to fire before OTA update
 * @access public
 * @param {[type]} void (*func)(void)
 */
void ThingsCloudWiFiManager::setPreOtaUpdateCallback(std::function<void()> func)
{
    _preotaupdatecallback = func;
}

/**
 * toggle wifiscan hiding of duplicate ssid names
 * if this is false, wifiscan will remove duplicat Access Points - defaut true
 * @access public
 * @param boolean removeDuplicates [true]
 */
void ThingsCloudWiFiManager::setRemoveDuplicateAPs(boolean removeDuplicates)
{
    _removeDuplicateAPs = removeDuplicates;
}

/**
 * toggle configportal blocking loop
 * if enabled, then the configportal will enter a blocking loop and wait for configuration
 * if disabled use with process() to manually process webserver
 * @since $dev
 * @access public
 * @param boolean shoudlBlock [false]
 */
void ThingsCloudWiFiManager::setConfigPortalBlocking(boolean shoudlBlock)
{
    _configPortalIsBlocking = shoudlBlock;
}

/**
 * toggle restore persistent, track internally
 * sets ESP wifi.persistent so we can remember it and restore user preference on destruct
 * there is no getter in esp8266 platform prior to https://github.com/esp8266/Arduino/pull/3857
 * @since $dev
 * @access public
 * @param boolean persistent [true]
 */
void ThingsCloudWiFiManager::setRestorePersistent(boolean persistent)
{
    _userpersistent = persistent;
    if (!persistent)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(F("persistent is off"));
#endif
    }
}

/**
 * toggle showing static ip form fields
 * if enabled, then the static ip, gateway, subnet fields will be visible, even if not set in code
 * @since $dev
 * @access public
 * @param boolean alwaysShow [false]
 */
void ThingsCloudWiFiManager::setShowStaticFields(boolean alwaysShow)
{
    if (_disableIpFields)
        _staShowStaticFields = alwaysShow ? 1 : -1;
    else
        _staShowStaticFields = alwaysShow ? 1 : 0;
}

/**
 * toggle showing dns fields
 * if enabled, then the dns1 field will be visible, even if not set in code
 * @since $dev
 * @access public
 * @param boolean alwaysShow [false]
 */
void ThingsCloudWiFiManager::setShowDnsFields(boolean alwaysShow)
{
    if (_disableIpFields)
        _staShowDns = alwaysShow ? 1 : -1;
    _staShowDns = alwaysShow ? 1 : 0;
}

/**
 * toggle captive portal
 * if enabled, then devices that use captive portal checks will be redirected to root
 * if not you will automatically have to navigate to ip [192.168.4.1]
 * @since $dev
 * @access public
 * @param boolean enabled [true]
 */
void ThingsCloudWiFiManager::setCaptivePortalEnable(boolean enabled)
{
    _enableCaptivePortal = enabled;
}

/**
 * toggle wifi autoreconnect policy
 * if enabled, then wifi will autoreconnect automatically always
 * On esp8266 we force this on when autoconnect is called, see notes
 * On esp32 this is handled on SYSTEM_EVENT_STA_DISCONNECTED since it does not exist in core yet
 * @since $dev
 * @access public
 * @param boolean enabled [true]
 */
void ThingsCloudWiFiManager::setWiFiAutoReconnect(boolean enabled)
{
    _wifiAutoReconnect = enabled;
}

/**
 * toggle configportal timeout wait for station client
 * if enabled, then the configportal will start timeout when no stations are connected to softAP
 * disabled by default as rogue stations can keep it open if there is no auth
 * @since $dev
 * @access public
 * @param boolean enabled [false]
 */
void ThingsCloudWiFiManager::setAPClientCheck(boolean enabled)
{
    _apClientCheck = enabled;
}

/**
 * toggle configportal timeout wait for web client
 * if enabled, then the configportal will restart timeout when client requests come in
 * @since $dev
 * @access public
 * @param boolean enabled [true]
 */
void ThingsCloudWiFiManager::setWebPortalClientCheck(boolean enabled)
{
    _webClientCheck = enabled;
}

/**
 * toggle wifiscan percentages or quality icons
 * @since $dev
 * @access public
 * @param boolean enabled [false]
 */
void ThingsCloudWiFiManager::setScanDispPerc(boolean enabled)
{
    _scanDispOptions = enabled;
}

/**
 * toggle configportal if autoconnect failed
 * if enabled, then the configportal will be activated on autoconnect failure
 * @since $dev
 * @access public
 * @param boolean enabled [true]
 */
void ThingsCloudWiFiManager::setEnableConfigPortal(boolean enable)
{
    _enableConfigPortal = enable;
}

/**
 * set the hostname (dhcp client id)
 * @since $dev
 * @access public
 * @param  char* hostname 32 character hostname to use for sta+ap in esp32, sta in esp8266
 * @return bool false if hostname is not valid
 */
bool ThingsCloudWiFiManager::setHostname(const char *hostname)
{
    //@todo max length 32
    _hostname = String(hostname);
    return true;
}

bool ThingsCloudWiFiManager::setHostname(String hostname)
{
    //@todo max length 32
    _hostname = hostname;
    return true;
}

/**
 * set the soft ao channel, ignored if channelsync is true and connected
 * @param int32_t   wifi channel, 0 to disable
 */
void ThingsCloudWiFiManager::setWiFiAPChannel(int32_t channel)
{
    _apChannel = channel;
}

/**
 * set the soft ap hidden
 * @param bool   wifi ap hidden, default is false
 */
void ThingsCloudWiFiManager::setWiFiAPHidden(bool hidden)
{
    _apHidden = hidden;
}

/**
 * toggle showing erase wifi config button on info page
 * @param boolean enabled
 */
void ThingsCloudWiFiManager::setShowInfoErase(boolean enabled)
{
    _showInfoErase = enabled;
}

/**
 * toggle showing update upload web ota button on info page
 * @param boolean enabled
 */
void ThingsCloudWiFiManager::setShowInfoUpdate(boolean enabled)
{
    _showInfoUpdate = enabled;
}

/**
 * check if the config portal is running
 * @return bool true if active
 */
bool ThingsCloudWiFiManager::getConfigPortalActive()
{
    return configPortalActive;
}

/**
 * [getConfigPortalActive description]
 * @return bool true if active
 */
bool ThingsCloudWiFiManager::getWebPortalActive()
{
    return webPortalActive;
}

String ThingsCloudWiFiManager::getWiFiHostname()
{
#ifdef ESP32
    return (String)WiFi.getHostname();
#else
    return (String)WiFi.hostname();
#endif
}

/**
 * [setTitle description]
 * @param String title, set app title
 */
void ThingsCloudWiFiManager::setTitle(String title)
{
    _title = title;
}

// GETTERS

/**
 * get config portal AP SSID
 * @since 0.0.1
 * @access public
 * @return String the configportal ap name
 */
String ThingsCloudWiFiManager::getConfigPortalSSID()
{
    return _apName;
}

/**
 * return the last known connection result
 * logged on autoconnect and wifisave, can be used to check why failed
 * get as readable string with getWLStatusString(getLastConxResult);
 * @since $dev
 * @access public
 * @return bool return wl_status codes
 */
uint8_t ThingsCloudWiFiManager::getLastConxResult()
{
    return _lastconxresult;
}

/**
 * check if wifi has a saved ap or not
 * @since $dev
 * @access public
 * @return bool true if a saved ap config exists
 */
bool ThingsCloudWiFiManager::getWiFiIsSaved()
{
    return WiFi_hasAutoConnect();
}

/**
 * getDefaultAPName
 * @since $dev
 * @return string
 */
String ThingsCloudWiFiManager::getDefaultAPName()
{
    return _wifissidprefix + "_" + getEspChipUniqueId();
}

/**
 * getEspChipUniqueId
 * @since $dev
 * @return string
 */
String ThingsCloudWiFiManager::getEspChipUniqueId()
{
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8)
    {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    String uniqueId = String(chipId, HEX);
    uniqueId.toUpperCase();
    return uniqueId;
}

/**
 * setCountry
 * @since $dev
 * @param String cc country code, must be defined in WiFiSetCountry, US, JP, CN
 */
void ThingsCloudWiFiManager::setCountry(String cc)
{
    _wificountry = cc;
}

/**
 * setClass
 * @param String str body class string
 */
void ThingsCloudWiFiManager::setClass(String str)
{
    _bodyClass = str;
}

/**
 * setDarkMode
 * @param bool enable, enable dark mode via invert class
 */
void ThingsCloudWiFiManager::setDarkMode(bool enable)
{
    _bodyClass = enable ? "invert" : "";
}

/**
 * setHttpPort
 * @param uint16_t port webserver port number default 80
 */
void ThingsCloudWiFiManager::setHttpPort(uint16_t port)
{
    _httpPort = port;
}

bool ThingsCloudWiFiManager::preloadWiFi(String ssid, String pass)
{
    _defaultssid = ssid;
    _defaultpass = pass;
    return true;
}

// HELPERS

/**
 * getWiFiSSID
 * @since $dev
 * @param bool persistent
 * @return String
 */
String ThingsCloudWiFiManager::getWiFiSSID(bool persistent)
{
    return WiFi_SSID(persistent);
}

/**
 * getWiFiPass
 * @since $dev
 * @param bool persistent
 * @return String
 */
String ThingsCloudWiFiManager::getWiFiPass(bool persistent)
{
    return WiFi_psk(persistent);
}

// DEBUG
// @todo fix DEBUG_WM(0,0);
template <typename Generic>
void ThingsCloudWiFiManager::DEBUG_WM(Generic text)
{
    DEBUG_WM(DEBUG_NOTIFY, text, "");
}

template <typename Generic>
void ThingsCloudWiFiManager::DEBUG_WM(wm_debuglevel_t level, Generic text)
{
    if (_debugLevel >= level)
        DEBUG_WM(level, text, "");
}

template <typename Generic, typename Genericb>
void ThingsCloudWiFiManager::DEBUG_WM(Generic text, Genericb textb)
{
    DEBUG_WM(DEBUG_NOTIFY, text, textb);
}

template <typename Generic, typename Genericb>
void ThingsCloudWiFiManager::DEBUG_WM(wm_debuglevel_t level, Generic text, Genericb textb)
{
    if (!_debug || _debugLevel < level)
        return;

    if (_debugLevel >= DEBUG_MAX)
    {
#ifdef ESP8266
// uint32_t free;
// uint16_t max;
// uint8_t frag;
// ESP.getHeapStats(&free, &max, &frag);// @todo Does not exist in 2.3.0
// _debugPort.printf("[MEM] free: %5d | max: %5d | frag: %3d%% \n", free, max, frag);
#elif defined ESP32
        // total_free_bytes;      ///<  Total free bytes in the heap. Equivalent to multi_free_heap_size().
        // total_allocated_bytes; ///<  Total bytes allocated to data in the heap.
        // largest_free_block;    ///<  Size of largest free block in the heap. This is the largest malloc-able size.
        // minimum_free_bytes;    ///<  Lifetime minimum free heap size. Equivalent to multi_minimum_free_heap_size().
        // allocated_blocks;      ///<  Number of (variable size) blocks allocated in the heap.
        // free_blocks;           ///<  Number of (variable size) free blocks in the heap.
        // total_blocks;          ///<  Total number of (variable size) blocks in the heap.
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
        uint32_t free = info.total_free_bytes;
        uint16_t max = info.largest_free_block;
        uint8_t frag = 100 - (max * 100) / free;
        _debugPort.printf("[MEM] free: %5d | max: %5d | frag: %3d%% \n", free, max, frag);
#endif
    }
    _debugPort.print(_debugPrefix);
    if (_debugLevel >= debugLvlShow)
        _debugPort.print("[" + (String)level + "] ");
    _debugPort.print(text);
    if (textb)
    {
        _debugPort.print(" ");
        _debugPort.print(textb);
    }
    _debugPort.println();
}

/**
 * [debugSoftAPConfig description]
 * @access public
 * @return {[type]} [description]
 */
void ThingsCloudWiFiManager::debugSoftAPConfig()
{

#ifdef ESP8266
    softap_config config;
    wifi_softap_get_config(&config);
#if !defined(WM_NOCOUNTRY)
    wifi_country_t country;
    wifi_get_country(&country);
#endif
#elif defined(ESP32)
    wifi_country_t country;
    wifi_config_t conf_config;
    esp_wifi_get_config(WIFI_IF_AP, &conf_config); // == ESP_OK
    wifi_ap_config_t config = conf_config.ap;
    esp_wifi_get_country(&country);
#endif

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("SoftAP Configuration"));
    DEBUG_WM(FPSTR(D_HR));
    DEBUG_WM(F("ssid:            "), (char *)config.ssid);
    DEBUG_WM(F("password:        "), (char *)config.password);
    DEBUG_WM(F("ssid_len:        "), config.ssid_len);
    DEBUG_WM(F("channel:         "), config.channel);
    DEBUG_WM(F("authmode:        "), config.authmode);
    DEBUG_WM(F("ssid_hidden:     "), config.ssid_hidden);
    DEBUG_WM(F("max_connection:  "), config.max_connection);
#endif
#if !defined(WM_NOCOUNTRY)
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("country:         "), (String)country.cc);
#endif
    DEBUG_WM(F("beacon_interval: "), (String)config.beacon_interval + "(ms)");
    DEBUG_WM(FPSTR(D_HR));
#endif
}

/**
 * [debugPlatformInfo description]
 * @access public
 * @return {[type]} [description]
 */
void ThingsCloudWiFiManager::debugPlatformInfo()
{
#ifdef ESP8266
    system_print_meminfo();
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("getCoreVersion():         "), ESP.getCoreVersion());
    DEBUG_WM(F("system_get_sdk_version(): "), system_get_sdk_version());
    DEBUG_WM(F("system_get_boot_version():"), system_get_boot_version());
    DEBUG_WM(F("getFreeHeap():            "), (String)ESP.getFreeHeap());
#endif
#elif defined(ESP32)
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(F("Free heap:       "), ESP.getFreeHeap());
    DEBUG_WM(F("ESP SDK version: "), ESP.getSdkVersion());
#endif
// esp_chip_info_t chipInfo;
// esp_chip_info(&chipInfo);
#ifdef WM_DEBUG_LEVEL
// DEBUG_WM("Chip Info: Model: ",chipInfo.model);
// DEBUG_WM("Chip Info: Cores: ",chipInfo.cores);
// DEBUG_WM("Chip Info: Rev: ",chipInfo.revision);
// DEBUG_WM(printf("Chip Info: Model: %d, cores: %d, revision: %d", chipInfo.model.c_str(), chipInfo.cores, chipInfo.revision));
// DEBUG_WM("Chip Rev: ",(String)ESP.getChipRevision());
#endif
    // core version is not avail
#endif
}

int ThingsCloudWiFiManager::getRSSIasQuality(int RSSI)
{
    int quality = 0;

    if (RSSI <= -100)
    {
        quality = 0;
    }
    else if (RSSI >= -50)
    {
        quality = 100;
    }
    else
    {
        quality = 2 * (RSSI + 100);
    }
    return quality;
}
/** Is this an IP? */
boolean ThingsCloudWiFiManager::isIp(String str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9'))
        {
            return false;
        }
    }
    return true;
}
/** IP to String? */
String ThingsCloudWiFiManager::toStringIp(IPAddress ip)
{
    String res = "";
    for (int i = 0; i < 3; i++)
    {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

boolean ThingsCloudWiFiManager::validApPassword()
{
    // check that ap password is valid, return false
    if (_apPassword.isEmpty())
        _apPassword = "";
    if (_apPassword != "")
    {
        if (_apPassword.length() < 8 || _apPassword.length() > 63)
        {
#ifdef WM_DEBUG_LEVEL
            DEBUG_WM(F("AccessPoint set password is INVALID or <8 chars"));
#endif
            _apPassword = "";
            return false; // @todo FATAL or fallback to empty ?
        }
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("AccessPoint set password is VALID"));
        DEBUG_WM(DEBUG_DEV, "ap pass", _apPassword);
#endif
    }
    return true;
}
/**
 * encode htmlentities
 * @since $dev
 * @param  string str  string to replace entities
 * @return string      encoded string
 */
String ThingsCloudWiFiManager::htmlEntities(String str)
{
    str.replace("&", "&amp;");
    str.replace("<", "&lt;");
    str.replace(">", "&gt;");
    // str.replace("-","&ndash;");
    // str.replace("\"","&quot;");
    // str.replace("/": "&#x2F;");
    // str.replace("`": "&#x60;");
    // str.replace("=": "&#x3D;");
    return str;
}
/**
 * [getWLStatusString description]
 * @access public
 * @param  {[type]} uint8_t status        [description]
 * @return {[type]}         [description]
 */
String ThingsCloudWiFiManager::getWLStatusString(uint8_t status)
{
    if (status <= 7)
        return WIFI_STA_STATUS[status];
    return FPSTR(S_NA);
}

String ThingsCloudWiFiManager::getWLStatusString()
{
    uint8_t status = WiFi.status();
    if (status <= 7)
        return WIFI_STA_STATUS[status];
    return FPSTR(S_NA);
}

String ThingsCloudWiFiManager::encryptionTypeStr(uint8_t authmode)
{
#ifdef WM_DEBUG_LEVEL
// DEBUG_WM("enc_tye: ",authmode);
#endif
    return AUTH_MODE_NAMES[authmode];
}

String ThingsCloudWiFiManager::getModeString(uint8_t mode)
{
    if (mode <= 3)
        return WIFI_MODES[mode];
    return FPSTR(S_NA);
}

bool ThingsCloudWiFiManager::WiFiSetCountry()
{
    if (_wificountry == "")
        return false; // skip not set

#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("WiFiSetCountry to"), _wificountry);
#endif

    /*
     * @return
     *    - ESP_OK: succeed
     *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by eps_wifi_init
     *    - ESP_ERR_WIFI_IF: invalid interface
     *    - ESP_ERR_WIFI_ARG: invalid argument
     *    - others: refer to error codes in esp_err.h
     */

    // @todo move these definitions, and out of cpp `esp_wifi_set_country(&WM_COUNTRY_US)`
    bool ret = true;
// ret = esp_wifi_set_bandwidth(WIFI_IF_AP,WIFI_BW_HT20); // WIFI_BW_HT40
#ifdef ESP32
    esp_err_t err = ESP_OK;
    // @todo check if wifi is init, no idea how, doesnt seem to be exposed atm ( might be now! )
    if (WiFi.getMode() == WIFI_MODE_NULL)
    {
        DEBUG_WM(DEBUG_ERROR, "[ERROR] cannot set country, wifi not init");
    } // exception if wifi not init!
    else if (_wificountry == "US")
        err = esp_wifi_set_country(&WM_COUNTRY_US);
    else if (_wificountry == "JP")
        err = esp_wifi_set_country(&WM_COUNTRY_JP);
    else if (_wificountry == "CN")
        err = esp_wifi_set_country(&WM_COUNTRY_CN);
#ifdef WM_DEBUG_LEVEL
    else
    {
        DEBUG_WM(DEBUG_ERROR, "[ERROR] country code not found");
    }
    if (err)
    {
        if (err == ESP_ERR_WIFI_NOT_INIT)
            DEBUG_WM(DEBUG_ERROR, "[ERROR] ESP_ERR_WIFI_NOT_INIT");
        else if (err == ESP_ERR_INVALID_ARG)
            DEBUG_WM(DEBUG_ERROR, "[ERROR] ESP_ERR_WIFI_ARG");
        else if (err != ESP_OK)
            DEBUG_WM(DEBUG_ERROR, "[ERROR] unknown error", (String)err);
    }
#endif
    ret = err == ESP_OK;

#elif defined(ESP8266) && !defined(WM_NOCOUNTRY)
    // if(WiFi.getMode() == WIFI_OFF); // exception if wifi not init!
    if (_wificountry == "US")
        ret = wifi_set_country((wifi_country_t *)&WM_COUNTRY_US);
    else if (_wificountry == "JP")
        ret = wifi_set_country((wifi_country_t *)&WM_COUNTRY_JP);
    else if (_wificountry == "CN")
        ret = wifi_set_country((wifi_country_t *)&WM_COUNTRY_CN);
#ifdef WM_DEBUG_LEVEL
    else
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] country code not found"));
#endif
#endif

#ifdef WM_DEBUG_LEVEL
    if (ret)
        DEBUG_WM(DEBUG_VERBOSE, F("[OK] esp_wifi_set_country: "), _wificountry);
    else
        DEBUG_WM(DEBUG_ERROR, F("[ERROR] esp_wifi_set_country failed"));
#endif
    return ret;
}

// set mode ignores WiFi.persistent
bool ThingsCloudWiFiManager::WiFi_Mode(WiFiMode_t m, bool persistent)
{
    bool ret;
#ifdef ESP8266
    if ((wifi_get_opmode() == (uint8)m) && !persistent)
    {
        return true;
    }
    ETS_UART_INTR_DISABLE();
    if (persistent)
        ret = wifi_set_opmode(m);
    else
        ret = wifi_set_opmode_current(m);
    ETS_UART_INTR_ENABLE();
    return ret;
#elif defined(ESP32)
    if (persistent && esp32persistent)
        WiFi.persistent(true);
    ret = WiFi.mode(m); // @todo persistent check persistant mode , NI
    if (persistent && esp32persistent)
        WiFi.persistent(false);
    return ret;
#endif
}
bool ThingsCloudWiFiManager::WiFi_Mode(WiFiMode_t m)
{
    return WiFi_Mode(m, false);
}

// sta disconnect without persistent
bool ThingsCloudWiFiManager::WiFi_Disconnect()
{
#ifdef ESP8266
    if ((WiFi.getMode() & WIFI_STA) != 0)
    {
        bool ret;
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_DEV, F("WiFi station disconnect"));
#endif
        ETS_UART_INTR_DISABLE(); // @todo probably not needed
        ret = wifi_station_disconnect();
        ETS_UART_INTR_ENABLE();
        return ret;
    }
#elif defined(ESP32)
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("WiFi station disconnect"));
#endif
    return WiFi.disconnect(); // not persistent atm
#endif
    return false;
}

// toggle STA without persistent
bool ThingsCloudWiFiManager::WiFi_enableSTA(bool enable, bool persistent)
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("WiFi_enableSTA"), (String)enable ? "enable" : "disable");
#endif
#ifdef ESP8266
    WiFiMode_t newMode;
    WiFiMode_t currentMode = WiFi.getMode();
    bool isEnabled = (currentMode & WIFI_STA) != 0;
    if (enable)
        newMode = (WiFiMode_t)(currentMode | WIFI_STA);
    else
        newMode = (WiFiMode_t)(currentMode & (~WIFI_STA));

    if ((isEnabled != enable) || persistent)
    {
        if (enable)
        {
#ifdef WM_DEBUG_LEVEL
            if (persistent)
                DEBUG_WM(DEBUG_DEV, F("enableSTA PERSISTENT ON"));
#endif
            return WiFi_Mode(newMode, persistent);
        }
        else
        {
            return WiFi_Mode(newMode, persistent);
        }
    }
    else
    {
        return true;
    }
#elif defined(ESP32)
    bool ret;
    if (persistent && esp32persistent)
        WiFi.persistent(true);
    ret = WiFi.enableSTA(enable); // @todo handle persistent when it is implemented in platform
    if (persistent && esp32persistent)
        WiFi.persistent(false);
    return ret;
#endif
}

bool ThingsCloudWiFiManager::WiFi_enableSTA(bool enable)
{
    return WiFi_enableSTA(enable, false);
}

bool ThingsCloudWiFiManager::WiFi_eraseConfig()
{
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_DEV, F("WiFi_eraseConfig"));
#endif

#ifdef ESP8266
#ifndef WM_FIXERASECONFIG
    return ESP.eraseConfig();
#else
    // erase config BUG replacement
    // https://github.com/esp8266/Arduino/pull/3635
    const size_t cfgSize = 0x4000;
    size_t cfgAddr = ESP.getFlashChipSize() - cfgSize;

    for (size_t offset = 0; offset < cfgSize; offset += SPI_FLASH_SEC_SIZE)
    {
        if (!ESP.flashEraseSector((cfgAddr + offset) / SPI_FLASH_SEC_SIZE))
        {
            return false;
        }
    }
    return true;
#endif
#elif defined(ESP32)

    bool ret;
    WiFi.mode(WIFI_AP_STA); // cannot erase if not in STA mode !
    WiFi.persistent(true);
    ret = WiFi.disconnect(true, true);
    delay(500);
    WiFi.persistent(false);
    return ret;
#endif
}

uint8_t ThingsCloudWiFiManager::WiFi_softap_num_stations()
{
#ifdef ESP8266
    return wifi_softap_get_station_num();
#elif defined(ESP32)
    return WiFi.softAPgetStationNum();
#endif
}

bool ThingsCloudWiFiManager::WiFi_hasAutoConnect()
{
    return WiFi_SSID(true) != "";
}

String ThingsCloudWiFiManager::WiFi_SSID(bool persistent) const
{

#ifdef ESP8266
    struct station_config conf;
    if (persistent)
        wifi_station_get_config_default(&conf);
    else
        wifi_station_get_config(&conf);

    char tmp[33]; // ssid can be up to 32chars, => plus null term
    memcpy(tmp, conf.ssid, sizeof(conf.ssid));
    tmp[32] = 0; // nullterm in case of 32 char ssid
    return String(reinterpret_cast<char *>(tmp));

#elif defined(ESP32)
    if (persistent)
    {
        wifi_config_t conf;
        esp_wifi_get_config(WIFI_IF_STA, &conf);
        return String(reinterpret_cast<const char *>(conf.sta.ssid));
    }
    else
    {
        if (WiFiGenericClass::getMode() == WIFI_MODE_NULL)
        {
            return String();
        }
        wifi_ap_record_t info;
        if (!esp_wifi_sta_get_ap_info(&info))
        {
            return String(reinterpret_cast<char *>(info.ssid));
        }
        return String();
    }
#endif
}

String ThingsCloudWiFiManager::WiFi_psk(bool persistent) const
{
#ifdef ESP8266
    struct station_config conf;

    if (persistent)
        wifi_station_get_config_default(&conf);
    else
        wifi_station_get_config(&conf);

    char tmp[65]; // psk is 64 bytes hex => plus null term
    memcpy(tmp, conf.password, sizeof(conf.password));
    tmp[64] = 0; // null term in case of 64 byte psk
    return String(reinterpret_cast<char *>(tmp));

#elif defined(ESP32)
    // only if wifi is init
    if (WiFiGenericClass::getMode() == WIFI_MODE_NULL)
    {
        return String();
    }
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<char *>(conf.sta.password));
#endif
}

#ifdef ESP32
#ifdef WM_ARDUINOEVENTS
void ThingsCloudWiFiManager::WiFiEvent(WiFiEvent_t event, arduino_event_info_t info)
{
#else
void ThingsCloudWiFiManager::WiFiEvent(WiFiEvent_t event, system_event_info_t info)
{
#define wifi_sta_disconnected disconnected
#define ARDUINO_EVENT_WIFI_STA_DISCONNECTED SYSTEM_EVENT_STA_DISCONNECTED
#define ARDUINO_EVENT_WIFI_SCAN_DONE SYSTEM_EVENT_SCAN_DONE
#endif
    if (!_hasBegun)
    {
#ifdef WM_DEBUG_LEVEL
        // DEBUG_WM(DEBUG_VERBOSE,"[ERROR] WiFiEvent, not ready");
#endif
        // Serial.println(F("\n[EVENT] WiFiEvent logging (wm debug not available)"));
        // Serial.print(F("[EVENT] ID: "));
        // Serial.println(event);
        return;
    }
#ifdef WM_DEBUG_LEVEL
// DEBUG_WM(DEBUG_VERBOSE,"[EVENT]",event);
#endif
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    {
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("[EVENT] WIFI_REASON: "), info.wifi_sta_disconnected.reason);
#endif
        if (info.wifi_sta_disconnected.reason == WIFI_REASON_AUTH_EXPIRE || info.wifi_sta_disconnected.reason == WIFI_REASON_AUTH_FAIL)
        {
            _lastconxresulttmp = 7; // hack in wrong password internally, sdk emit WIFI_REASON_AUTH_EXPIRE on some routers on auth_fail
        }
        else
            _lastconxresulttmp = WiFi.status();
#ifdef WM_DEBUG_LEVEL
        if (info.wifi_sta_disconnected.reason == WIFI_REASON_NO_AP_FOUND)
            DEBUG_WM(DEBUG_VERBOSE, F("[EVENT] WIFI_REASON: NO_AP_FOUND"));
        if (info.wifi_sta_disconnected.reason == WIFI_REASON_ASSOC_FAIL)
        {
            if (_aggresiveReconn)
                _connectRetries += 4;
            DEBUG_WM(DEBUG_VERBOSE, F("[EVENT] WIFI_REASON: AUTH FAIL"));
        }
#endif
#ifdef esp32autoreconnect
#ifdef WM_DEBUG_LEVEL
        DEBUG_WM(DEBUG_VERBOSE, F("[Event] SYSTEM_EVENT_STA_DISCONNECTED, reconnecting"));
#endif
        WiFi.reconnect();
#endif
    }
    else if (event == ARDUINO_EVENT_WIFI_SCAN_DONE)
    {
        uint16_t scans = WiFi.scanComplete();
        WiFi_scanComplete(scans);
    }
}
#endif

void ThingsCloudWiFiManager::WiFi_autoReconnect()
{
#ifdef ESP8266
    WiFi.setAutoReconnect(_wifiAutoReconnect);
#elif defined(ESP32)
    // if(_wifiAutoReconnect){
    // @todo move to seperate method, used for event listener now
#ifdef WM_DEBUG_LEVEL
    DEBUG_WM(DEBUG_VERBOSE, F("ESP32 event handler enabled"));
#endif
    using namespace std::placeholders;
    wm_event_id = WiFi.onEvent(std::bind(&ThingsCloudWiFiManager::WiFiEvent, this, _1, _2));
    // }
#endif
}

#endif
