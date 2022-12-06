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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <functional>

#include <lib/core/CHIPError.h>

class BkLightingManager
{
public:
    enum Action_t
    {
        ON_ACTION = 0,
        OFF_ACTION,
        INVALID_ACTION
    } Action;

    enum State_t
    {
        kState_On = 0,
        kState_Off,
    } State;

    CHIP_ERROR Init();
    bool IsTurnedOn();
    bool InitiateAction(Action_t aAction);
    void SetColor(uint8_t hue, uint8_t saturation);
    void SetLevel(uint8_t level);

    using BkLightingCallback_fn = std::function<void(Action_t)>;

    void SetCallbacks(BkLightingCallback_fn aActionInitiated_CB, BkLightingCallback_fn aActionCompleted_CB);

private:
    friend BkLightingManager & BkLightingMgr(void);
    State_t mState;
    uint8_t mHue;
    uint8_t mSaturation;
    uint8_t mLevel;

    BkLightingCallback_fn mActionInitiated_CB;
    BkLightingCallback_fn mActionCompleted_CB;

    void Set(bool aOn);

    static BkLightingManager sLight;
};

inline BkLightingManager & BkLightingMgr(void)
{
    return BkLightingManager::sLight;
}
