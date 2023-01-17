/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
/* this file behaves like a config.h, comes first */
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/ConnectivityManager.h>

#include <platform/internal/GenericConnectivityManagerImpl_UDP.ipp>

#if INET_CONFIG_ENABLE_TCP_ENDPOINT
#include <platform/internal/GenericConnectivityManagerImpl_TCP.ipp>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#include <platform/internal/GenericConnectivityManagerImpl_BLE.ipp>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/internal/GenericConnectivityManagerImpl_Thread.ipp>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
#include <platform/internal/GenericConnectivityManagerImpl_WiFi.ipp>
#endif

#include <platform/internal/BLEManager.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <lwip/dns.h>
#include <lwip/ip_addr.h>
#include <lwip/nd6.h>
#include <lwip/netif.h>

#include "wlan_ui_pub.h"
#include "flash_namespace_value.h"

#define BEKEN_WIFI_INFO   "BekenWiFi"

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::TLV;
using namespace ::chip::DeviceLayer::Internal;
using namespace ::chip::DeviceLayer::NetworkCommissioning;

namespace chip {
namespace DeviceLayer {

ConnectivityManagerImpl ConnectivityManagerImpl::sInstance;
// NetworkCommissioning::BekenWiFiDriver::WiFiNetwork mWifiNetconf;
static void wlan_status_cb(void *ctxt);

// ==================== ConnectivityManager Platform Internal Methods ====================

CHIP_ERROR ConnectivityManagerImpl::_Init()
{
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
    mLastStationConnectFailTime   = System::Clock::kZero;
    mWiFiStationMode              = kWiFiStationMode_Disabled;
    mWiFiStationState             = kWiFiStationState_NotConnected;
    mWiFiStationReconnectInterval = System::Clock::Milliseconds32(CHIP_DEVICE_CONFIG_WIFI_STATION_RECONNECT_INTERVAL);
    mkBKEventType = DeviceEventType::kBKWiFiRW_EVT_STA_IDLE_Event;
    mFlags.SetRaw(0);

    DriveStationState();
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

    return CHIP_NO_ERROR;
}

void ConnectivityManagerImpl::_OnPlatformEvent(const ChipDeviceEvent * event)
{
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
    switch (event->Type)
    {
    case DeviceEventType::kBKWiFiRW_EVT_STA_IDLE_Event:
        break;
    case DeviceEventType::kBKWIFIRW_EVT_STA_CONNECTING_Event:
        ChangeWiFiStationState(kWiFiStationState_Connecting);
        break;
    case DeviceEventType::kBKWIFIRW_EVT_STA_BEACON_LOSE_Event:
    case DeviceEventType::kBKWIFIRW_EVT_STA_PASSWORD_WRONG_Event:
    case DeviceEventType::kBKWIFIRW_EVT_STA_NO_AP_FOUND_Event:
    case DeviceEventType::kBKWIFIRW_EVT_STA_ASSOC_FULL_Event:
    case DeviceEventType::kBKWIFIRW_EVT_STA_CONNECT_FAILED_Event:
        mkBKEventType = event->Type;
        ChangeWiFiStationState(kWiFiStationState_Connecting_Failed);
        break;
    case DeviceEventType::kBKWIFIRW_EVT_STA_DISCONNECTED_Event:
        ChangeWiFiStationState(kWiFiStationState_Disconnecting);
        break;
    case DeviceEventType::kBKWIFIRW_EVT_STA_CONNECTED_Event:
        //ChangeWiFiStationState(kWiFiStationState_Connecting_Succeeded);
        break;
    case DeviceEventType::kBKWIFIRW_EVT_STA_GOT_IP_Event:
        ChangeWiFiStationState(kWiFiStationState_Connected);
        break;
    default:
        break;
    }

    if (event->Type >= DeviceEventType::kBKWiFiRW_EVT_STA_IDLE_Event &&
        event->Type <= DeviceEventType::kBKWIFIRW_EVT_STA_GOT_IP_Event)
    {
        ChipLogProgress(DeviceLayer, "%s get event %d", __FUNCTION__, event->Type);
        DeviceLayer::SystemLayer().ScheduleWork(DriveStationState, NULL);
    }
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI
}

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
CHIP_ERROR ConnectivityManagerImpl::_SetWiFiStationMode(WiFiStationMode val)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    VerifyOrExit(val != kWiFiStationMode_NotSupported, err = CHIP_ERROR_INVALID_ARGUMENT);

    if (val == kWiFiStationMode_Disabled)
    {
        //bk_wlan_status_unregister_cb();
        wlan_sta_disconnect();
    }
    if (val == kWiFiStationMode_Enabled)
    {
        // TODO deal wifi not post any event
        bk_wlan_status_register_cb(wlan_status_cb);
        mWiFiStationState = kWiFiStationState_NotConnected;
    }
    if (mWiFiStationMode != val)
    {
        ChipLogProgress(DeviceLayer, "WiFi station mode change: %s -> %s", WiFiStationModeToStr(mWiFiStationMode),
                        WiFiStationModeToStr(val));
    }
    mWiFiStationMode = val;
exit:
    return err;
}

bool ConnectivityManagerImpl::_IsWiFiStationProvisioned(void)
{
    char ssid[DeviceLayer::Internal::kMaxWiFiSSIDLength];
    size_t ssidLen = 0;
    CHIP_ERROR err   = CHIP_NO_ERROR;
    err              = PersistedStorage::KeyValueStoreMgr().Get("wifi-ssid", ssid, sizeof(ssid), &ssidLen);
    if (err == CHIP_NO_ERROR)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void ConnectivityManagerImpl::_ClearWiFiStationProvision(void)
{
    // TODO delete wifi information in NetworkCommissioningDriver
}

// ==================== ConnectivityManager Private Methods ====================

static void wlan_status_cb(void *ctxt)
{
    int notice_event = *(unsigned int*)ctxt;
    ChipDeviceEvent event;
    memset(&event, 0, sizeof(event));
    switch (notice_event)
    {
    case RW_EVT_STA_IDLE:
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_IDLE");
        event.Type = DeviceEventType::kBKWiFiRW_EVT_STA_IDLE_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_CONNECTING:
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_CONNECTING");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_CONNECTING_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_BEACON_LOSE:
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_BEACON_LOSE");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_BEACON_LOSE_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_PASSWORD_WRONG:
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_PASSWORD_WRONG");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_PASSWORD_WRONG_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_NO_AP_FOUND:
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_NO_AP_FOUND");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_NO_AP_FOUND_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_ASSOC_FULL:
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_ASSOC_FULL");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_ASSOC_FULL_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_DISCONNECTED: /* disconnect with server */
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_DISCONNECTED");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_DISCONNECTED_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_CONNECT_FAILED: /* authentication failed */
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_CONNECT_FAILED");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_CONNECT_FAILED_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_CONNECTED: /* authentication success */
        // if post CONNECTED event, GOT_IP event will losed
        //ChipLogProgress(DeviceLayer, "RW_EVT_STA_CONNECTED");
        //event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_CONNECTED_Event;
        //PlatformMgr().PostEventOrDie(&event);
        break;
    case RW_EVT_STA_GOT_IP:
        ChipLogProgress(DeviceLayer, "RW_EVT_STA_GOT_IP");
        event.Type = DeviceEventType::kBKWIFIRW_EVT_STA_GOT_IP_Event;
        PlatformMgr().PostEventOrDie(&event);
        break;
    default:
        ChipLogProgress(DeviceLayer, "unSupported wifi status:%d", notice_event);
        break;
    }
}
void ConnectivityManagerImpl::DriveStationState()
{
    // connect preview network
    ChipLogProgress(DeviceLayer, "%s", __FUNCTION__);

    if (mWiFiStationMode == kWiFiStationMode_Disabled)
    {
    }
    else
    {
        if (mWiFiStationState == kWiFiStationState_Connected)
        {
            if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled)
            {
                SetWiFiStationMode(kWiFiStationMode_ApplicationControlled);
            }
            // OnStationConnected();
            UpdateInternetConnectivityState();
            BekenWiFiDriver::GetInstance().OnConnectWiFiNetwork();
        }
        else if (mWiFiStationState == kWiFiStationState_Disconnecting)
        {
            UpdateInternetConnectivityState();
        }
        else if (mWiFiStationState == kWiFiStationState_Connecting_Failed)
        {
            if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled)
            {
                SetWiFiStationMode(kWiFiStationMode_Disabled);
            }
            UpdateInternetConnectivityState();
            BekenWiFiDriver::GetInstance().OnConnectWiFiNetwork();
        }
    }
}

void ConnectivityManagerImpl::OnStationConnected()
{
    // Alert other components of the new state.
    ChipDeviceEvent event;
    event.Type                          = DeviceEventType::kWiFiConnectivityChange;
    event.WiFiConnectivityChange.Result = kConnectivity_Established;
    PlatformMgr().PostEventOrDie(&event);
}

void ConnectivityManagerImpl::OnStationDisconnected()
{
    // Alert other components of the new state.
    ChipDeviceEvent event;
    event.Type                          = DeviceEventType::kWiFiConnectivityChange;
    event.WiFiConnectivityChange.Result = kConnectivity_Lost;
    PlatformMgr().PostEventOrDie(&event);
}

void ConnectivityManagerImpl::ChangeWiFiStationState(WiFiStationState newState)
{
    if (mWiFiStationState != newState)
    {
        ChipLogProgress(DeviceLayer, "WiFi station state change: %s -> %s", WiFiStationStateToStr(mWiFiStationState),
                        WiFiStationStateToStr(newState));
        mWiFiStationState = newState;
    }
}

void ConnectivityManagerImpl::DriveStationState(::chip::System::Layer * aLayer, void * aAppState)
{
    sInstance.DriveStationState();
}


void ConnectivityManagerImpl::UpdateInternetConnectivityState(void)
{
    bool haveIPv4Conn      = false;
    bool haveIPv6Conn      = false;
    const bool hadIPv4Conn = mFlags.Has(ConnectivityFlags::kHaveIPv4InternetConnectivity);
    const bool hadIPv6Conn = mFlags.Has(ConnectivityFlags::kHaveIPv6InternetConnectivity);
    static uint8_t prev_ipv6_count = 0;
    uint8_t ipv6_count = 0;
    if (mWiFiStationState == kWiFiStationState_Connected)
    {
        // Get the LwIP netif for the WiFi station interface.
        struct netif * netif = netif_list;

        // If the WiFi station interface is up...
        if (netif != NULL && netif_is_up(netif) && netif_is_link_up(netif))
        {
            // Check if a DNS server is currently configured.  If so...
            ip_addr_t dnsServerAddr = *dns_getserver(0);
            if (!ip_addr_isany_val(dnsServerAddr))
            {
                // If the station interface has been assigned an IPv4 address, and has
                // an IPv4 gateway, then presume that the device has IPv4 Internet
                // connectivity.
                if (!ip4_addr_isany_val(*netif_ip4_addr(netif)) && !ip4_addr_isany_val(*netif_ip4_gw(netif)))
                {
                    haveIPv4Conn = true;
                }

                for (uint8_t i = 0; i < MAX_IPV6_ADDRESSES; i++)
                {
                    if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)))
                    {
                        haveIPv6Conn = true;
                        ipv6_count += 1;
                    }
                }
            }
        }
    }

    // If the internet connectivity state has changed...
    if (haveIPv4Conn != hadIPv4Conn || haveIPv6Conn != hadIPv6Conn)
    {
        // Update the current state.
        mFlags.Set(ConnectivityFlags::kHaveIPv4InternetConnectivity, haveIPv4Conn)
            .Set(ConnectivityFlags::kHaveIPv6InternetConnectivity, haveIPv6Conn);

        // Alert other components of the state change.
        ChipDeviceEvent event;
        event.Type                            = DeviceEventType::kInternetConnectivityChange;
        event.InternetConnectivityChange.IPv4 = GetConnectivityChange(hadIPv4Conn, haveIPv4Conn);
        event.InternetConnectivityChange.IPv6 = GetConnectivityChange(hadIPv6Conn, haveIPv6Conn);
        PlatformMgr().PostEventOrDie(&event);
    }
    if (haveIPv4Conn != hadIPv4Conn)
    {
        ChipLogProgress(DeviceLayer, "%s Internet connectivity %s", "IPv4", (haveIPv4Conn) ? "ESTABLISHED" : "LOST");
        ChipDeviceEvent event;
        event.Type = DeviceEventType::kInterfaceIpAddressChanged;
        if (haveIPv4Conn)
        {
            event.InterfaceIpAddressChanged.Type = InterfaceIpChangeType::kIpV4_Assigned;
        }
        else
        {
            event.InterfaceIpAddressChanged.Type = InterfaceIpChangeType::kIpV4_Lost;
        }
        PlatformMgr().PostEventOrDie(&event);
    }

    if (ipv6_count != prev_ipv6_count)
    {
        ChipLogProgress(DeviceLayer, "%s Internet connectivity %s", "IPv6",
                        (ipv6_count > prev_ipv6_count) ? "ESTABLISHED" : "LOST");
        ChipDeviceEvent event;
        event.Type = DeviceEventType::kInterfaceIpAddressChanged;
        if (ipv6_count > prev_ipv6_count)
        {
            event.InterfaceIpAddressChanged.Type = InterfaceIpChangeType::kIpV6_Assigned;
        }
        else
        {
            event.InterfaceIpAddressChanged.Type = InterfaceIpChangeType::kIpV6_Lost;
        }
        prev_ipv6_count = ipv6_count;
        PlatformMgr().PostEventOrDie(&event);
    }
}

#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

} // namespace DeviceLayer
} // namespace chip
