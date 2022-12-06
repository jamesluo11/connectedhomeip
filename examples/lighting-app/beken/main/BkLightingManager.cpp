/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
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

#include "matter_pal.h"
#include "BkLightingManager.h"

#include <lib/support/logging/CHIPLogging.h>
#include <ColorFormat.h>

BkLightingManager BkLightingManager::sLight;

static void lighting_gpio_init()
{
#ifdef BEKEN_SOC_BK7231
    BkGpioInitialize(GPIO6, OUTPUT_NORMAL);
    BkGpioInitialize(GPIO7, OUTPUT_NORMAL);
    BkGpioInitialize(GPIO8, OUTPUT_NORMAL);
    BkGpioOutputHigh(GPIO6);
    BkGpioOutputHigh(GPIO7);
    BkGpioOutputHigh(GPIO8);
#endif

#ifdef BEKEN_SOC_BK7235
    gpio_id_t gpios[] = {GPIO_8, GPIO_9, GPIO_44};
    bk_gpio_disable_input(gpios[i]);
    bk_gpio_enable_output(gpios[i]);
    bk_gpio_disable_pull(gpios[i]);
    bk_gpio_set_output_high(gpios[i]);
#endif
}
static void lighting_gpio_set(BkLightingManager::Action_t aAction)
{
#ifdef BEKEN_SOC_BK7231
    if(aAction == BkLightingManager::ON_ACTION)
    {
        user_pwm_start(BK_PWM_0, 0);
        user_pwm_start(BK_PWM_1, 0);
        user_pwm_start(BK_PWM_2, 0);
    }
    if(aAction == BkLightingManager::OFF_ACTION)
    {
        user_pwm_stop(BK_PWM_0);
        user_pwm_stop(BK_PWM_1);
        user_pwm_stop(BK_PWM_2);
        lighting_gpio_init();
    }
#endif

#ifdef BEKEN_SOC_BK7235
    if(aAction == BkLightingManager::ON_ACTION)
        bk_gpio_set_output_low(GPIO_8);
    if(aAction == BkLightingManager::OFF_ACTION)
        bk_gpio_set_output_high(GPIO_8);
#endif
}
void lighting_color_set(uint8_t r, uint8_t g, uint8_t b)
{
#ifdef BEKEN_SOC_BK7231
    ChipLogProgress(DeviceLayer, "change color set %d,%d,%d",r,g,b);
    user_pwm_update_cfg(BK_PWM_0, 255-r);
    user_pwm_update_cfg(BK_PWM_1, 255-g);
    user_pwm_update_cfg(BK_PWM_2, 255-b);
#endif

#ifdef BEKEN_SOC_BK7235
#endif
}

CHIP_ERROR BkLightingManager::Init()
{
    mState = kState_Off;
    lighting_gpio_init();
    mActionInitiated_CB = lighting_gpio_set;
    return CHIP_NO_ERROR;
}

bool BkLightingManager::IsTurnedOn()
{
    return mState == kState_On;
}

void BkLightingManager::SetCallbacks(BkLightingCallback_fn aActionInitiated_CB, BkLightingCallback_fn aActionCompleted_CB)
{
    mActionInitiated_CB = aActionInitiated_CB;
    mActionCompleted_CB = aActionCompleted_CB;
}

bool BkLightingManager::InitiateAction(Action_t aAction)
{
    // TODO: this function is called InitiateAction because we want to implement some features such as ramping up here.
    bool action_initiated = false;
    State_t new_state;

    switch (aAction)
    {
    case ON_ACTION:
        ChipLogProgress(AppServer, "LightingManager::InitiateAction(ON_ACTION)");
        break;
    case OFF_ACTION:
        ChipLogProgress(AppServer, "LightingManager::InitiateAction(OFF_ACTION)");
        break;
    default:
        ChipLogProgress(AppServer, "LightingManager::InitiateAction(unknown)");
        break;
    }

    // Initiate On/Off Action only when the previous one is complete.
    if (mState == kState_Off && aAction == ON_ACTION)
    {
        action_initiated = true;
        new_state        = kState_On;
    }
    else if (mState == kState_On && aAction == OFF_ACTION)
    {
        action_initiated = true;
        new_state        = kState_Off;
    }

    if (action_initiated)
    {
        if (mActionInitiated_CB)
        {
            mActionInitiated_CB(aAction);
        }

        Set(new_state == kState_On);

        if (mActionCompleted_CB)
        {
            mActionCompleted_CB(aAction);
        }
    }

    return action_initiated;
}

void BkLightingManager::SetColor(uint8_t hue, uint8_t saturation)
{
    mHue           = hue;
    mSaturation    = saturation;
    HsvColor_t hsv = { mHue, mSaturation, mLevel };
    RgbColor_t rgb = HsvToRgb(hsv);

    ChipLogProgress(DeviceLayer, "color hsv update %d,%d,%d", hsv.h, hsv.s, hsv.v);
    lighting_color_set(rgb.r, rgb.g, rgb.b);
}

void BkLightingManager::SetLevel(uint8_t level)
{
    mLevel         = level;
    HsvColor_t hsv = { mHue, mSaturation, mLevel };
    RgbColor_t rgb = HsvToRgb(hsv);

    ChipLogProgress(DeviceLayer, "level hsv update %d,%d,%d", hsv.h, hsv.s, hsv.v);
    lighting_color_set(rgb.r, rgb.g, rgb.b);
}

void BkLightingManager::Set(bool aOn)
{
    if (aOn)
    {
        mState = kState_On;
    }
    else
    {
        mState = kState_Off;
    }
}
