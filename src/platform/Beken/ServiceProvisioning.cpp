/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include <platform/CHIPDeviceLayer.h>
#include <support/CodeUtils.h>
#include <support/logging/CHIPLogging.h>

#include "ServiceProvisioning.h"

#include "include.h"
#include "str_pub.h"
#include "mem_pub.h"
#include "wlan_ui_pub.h"

using namespace ::chip::DeviceLayer;

CHIP_ERROR SetWiFiStationProvisioning(const char * ssid, const char * key)
{
    ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Disabled);
    network_InitTypeDef_st network_cfg;

    memset(&network_cfg, 0, sizeof(network_InitTypeDef_st));

    network_cfg.wifi_mode = BK_STATION;
    strcpy(network_cfg.wifi_ssid, ssid);
    strcpy(network_cfg.wifi_key, key);
    network_cfg.dhcp_mode = DHCP_CLIENT;
    bk_wlan_start(&network_cfg);
    ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Enabled);

    return CHIP_NO_ERROR;
}
