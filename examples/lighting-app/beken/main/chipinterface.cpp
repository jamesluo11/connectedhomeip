/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include "CHIPDeviceManager.h"
#include "DeviceCallbacks.h"
#include "Server.h"

#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <platform/CHIPDeviceLayer.h>
#include <support/CHIPMem.h>

#include <app/clusters/identify-server/identify-server.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <app/server/OnboardingCodesUtil.h>
#include <lib/support/ErrorStr.h>
#include <platform/Beken/BekenConfig.h>
#include <platform/Beken/NetworkCommissioningDriver.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>

using namespace ::chip;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceManager;
using namespace ::chip::DeviceLayer;

//#define QRCODE_BASE_URL "https://dhrishi.github.io/connectedhomeip/qrcode.html"
//#define EXAMPLE_VENDOR_TAG_IP 1

static DeviceCallbacks EchoCallbacks;

namespace {
app::Clusters::NetworkCommissioning::Instance
    sWiFiNetworkCommissioningInstance(0 /* Endpoint Id */, &(NetworkCommissioning::BekenWiFiDriver::GetInstance()));
} // namespace

// need to check CONFIG_RENDEZVOUS_MODE
bool isRendezvousBLE()
{
    RendezvousInformationFlags flags = RendezvousInformationFlags(CONFIG_RENDEZVOUS_MODE);
    return flags.Has(RendezvousInformationFlag::kBLE);
}

void OnIdentifyStart(Identify *)
{
    ChipLogProgress(Zcl, "OnIdentifyStart");
}

void OnIdentifyStop(Identify *)
{
    ChipLogProgress(Zcl, "OnIdentifyStop");
}

void OnTriggerEffect(Identify * identify)
{
    switch (identify->mCurrentEffectIdentifier)
    {
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BLINK:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BLINK");
        break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BREATHE:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BREATHE");
        break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_OKAY:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_OKAY");
        break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_CHANNEL_CHANGE:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_CHANNEL_CHANGE");
        break;
    default:
        ChipLogProgress(Zcl, "No identifier effect");
        return;
    }
}

static Identify gIdentify1 = {
    chip::EndpointId{ 1 }, OnIdentifyStart, OnIdentifyStop, EMBER_ZCL_IDENTIFY_IDENTIFY_TYPE_VISIBLE_LED, OnTriggerEffect,
};

//Warkaround for ld error:undefined reference to '__sync_synchronize'
//refer to https://stackoverflow.com/questions/64658430/gnu-arm-embedded-toolchain-undefined-reference-to-sync-synchronize
extern "C" void __sync_synchronize() {}

extern "C" unsigned int __atomic_fetch_add_4(volatile void * ptr, unsigned int val, int memorder)
{
    return (*(unsigned int*)ptr + val);
}

extern "C" bool __atomic_compare_exchange_4(volatile void* pulDestination, void* ulComparand, unsigned int desired, bool weak, int success_memorder, int failure_memorder)
{
    bool ulReturnValue;
    if( *(unsigned int*)pulDestination == *(unsigned int*)ulComparand )
    {
        *(unsigned int*)pulDestination = desired;
         ulReturnValue = true;
    }
    else
    {
        *(unsigned int*)ulComparand = *(unsigned int*)pulDestination;
        ulReturnValue = false;
    }
    return ulReturnValue;
}

extern "C" unsigned int __atomic_fetch_sub_4(volatile void * ptr, unsigned int val, int memorder)
{
    return (*(unsigned int*)ptr + val);
}
extern "C" bool __atomic_compare_exchange_1(volatile void* pulDestination, void* ulComparand, unsigned char desired, bool weak, int success_memorder, int failure_memorder)
{
    bool ulReturnValue;
    if( *(unsigned char*)pulDestination == *(unsigned char*)ulComparand )
    {
        *(unsigned char*)pulDestination = desired;
         ulReturnValue = true;
    }
    else
    {
        *(unsigned char*)ulComparand = *(unsigned char*)pulDestination;
        ulReturnValue = false;
    }
    return ulReturnValue;
}

extern "C" unsigned int __atomic_fetch_and_4(volatile void *pulDestination, unsigned int ulValue, int memorder)
{
    unsigned int ulCurrent;

    ulCurrent = *(unsigned int*)pulDestination;
    *(unsigned int*)pulDestination &= ulValue;
    return ulCurrent;
}

extern "C" bool __sync_bool_compare_and_swap_4(volatile void* ptr, unsigned int oldval, unsigned int newval)
{
    if(*(unsigned int*)ptr == oldval)
    {
        *(unsigned int*)ptr = newval;
        return true;
    }
    else
    {
        return false;
    }
}
/* stub for __libc_init_array */
extern "C" void _fini(void) { }
extern "C" void _init(void) {;}

extern "C" void ChipTest(void)
{
    ChipLogProgress(Zcl, "ChipTest");
    CHIP_ERROR err = CHIP_NO_ERROR;
    //initPref();

    CHIPDeviceManager & deviceMgr = CHIPDeviceManager::GetInstance();
    err                           = deviceMgr.Init(&EchoCallbacks); //start the CHIP task
    if (err != CHIP_NO_ERROR)
    {
        ChipLogProgress(Zcl, "DeviceManagerInit() - ERROR!");
    }
    else
    {
        ChipLogProgress(Zcl, "DeviceManagerInit() - OK");
    }
    chip::Server::GetInstance().Init();
    // Initialize device attestation config
    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());
    sWiFiNetworkCommissioningInstance.Init();

    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

    while (true)
        vTaskDelay(pdMS_TO_TICKS(50));
}

bool lowPowerClusterSleep()
{
    return true;
}
