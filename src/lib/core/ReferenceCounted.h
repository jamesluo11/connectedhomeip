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

/**
 * @file
 *   This file defines a C++ referenced counted object that auto deletes when
 *   all references to it have been removed.
 */

#pragma once

#include <atomic>
#include <limits>
#include <stdlib.h>

#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include "FreeRTOS.h"
#include "semphr.h"

namespace chip {

template <class T>
class DeleteDeletor
{
public:
    static void Release(T * obj) { chip::Platform::Delete(obj); }
};

template <class T>
class NoopDeletor
{
public:
    static void Release(T * obj) {}
};

/**
 * A reference counted object maintains a count of usages and when the usage
 * count drops to 0, it deletes itself.
 */
template <class Subclass, class Deletor = DeleteDeletor<Subclass>, int kInitRefCount = 1, typename CounterType = uint32_t>
class ReferenceCounted
{
public:
    using count_type = uint32_t;

    //=============Add by Beken Corporation Start====================//
    ReferenceCounted()
    {
        mRefCountMutex = xSemaphoreCreateMutex();
        
        if (mRefCountMutex == NULL)
        {
            os_printf("Failed to create CHIP mRefCountMutex \r\n");
        }
    }
    
    ~ReferenceCounted()
    {
        vSemaphoreDelete(mRefCountMutex);
    }
    
    void LockMrefcountChipStack()
    {
        xSemaphoreTake(mRefCountMutex, portMAX_DELAY);
    }
    
    void UnlockMrefcountChipStack()
    {
        xSemaphoreGive(mRefCountMutex);
    }
    //=============Add by Beken Corporation End====================//
    
    /** Adds one to the usage count of this class */
    
    Subclass * Retain()
    {
        VerifyOrDie(!kInitRefCount || GetReferenceCount() > 0);//Modified by Beken Corporation 
        VerifyOrDie(GetReferenceCount() < std::numeric_limits<count_type>::max());
        //++mRefCount;
        IncrementReferenceCount();//Modified by Beken Corporation 

        return static_cast<Subclass *>(this);
    }

    /** Release usage of this class */
    void Release()
    {
        VerifyOrDie(GetReferenceCount() != 0);
        //if (--mRefCount == 0)
        if (DecrementReferenceCount() == 0)//Modified by Beken Corporation 
        {
            Deletor::Release(static_cast<Subclass *>(this));
        }
    }

    /** Get the current reference counter value */
    //Modified by Beken Corporation 
    count_type GetReferenceCount()  { 
    count_type RefCount;
        LockMrefcountChipStack();
        RefCount = mRefCount;
        UnlockMrefcountChipStack();
        return RefCount; 
    }
    //=============Add by Beken Corporation Start====================//
    void IncrementReferenceCount() 
    {   
        LockMrefcountChipStack();
        ++mRefCount;
        UnlockMrefcountChipStack();
    }
        
    count_type DecrementReferenceCount() 
    {   
        LockMrefcountChipStack();
        --mRefCount;
        UnlockMrefcountChipStack();
        return mRefCount;
    }
    //=============Add by Beken Corporation End====================//

private:
    count_type mRefCount = kInitRefCount;
    SemaphoreHandle_t mRefCountMutex;//Add by Beken Corporation Start
};

} // namespace chip
