/*
 * Copyright (C) 2014 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include "vaapipicture.h"

#include "common/log.h"
#include "VaapiBuffer.h"
#include "vaapidisplay.h"
#include "vaapicontext.h"
#include "VaapiSurface.h"
#include "VaapiUtils.h"

namespace YamiMediaCodec{

extern uint8_t slice_dt[10 * 1024 * 1024];
extern uint32_t data_index;
//extern uint32_t slice_dt_size;
extern VASliceParameterBufferHEVC sliceArray[MAX_SLICE_NUM];
extern int32_t slice_index;

VaapiPicture::VaapiPicture(const ContextPtr& context,
    const SurfacePtr& surface, int64_t timeStamp)
    : m_display(context->getDisplay())
    , m_context(context)
    , m_surface(surface)
    , m_timeStamp(timeStamp)
{

}

VaapiPicture::VaapiPicture()
    : m_timeStamp(0)
{
}

bool VaapiPicture::render()
{
    if (m_surface->getID() == VA_INVALID_SURFACE) {
        ERROR("bug: no surface to encode");
        return false;
    }

    VAStatus status;
    status = vaBeginPicture(m_display->getID(), m_context->getID(), m_surface->getID());
    if (!checkVaapiStatus(status, "vaBeginPicture()"))
        return false;

    bool ret = doRender();

    status = vaEndPicture(m_display->getID(), m_context->getID());
    if (!checkVaapiStatus(status, "vaEndPicture()"))
        return false;
    return ret;
}

bool VaapiPicture::render_265_slice()
{
    VAStatus vaSts = VA_STATUS_SUCCESS;
    VABufferID sliceParamBufferId[MAX_SLICE_NUM];
    VABufferID sliceDataID = VA_INVALID_ID;
    for(int i = 0; i < MAX_SLICE_NUM; i++){
        sliceParamBufferId[i] = VA_INVALID_ID;
    }
    vaSts = vaCreateBuffer(m_display->getID(),
        m_context->getID(),
        VASliceParameterBufferType,
        sizeof(VASliceParameterBufferHEVC),
        slice_index, //because slice_index has increased.
        sliceArray,
        sliceParamBufferId);
    
    //printf("dpwu  %s %s %d, slice_dt_size = %d, slice_index = %d ====\n", __FILE__, __FUNCTION__, __LINE__, slice_dt_size, slice_index);
#if (0)    
    for(int32_t i = 0; i < slice_index; i++){
        printf("dpwu  %s %s %d, sliceParamBufferId[%d] = 0x%x,  ====\n", __FILE__, __FUNCTION__, __LINE__, i, sliceParamBufferId[i]);
    }
#endif

    if (!checkVaapiStatus(vaSts, "vaCreateBuffer")){
        return false;
    }
    vaSts = vaCreateBuffer(m_display->getID(), m_context->getID(),
        VASliceDataBufferType, data_index, 1, slice_dt, &sliceDataID);
    if (!checkVaapiStatus(vaSts, "vaCreateBuffer")){
        return false;
    }

    vaSts = vaRenderPicture(m_display->getID(), m_context->getID(), sliceParamBufferId, 1);
    if (!checkVaapiStatus(vaSts, "vaRenderPicture failed")){
        return false;
    }
    vaSts = vaRenderPicture(m_display->getID(), m_context->getID(), &sliceDataID, 1);
    if (!checkVaapiStatus(vaSts, "vaRenderPicture failed")){
        return false;
    }

    for(int32_t i = 0; i < slice_index; i++){
        //printf("dpwu  %s %s %d, sliceParamBufferId[%d] = 0x%x,  ====\n", __FILE__, __FUNCTION__, __LINE__, i, sliceParamBufferId[i]);
        if(sliceParamBufferId[i] != VA_INVALID_ID)
            checkVaapiStatus(vaDestroyBuffer(m_display->getID(), sliceParamBufferId[i]), "vaDestroyBuffer");
    }
    checkVaapiStatus(vaDestroyBuffer(m_display->getID(), sliceDataID), "vaDestroyBuffer");
    slice_index = 0;
    data_index = 0;
    //slice_dt_size = 0;

    return true;
}


bool VaapiPicture::render(BufObjectPtr& buffer)
{
    VAStatus status = VA_STATUS_SUCCESS;
    VABufferID bufferID = VA_INVALID_ID;

    if (!buffer)
        return true;

    buffer->unmap();

    bufferID = buffer->getID();
    if (bufferID == VA_INVALID_ID)
        return false;

    status = vaRenderPicture(m_display->getID(), m_context->getID(), &bufferID, 1);
    if (!checkVaapiStatus(status, "vaRenderPicture failed"))
        return false;

    buffer.reset();             // silently work  arouond for psb
    return true;
}

bool VaapiPicture::render(std::pair <BufObjectPtr,BufObjectPtr> &paramAndData)
{
    return render(paramAndData.first) && render(paramAndData.second);
}

bool VaapiPicture::addObject(std::vector<std::pair<BufObjectPtr,BufObjectPtr> >& objects,
                             const BufObjectPtr & param,
                             const BufObjectPtr & data)
{
    if (!param || !data)
        return false;
    objects.push_back(std::make_pair(param, data));
    return true;
}

bool VaapiPicture::addObject(std::vector < BufObjectPtr >& objects,
                             const BufObjectPtr& object)
{
    if (!object)
        return false;
    objects.push_back(object);
    return true;
}

bool VaapiPicture::sync()
{
    return vaSyncSurface(m_display->getID(), getSurfaceID()) == VA_STATUS_SUCCESS;
}
}
