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

/**
 * @file DeviceCallbacks.cpp
 *
 * Implements all the callbacks to the application from the CHIP Stack
 *
 **/

#include "DeviceCallbacks.h"
#include "BkLightingManager.h"

#include <common/CHIPDeviceManager.h>

#include <app-common/zap-generated/attribute-id.h>
#include <app-common/zap-generated/cluster-id.h>
#include <app/CommandHandler.h>
#include <app/server/Dnssd.h>
#include <app/util/af.h>
#include <app/util/basic-types.h>
#include <app/util/util.h>
#include <lib/dnssd/Advertiser.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <lib/support/logging/Constants.h>

static const char * TAG = "app-devicecallbacks";

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::System;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceManager;
using namespace ::chip::Logging;

uint32_t identifyTimerCount;
constexpr uint32_t kIdentifyTimerDelayMS = 250;

void AppDeviceCallbacks::PostAttributeChangeCallback(EndpointId endpointId, ClusterId clusterId, AttributeId attributeId,
                                                     uint8_t type, uint16_t size, uint8_t * value)
{
    switch (clusterId)
    {
    case ZCL_ON_OFF_CLUSTER_ID:
        OnOnOffPostAttributeChangeCallback(endpointId, attributeId, value);
        break;

    case ZCL_IDENTIFY_CLUSTER_ID:
        OnIdentifyPostAttributeChangeCallback(endpointId, attributeId, value);
        break;

    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
        OnLevelControlAttributeChangeCallback(endpointId, attributeId, value);
        break;

    case ZCL_COLOR_CONTROL_CLUSTER_ID:
        OnColorControlAttributeChangeCallback(endpointId, attributeId, value);
        break;

    default:
        ChipLogProgress(Zcl, "Unknown cluster ID: " ChipLogFormatMEI, ChipLogValueMEI(clusterId));
        break;
    }
}

void AppDeviceCallbacks::OnOnOffPostAttributeChangeCallback(EndpointId endpointId, AttributeId attributeId, uint8_t * value)
{
    VerifyOrExit(attributeId == ZCL_ON_OFF_ATTRIBUTE_ID,
                 ChipLogError(DeviceLayer, "[%s] Unhandled Attribute ID: '0x%04lx", TAG, attributeId));
    VerifyOrExit(endpointId == 1 || endpointId == 2,
                 ChipLogError(DeviceLayer, "[%s] Unexpected EndPoint ID: `0x%02x'", TAG, endpointId));

    // At this point we can assume that value points to a bool value.
    // statusLED1.Set(*value);
    if (*value) 
        BkLightingMgr().InitiateAction(BkLightingManager::ON_ACTION);
    else
        BkLightingMgr().InitiateAction(BkLightingManager::OFF_ACTION);

exit:
    return;
}

void AppDeviceCallbacks::OnLevelControlAttributeChangeCallback(chip::EndpointId endpointId, chip::AttributeId attributeId, uint8_t * value)
{
    VerifyOrExit(attributeId == ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                 ChipLogError(DeviceLayer, "[%s] Unhandled Attribute ID: '0x%04lx", TAG, attributeId));
    VerifyOrExit(endpointId == 1, ChipLogError(DeviceLayer, "[%s] Unexpected EndPoint ID: `0x%02x'", TAG, endpointId));

    BkLightingMgr().SetLevel(*value);

exit:
    return;
}

void AppDeviceCallbacks::OnColorControlAttributeChangeCallback(chip::EndpointId endpointId, chip::AttributeId attributeId, uint8_t * value)
{
    uint8_t hue, saturation;

    VerifyOrExit(attributeId == ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID ||
                     attributeId == ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID,
                 ChipLogError(DeviceLayer, "[%s] Unhandled AttributeId ID: '0x%04lx", TAG, attributeId));
    VerifyOrExit(endpointId == 1, ChipLogError(DeviceLayer, "[%s] Unexpected EndPoint ID: `0x%02x'", TAG, endpointId));

    if (attributeId == ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID)
    {
        hue = *value;
        emberAfReadServerAttribute(endpointId, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_SATURATION_ATTRIBUTE_ID, &saturation, sizeof(uint8_t));
    }
    else
    {
        saturation = *value;
        emberAfReadServerAttribute(endpointId, ZCL_COLOR_CONTROL_CLUSTER_ID, ZCL_COLOR_CONTROL_CURRENT_HUE_ATTRIBUTE_ID, &hue, sizeof(uint8_t));
    }
    BkLightingMgr().SetColor(hue, saturation);

exit:
    return;
}


void IdentifyTimerHandler(Layer * systemLayer, void * appState)
{
    // statusLED1.Animate();

    if (identifyTimerCount)
    {
        systemLayer->StartTimer(Clock::Milliseconds32(kIdentifyTimerDelayMS), IdentifyTimerHandler, appState);
        // Decrement the timer count.
        identifyTimerCount--;
    }
}

void AppDeviceCallbacks::OnIdentifyPostAttributeChangeCallback(EndpointId endpointId, AttributeId attributeId, uint8_t * value)
{
    VerifyOrExit(attributeId == ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                 ChipLogError(DeviceLayer, "[%s] Unhandled Attribute ID: '0x%04lx", TAG, attributeId));
    VerifyOrExit(endpointId == 1, ChipLogError(DeviceLayer, "[%s] Unexpected EndPoint ID: `0x%02x'", TAG, endpointId));

    // timerCount represents the number of callback executions before we stop the timer.
    // value is expressed in seconds and the timer is fired every 250ms, so just multiply value by 4.
    // Also, we want timerCount to be odd number, so the ligth state ends in the same state it starts.
    identifyTimerCount = (*value) * 4;

    DeviceLayer::SystemLayer().CancelTimer(IdentifyTimerHandler, this);
    DeviceLayer::SystemLayer().StartTimer(Clock::Milliseconds32(kIdentifyTimerDelayMS), IdentifyTimerHandler, this);

exit:
    return;
}
